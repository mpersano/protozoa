#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <GL/gl.h>

#include "font.h"
#include "font_render.h"
#include "arena.h"
#include "ship.h"
#include "settings.h"
#include "panic.h"
#include "common.h"
#include "gl_util.h"
#include "in_game.h"
#include "in_game_text.h"

enum {
	MAX_IN_GAME_TEXTS = 64
};

struct in_game_text {
	int used;
	vector2 pos;
	int nlines;
	char text[3][30];
	int width;
	struct rgba color;
	float scale;
	int ttl;
	int t;
};

static struct in_game_text in_game_texts[MAX_IN_GAME_TEXTS];

void
reset_in_game_texts(void)
{
	int i;

	for (i = 0; i < MAX_IN_GAME_TEXTS; i++)
		in_game_texts[i].used = 0;
}

void
add_in_game_text(const vector2 *pos, const struct rgba *color,
  float scale, int ttl, const char *fmt, ...)
{
	struct in_game_text *p;
	int i;

	p = NULL;
	
	for (i = 0; i < MAX_IN_GAME_TEXTS; i++) {
		if (!in_game_texts[i].used) {
			p = &in_game_texts[i];
			break;
		}
	}

	if (p) {
		char text[200];
		char *q;
		va_list ap;

		p->used = 1;
		p->pos = *pos;

		va_start(ap, fmt);
		vsnprintf(text, sizeof p->text, fmt, ap);
		va_end(ap);

		p->nlines = 0;
		p->width = 0;

		for (q = strtok(text, "\n"); q; q = strtok(NULL, "\n")) {
			int w;
			char *r = p->text[p->nlines++];

			strncpy(r, q, sizeof p->text[0] - 1);

			w = string_width_in_pixels(font_medium, r);

			if (w > p->width)
				p->width = w;
		}

		p->color = *color;
		p->scale = scale;
		p->ttl = ttl;
		p->t = 0;
	}
}

#define LINE_HEIGHT 20

static void
draw_in_game_text(const struct in_game_text *p)
{
	/* const struct text_settings *text_settings; */
	const struct rgba *color;
	float scale;
	float s, t, v;
	const struct font_render_info *font = font_medium;
	int i;

	/* text_settings = powerup_settings[p->type]->text; */
	color = &p->color;
	scale = p->scale;

	s = 1.f - (float)p->t/p->ttl;
	t = scale*(.01f + s*.01f);

	v = scale*t*(-gc.eye.z/BASE_EYE_Z);

	glColor4f(color->r, color->g, color->b, s*color->a);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glTranslatef(p->pos.x, p->pos.y, 0.f);

	glScalef(v, v, v);

	glTranslatef(-.5f*p->width, -.5f*LINE_HEIGHT*p->nlines, 0);
	glScalef(1, -1, 1);

	for (i = 0; i < p->nlines; i++) {
		const char *str = p->text[i];
		int w = string_width_in_pixels(font, str);
		render_string(font, .5f*(p->width - w), 0, 1, str);
		glTranslatef(0, LINE_HEIGHT, 0);
	}

	glPopMatrix();
}

static void
update_in_game_text(struct in_game_text *p)
{
	if (++p->t >= p->ttl)
		p->used = 0;
}

void
draw_in_game_texts(void)
{
	struct in_game_text *p;

	for (p = in_game_texts; p != &in_game_texts[MAX_IN_GAME_TEXTS]; p++) {
		if (p->used)
			draw_in_game_text(p);
	}
}

void
update_in_game_texts(void)
{
	struct in_game_text *p;

	for (p = in_game_texts; p != &in_game_texts[MAX_IN_GAME_TEXTS]; p++) {
		if (p->used)
			update_in_game_text(p);
	}
}

void
initialize_in_game_texts(void)
{
}

void
serialize_in_game_texts(FILE *out)
{
	int count = 0;
	struct in_game_text *p;

	for (p = in_game_texts; p != &in_game_texts[MAX_IN_GAME_TEXTS]; p++) {
		if (p->used)
			++count;
	}

	fwrite(&count, sizeof count, 1, out);

	if (count) {
		for (p = in_game_texts; p != &in_game_texts[MAX_IN_GAME_TEXTS]; p++) {
			if (p->used)
				fwrite(p, sizeof *p, 1, out);
		}
	}
}

int
unserialize_in_game_texts(const char *data)
{
	int count;
	struct in_game_text *p;

	memcpy(&count, data, sizeof count);
	data += sizeof count;

	assert(count < MAX_IN_GAME_TEXTS);

	if (count) 
		memcpy(in_game_texts, data, count*sizeof *in_game_texts);

	for (p = &in_game_texts[count]; p != &in_game_texts[MAX_IN_GAME_TEXTS]; p++)
		p->used = 0;

	return sizeof count + count*sizeof *in_game_texts;
}
