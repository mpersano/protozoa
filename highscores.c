#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <GL/gl.h>

#include "common.h"
#include "panic.h"
#include "font_render.h"
#include "highscores.h"

enum {
	NUM_HIGHSCORES = 10,
	LOG10_MAX_SCORE = 6
};

static struct score_rank_map {
	int score_limit;
	const char *rank;
} score_rank_map[] = {
	{ 3*3000, "Patzer" },
	{ 3*7500, "Novice" },
	{ 3*15000, "Average" },
	{ 3*30000, "Good" },
	{ 3*45000, "Pro" },
	{ 3*75000, "Elite" },
	{ 3*150000, "*GOD*" },
	{ -1, "Bot" },
};

struct highscore {
	int score;
	int level;
	int wave;
	char name[MAX_NAME_LEN + 1];
};

int last_highscore = -1;

struct highscore highscore_table[NUM_HIGHSCORES];

static int digit_width[10];

static struct font_render_info *fri;

static const char *HIGHSCORE_FILE = "highscores.txt";

float cpoints[][3] = {
	{ 1, .2, .2 },	/* red */
	{ .2, .2, 1 },	/* blue */
};

static void
interpolate_color(float rgb[3], float t)
{
	const int npoints = sizeof cpoints/sizeof *cpoints;

	float u = t*(npoints - 1);

	int p = floor(u);
	int q = ceil(u);

	/* cosine interpolation */
	float mu = u - p;
	float mu2 = .5f*(1.f - cos(mu*M_PI));

	if (mu2 > 1)
		mu2 = 1;
	else if (mu2 < 0)
		mu2 = 0;

	rgb[0] = cpoints[p][0]*(1 - mu2) + cpoints[q][0]*mu2;
	rgb[1] = cpoints[p][1]*(1 - mu2) + cpoints[q][1]*mu2;
	rgb[2] = cpoints[p][2]*(1 - mu2) + cpoints[q][2]*mu2;
}

const struct rank *
score_to_rank(int score)
{
	static struct rank rank;
	struct score_rank_map *p;

	for (p = score_rank_map; p->score_limit; p++) {
		if (score < p->score_limit || p->score_limit == -1) {
			float t = (float)score/80000;

			if (t > 1.f)
				t = 1.f;

			rank.description = p->rank;
			interpolate_color(rank.color, 1.f - t);

			return &rank;
		}
	}

	assert(0);
	return NULL;
}

static int
load_highscores(void)
{
	int i;
	FILE *in;

	if ((in = fopen(HIGHSCORE_FILE, "r")) == NULL)
		return 0;

	for (i = 0; i < NUM_HIGHSCORES; i++) {
		int score = 0, level = 0, wave = 0;
		char *name, *p, *end;
		static char line[256];

		if (fgets(line, sizeof line, in) == NULL)
			panic("error reading highscore file");

		name = strtok(line, " ");
		if (strlen(name) > MAX_NAME_LEN)
			panic("error reading highscore file");

#define PARSE_INTEGER(n) \
		p = strtok(NULL, " "); \
		if (p == NULL || ((n = strtol(p, &end, 10)), end == p)) \
			panic("error reading highscore file");

		PARSE_INTEGER(level)
		PARSE_INTEGER(wave)
		PARSE_INTEGER(score)

		strncpy(highscore_table[i].name, name, sizeof highscore_table[i].name - 1);
		highscore_table[i].level = level;
		highscore_table[i].wave = wave;
		highscore_table[i].score = score;
	}

	fclose(in);

	return 1;
}

void
initialize_highscores(void)
{
	int i;

	if (!load_highscores()) {
		int score;

		const char *names[NUM_HIGHSCORES] = {
			"M*C", "CRI", "RCC", "ABA", "RMS",
			"ZED", "M*C", "SPC", "B-I", "LIN",
		};

		score = 100000;

		for (i = 0; i < NUM_HIGHSCORES; i++) {
			int w = NUM_HIGHSCORES - 1 - i;

			strncpy(highscore_table[i].name, names[i],
					sizeof highscore_table[i].name - 1);
			highscore_table[i].score = score;
			highscore_table[i].level = 1 + w/7;;
			highscore_table[i].wave = 1 + w%7; /* FIXME */

			score -= (NUM_HIGHSCORES - i)*(NUM_HIGHSCORES - i)*250;
		}
	}

	fri = font_small;

	for (i = 0; i < 10; i++) {
		char s[2] = { i + '0', '\0' };
		digit_width[i] = string_width_in_pixels(fri, s);
	}
}

static void
draw_fixed_width_number(int num, int zero_pad, int ndigits)
{
	int i;
	
	for (i = 0; i < ndigits; i++) {
		int d = num%10;
		int x = digit_width[0]*(ndigits - 1 - i) +
		  (digit_width[0] - digit_width[d])/2;

	        render_string(fri, x, 0, 1, "%d", d);

		num /= 10;

		if (!num && !zero_pad)
			break;
	}
}

static void
draw_right_justified(int x, int y, const char *fmt, ...)
{
	static char str[80];
	int width;
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(str, sizeof str, fmt, ap);
	va_end(ap);

	width = string_width_in_pixels(fri, str);

	glPushMatrix();
	glTranslatef(x - width, y, 0);
	render_string(fri, 0, 0, 1, str);
	glPopMatrix();
}

static void
draw_centered(int x0, int x1, int y, const char *fmt, ...)
{
	static char str[80];
	int width;
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(str, sizeof str, fmt, ap);
	va_end(ap);

	width = string_width_in_pixels(fri, "%s", str);

	glPushMatrix();
	glTranslatef(.5f*(x0 + x1 - width), y, 0);
	render_string(fri, 0, 0, 1, "%s", str);
	glPopMatrix();
}

static void
draw_score(int x, int y, int score)
{
	extern int digit_width[];
	extern void draw_fixed_width_number(int num, int zero_pad, int ndigits);

	glPushMatrix();
	glTranslatef(x - LOG10_MAX_SCORE*digit_width[0], y, 0);
	draw_fixed_width_number(score, 0, LOG10_MAX_SCORE);
	glPopMatrix();
}

void
draw_highscore_table(float base_x, float base_y, float spread)
{
	int i, t;
	const int item_height = 3*fri->char_height/2;
	const int viewport_width = 800;
	const int viewport_height = 600;
	static const int column_limit[] = {
		.5*80,		/* ordinal */
		.5*280,		/* name */
		.5*460,		/* score */
		.5*640,		/* wave */
		.5*800,		/* rank */
	};
	static const char *ordinal[] = {
		"1ST", "2ND", "3RD", "4TH", "5TH",
		"6TH", "7TH", "8TH", "9TH", "10TH" };
	float y;
	const float SPREAD_FUDGE = 25;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.f, viewport_width, viewport_height, 0.f, -1.f, 1.f);

	glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

	glColor3f(.8, .8, 1.);

#define CALC_Y(n) \
	y = base_y + (n)*item_height - spread*SPREAD_FUDGE*((NUM_HIGHSCORES + 3) - (n));

	CALC_Y(0)

	draw_centered(base_x, base_x + column_limit[4],
	  y, "* * *   H A L L   O F   F A M E   * * *");

	CALC_Y(2)

	draw_centered(base_x + column_limit[0], base_x + column_limit[1],
	  y, "NAME");
	draw_centered(base_x + column_limit[1], base_x + column_limit[2],
	  y, "SCORE");
	draw_centered(base_x + column_limit[2], base_x + column_limit[3],
	  y, "LEVEL");
	draw_centered(base_x + column_limit[3], base_x + column_limit[4],
	  y, "RANK");

	t = get_cur_ticks(); /* the purpose of which will be apparent momentarily. */

	for (i = 0; i < NUM_HIGHSCORES; i++) {
		const struct rank *r = score_to_rank(highscore_table[i].score);

		CALC_Y(i + 3)

		if (i == last_highscore) {
			float s = .5f + .5f*sin(.2f*t); /* ... there you go. */
			glColor3f(s*.6 + (1.f - s), s*.6 + (1.f - s), 1.);
		} else {
			glColor3f(.6, .6, 1.);
		}

		draw_right_justified(base_x + column_limit[0], y, "%s",
		  ordinal[i]);

		/* name */
		draw_centered(base_x + column_limit[0],
		  base_x + column_limit[1], y, "%s",
		  highscore_table[i].name);

		/* score */
		draw_score(base_x + column_limit[2], y,
		  highscore_table[i].score);

		/* wave */
		draw_centered(base_x + column_limit[2],
		  base_x + column_limit[3], y, "%d/%d",
		  highscore_table[i].level,
		  highscore_table[i].wave);

		/* rank */
		glColor3f(r->color[0], r->color[1], r->color[2]);
		draw_centered(base_x + column_limit[3],
		  base_x + column_limit[4], y, r->description);

		y += item_height;
	}

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);

	glPopAttrib();
}

int
is_highscore(int score)
{
	return score > highscore_table[NUM_HIGHSCORES - 1].score;
}

void
highscores_add(const char *name, int score, int level, int wave)
{
	if (is_highscore(score)) {
		int i;

		for (i = NUM_HIGHSCORES - 2; i >= 0; i--) {
			if (score < highscore_table[i].score) {
				break;
			}
		}

		i++;

		if (i < NUM_HIGHSCORES - 1) {
			memmove(&highscore_table[i + 1], &highscore_table[i],
			  (NUM_HIGHSCORES - i - 1)*sizeof *highscore_table);
		}

		memcpy(highscore_table[i].name, name, MAX_NAME_LEN);
		highscore_table[i].score = score;
		highscore_table[i].level = level;
		highscore_table[i].wave = wave;

		last_highscore = i;
	}
}

void
save_highscores(void)
{
	FILE *out;
	int i;

	if ((out = fopen(HIGHSCORE_FILE, "w")) == NULL)
		panic("failed to open %s: %s\n", HIGHSCORE_FILE, strerror(errno));

	for (i = 0; i < NUM_HIGHSCORES; i++)
		fprintf(out, "%s %d %d %d\n", highscore_table[i].name,
		  highscore_table[i].level, highscore_table[i].wave,
		  highscore_table[i].score);

	fclose(out);
}
