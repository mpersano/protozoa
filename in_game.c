/*
 * "But Alligator was not digging the bottom of the hole
 *  Which was to be his grave,
 *  But rather he was digging his own hole
 *  As a shelter for himself"
 *	- the Popol Vuh
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL.h>
#include <SDL_keysym.h>

#include "panic.h"
#include "common.h"
#include "audio.h"
#include "settings.h"
#include "arena.h"
#include "ship.h"
#include "missile.h"
#include "bomb.h"
#include "laser.h"
#include "foe.h"
#include "particle.h"
#include "explosion.h"
#include "powerup.h"
#include "in_game_text.h"
#include "font.h"
#include "font_render.h"
#include "water.h"
#include "background.h"
#include "highscores.h"
#include "gl_util.h"
#include "in_game.h"

enum {
	TICS_UNTIL_RESPAWN = 100,
	LOG10_MAX_SCORE = 6,
	MAX_WAVES = 20,
	PRE_WAVE_CLEARED_TICS = 50,
	WAVE_CLEARED_TICS = 70,
	MULTIPLIER_MESSAGE_TTL = 80,
	MULTIPLIER_CHANGE_TICS = 800,
	GAME_OVER_FADE_IN_TICS = 60,
	LEVEL_CLEARED_FADE_IN_TICS = 60,
	LEVEL_CLEARED_FADE_OUT_TICS = 60,
	LEVEL_TRANSITION_TOTAL_TICS = 80,
	RANK_TOTAL_TICS = 150,
	MAX_MULTIPLIER = 5,
	MAX_COMBO_TIC_INTERVAL = 3,
	MIN_COMBO_KILLS = 2,
	COMBO_MESSAGE_TTL = 60,
	TIME_BONUS_MIN_TICS = 30*FPS,
	TIME_BONUS_PER_TIC = 5, /* tee-hee. */
};

struct game_context gc;

struct stat_counters level_stat_counters;
struct stat_counters game_stat_counters;

struct level *levels[NUM_LEVELS];

struct inner_state inner_state;

static int digit_width[10];
static int ships_text_width;
static struct vector3 delta_eye;
static int last_score_tic;
static int cur_combo;

static void reset_wave(void);
static void reset_level(void);

static int stat_tic_counter;

static int serializing;
static FILE *serialization_out_stream;

static const int viewport_width = 800;
static const int viewport_height = 600;

static int crosshair_arrow_texture_id;

#define SCORE_TEXT_TTL 15

static void
start_serialization(void)
{
	static int id = 0;
	char file[80];

	sprintf(file, "dump-%d.bin", id++);

	serializing = 1;

	if ((serialization_out_stream = fopen(file, "w")) == NULL)
		panic("fopen failed: %s", strerror(errno));
}

static void
end_serialization(void)
{
	serializing = 0;

	assert(serialization_out_stream != NULL);
	fclose(serialization_out_stream);
}

static void
serialize_game_state(FILE *out)
{
	fwrite(&gc, sizeof gc, 1, out);
}

static int
unserialize_game_state(const char *data)
{
	memcpy(&gc, data, sizeof gc);
	cur_arena = levels[gc.cur_level]->waves[gc.cur_wave]->arena;
	inner_state.state = IS_IN_GAME;
	return sizeof(gc);
}

static void
serialize(void)
{
	FILE *out = serialization_out_stream;

	assert(inner_state.state == IS_IN_GAME);

	serialize_game_state(out);
	serialize_ship(out);
	serialize_explosions(out);
	serialize_particles(out);
	serialize_missiles(out);
	serialize_foes(out);
	serialize_background(out);
	serialize_water(out);
	serialize_in_game_texts(out);
}

int
unserialize(const char *data)
{
	const char *p = data;
#ifdef DEBUG_SERIALIZE
	const char *q;
#endif

	p += unserialize_game_state(p);
#ifdef DEBUG_SERIALIZE
	printf("  game state: %d\n", p - data);
	q = p;
#endif

	p += unserialize_ship(p);
#ifdef DEBUG_SERIALIZE
	printf("  ship: %d\n", p - q);
	q = p;
#endif

	p += unserialize_explosions(p);
#ifdef DEBUG_SERIALIZE
	printf("  explosions: %d\n", p - q);
	q = p;
#endif

	p += unserialize_particles(p);
#ifdef DEBUG_SERIALIZE
	printf("  particles: %d\n", p - q);
	q = p;
#endif

	p += unserialize_missiles(p);
#ifdef DEBUG_SERIALIZE
	printf("  missiles: %d\n", p - q);
	q = p;
#endif

	p += unserialize_foes(p);
#ifdef DEBUG_SERIALIZE
	printf("  foes: %d\n", p - q);
	q = p;
#endif

	p += unserialize_background(p);
#ifdef DEBUG_SERIALIZE
	printf("  background: %d\n", p - q);
	q = p;
#endif

	p += unserialize_water(p);
#ifdef DEBUG_SERIALIZE
	printf("  water: %d\n", p - q);
	q = p;
#endif

	p += unserialize_in_game_texts(p);
#ifdef DEBUG_SERIALIZE
	printf("  texts: %d\n", p - q);
	q = p;

	printf("total: %d\n", p - data);
#endif

	return p - data;
}

void
bump_score(const vector2 *pos, int points)
{
	if (inner_state.state == IS_IN_GAME) {
		static const struct rgba color = { 1, 1, 1, 1 };
		float scale = 1.f + .0015f*points;
		points *= gc.multiplier;
		add_in_game_text(pos, &color, scale, SCORE_TEXT_TTL, "%d", points);
		gc.score += points;

		if (last_score_tic == -1 ||
		  inner_state.tics - last_score_tic <= MAX_COMBO_TIC_INTERVAL)
			++cur_combo;

		last_score_tic = inner_state.tics;
	}
}

static void
set_inner_state(int state)
{
	inner_state.state = state;
	inner_state.tics = 0;
}

static void
spawn_new_ship(void)
{
	--gc.ships_left;
	reset_ship();
	gc.multiplier = 1;
	gc.multiplier_tic_count = 0;
	last_score_tic = -1;
	cur_combo = 0;
	gc.combo_message_tics = -1;
}

static void initialize_stats_table(struct stat_counters *counters);

static void
trigger_game_over(void)
{
	fade_music();
	initialize_stats_table(&game_stat_counters);
	set_inner_state(IS_GAME_OVER);
}

static void
update_inner_state(void)
{
	static int prev_level_tics;

	inner_state.tics++;

	switch (inner_state.state) {
		case IS_WAVE_TITLE:
			if (inner_state.tics >= WAVE_TITLE_TICS) {
				set_inner_state(IS_IN_GAME);
				/* set_inner_state(IS_PRE_WAVE_CLEARED); */
			} else {
				if (inner_state.tics == WAVE_TITLE_TICS/2) {
					if (!ship.is_alive)
						spawn_new_ship();
					else
						reset_ship();
				}
			}
			break;

		case IS_IN_GAME:
			level_stat_counters.tics++;
			game_stat_counters.tics++;

			if (gc.tics_remaining <= 0) {
				/* boom. */
				hit_ship(&ship.pos, 1.f);
				trigger_game_over();
			} else {
				gc.tics_remaining--;

				if (!ship.is_alive) {
					prev_level_tics = inner_state.tics;
					set_inner_state(IS_RESPAWNING_SHIP);
				} else if (!gc.foes_left) {
					level_stat_counters.waves++;
					game_stat_counters.waves++;
					if (gc.tics_remaining > TIME_BONUS_MIN_TICS)
						gc.score += gc.tics_remaining*TIME_BONUS_PER_TIC;
					set_inner_state(IS_PRE_WAVE_CLEARED);
				} else {
					spawn_new_foes();
				}
			}
			break;

		case IS_PRE_WAVE_CLEARED:
			if (inner_state.tics >= PRE_WAVE_CLEARED_TICS) {
				gen_ship_implosion();
				reset_powerups();
				reset_missiles();
				reset_bombs();
				reset_lasers();
				vec2_set(&ship.pos, 0.f, 0.f); /* HACK */
				play_fx(FX_WAVE_TRANSITION);
				set_inner_state(IS_WAVE_CLEARED);
			}
			break;

		case IS_RESPAWNING_SHIP:
			if (inner_state.tics >= TICS_UNTIL_RESPAWN) {
				if (gc.ships_left) {
					spawn_new_ship();
					set_inner_state(IS_IN_GAME);
					inner_state.tics = prev_level_tics;
				} else {
					trigger_game_over();
				}
			}
			break;

		case IS_GAME_OVER:
			break;

		case IS_RANK:
			if (inner_state.tics >= RANK_TOTAL_TICS) {
				stop_music();

				if (is_highscore(gc.score)) {
					set_inner_state(IS_HIGHSCORE_INPUT);
					SDL_ShowCursor(SDL_ENABLE);
					start_highscore_input();
				} else {
					/* loser. */
					SDL_ShowCursor(SDL_ENABLE);
					start_main_menu();
				}
			}
			break;

		case IS_WAVE_CLEARED:
			if (gc.cur_wave == levels[gc.cur_level]->num_waves - 1 &&
				inner_state.tics >= WAVE_CLEARED_TICS/2) {
				initialize_stats_table(&level_stat_counters);
				set_inner_state(IS_LEVEL_CLEARED);
			} else if (inner_state.tics >= WAVE_CLEARED_TICS) {
				gc.cur_wave++;
				assert(gc.cur_wave < levels[gc.cur_level]->num_waves);
				reset_wave();
				set_inner_state(IS_WAVE_TITLE);
			}
			break;

		case IS_HIGHSCORE_INPUT:
			break;

		case IS_LEVEL_CLEARED:
			break;

		case IS_LEVEL_TRANSITION:
			if (inner_state.tics >= LEVEL_TRANSITION_TOTAL_TICS) {
				gc.cur_level = (gc.cur_level + 1)%NUM_LEVELS;
				set_inner_state(IS_WAVE_TITLE);
				reset_level();
			}
			break;

		default:
			assert(0);
	}
}

struct game_over_stats {
	int time;		/* in seconds */
	int accuracy;		/* 0-100% */
	int waves;		/* waves completed */
	int secrets;		/* secrets discovered */

	int display_time;
	int display_accuracy;
	int display_waves;
	int display_secrets;

	int bonus;		/* bonus */
};

static struct game_over_stats game_over_stats;

static void
initialize_stats_table(struct stat_counters *counters)
{
	game_over_stats.time = counters->tics/FPS;

	if (counters->missiles_fired == 0) {
		/* wtf? show some spirit! */
		game_over_stats.accuracy = 0;
	} else {
		game_over_stats.accuracy =
		  100*(counters->missiles_fired - counters->missiles_missed)/counters->missiles_fired;
	}

	game_over_stats.waves = counters->waves;
	game_over_stats.secrets = counters->combo_count;

	game_over_stats.display_time = game_over_stats.display_accuracy =
	game_over_stats.display_waves = game_over_stats.display_secrets = 0;

	game_over_stats.bonus = 0;
	stat_tic_counter = 0;
}

struct game_over_stat_bonus {
	int *count;
	int *display_count;
	int tics_per_update;
	int bonus;
} game_over_stat_bonus[] = {
	{ &game_over_stats.time, &game_over_stats.display_time, 1, 5 },
	{ &game_over_stats.accuracy, &game_over_stats.display_accuracy, 2, 30 },
	{ &game_over_stats.secrets, &game_over_stats.display_secrets, 4, 50 },
	{ &game_over_stats.waves, &game_over_stats.display_waves, 8, 500 },
	{ NULL, 0, 0 },
};

static int
update_stats_table(void)
{
	struct game_over_stat_bonus *p;

	++stat_tic_counter;

	for (p = game_over_stat_bonus; p->count; p++) {
		if (*p->display_count < *p->count)
			break;
	}

	if (p->count) {
		if (stat_tic_counter >= p->tics_per_update) {
			stat_tic_counter = 0;
			game_over_stats.bonus += p->bonus;
			(*p->display_count)++;
			play_fx(FX_SHOT_1);
		}

		return 1;
	} else {
		/*
		   gc.score += game_over_stats.bonus;
		   set_inner_state(IS_RANK);
		   */
		return 0;
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

	        render_string(font_medium, x, 0, 1, "%d", d);

		num /= 10;

		if (!num && !zero_pad)
			break;
	}
}

static void
draw_time(void)
{
	const char *secs = "''";
	const char *mins = "'";

	glTranslatef(-string_width_in_pixels(font_medium, secs), 0, 0);
	render_string(font_medium, 0, 0, 1, secs);

	glTranslatef(-2*digit_width[0], 0, 0);
	draw_fixed_width_number(game_over_stats.display_time%60, 1, 2);

	glTranslatef(-string_width_in_pixels(font_medium, mins), 0, 0);
	render_string(font_medium, 0, 0, 1, mins);

	glTranslatef(-2*digit_width[0], 0, 0);
	draw_fixed_width_number(game_over_stats.display_time/60, 0, 2);
}

static void
draw_accuracy(void)
{
	glTranslatef(-string_width_in_pixels(font_medium, "%c", '%'), 0, 0);
	render_string(font_medium, 0, 0, 1, "%c", '%');

	glTranslatef(-3*digit_width[0], 0, 0);
	draw_fixed_width_number(game_over_stats.display_accuracy, 0, 3);
}

static void
draw_waves(void)
{
	glTranslatef(-3*digit_width[0], 0, 0);
	draw_fixed_width_number(game_over_stats.display_waves, 0, 3);
}

static void
draw_secrets(void)
{
	glTranslatef(-3*digit_width[0], 0, 0);
	draw_fixed_width_number(game_over_stats.display_secrets, 0, 3);
}

static void
draw_bonus(void)
{
	glTranslatef(-6*digit_width[0], 0, 0);
	draw_fixed_width_number(game_over_stats.bonus, 0, 6);
}

static void
draw_rank(void)
{
	if (inner_state.state == IS_RANK) {
		const struct rank *p;
		float s;

		s = inner_state.tics < .5f*RANK_TOTAL_TICS ?
			(float)inner_state.tics/(.5f*RANK_TOTAL_TICS) : 1.f;

		p = score_to_rank(gc.score);

		glColor4f(s*p->color[0], s*p->color[1], s*p->color[2], s*1.);

		glTranslatef(-string_width_in_pixels(font_medium, p->description), 0,
		  0);
		render_string(font_medium, 0, 0, 1, p->description);
	}
}

struct rank_screen_label {
	const char *label_text;
	void(*draw_value)(void);
};

static void
draw_stats_table(const struct rank_screen_label *labels, int num_labels,
  const char *table_title, float fade_in)
{
	const struct rank_screen_label *p;
	const struct font_render_info *fri = font_medium;
	const int item_height = 3*fri->char_height/2;
	const float table_title_scale = 1.4;
	int max_value_width;
	int y;

	max_value_width = string_width_in_pixels(fri, "000000");

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	y = .5*(viewport_height - (num_labels + 2)*item_height);

	/* game over text */

	glColor4f(fade_in*.2, fade_in*.2, fade_in*1., fade_in*1.);

	glPushMatrix();
	glTranslatef(.5f*viewport_width, y, 0);
	glScalef(table_title_scale, table_title_scale, table_title_scale);
	glTranslatef(-.5*string_width_in_pixels(fri, table_title), 0, 0);
	render_string(fri, 0, 0, 1, table_title);
	glPopMatrix();

	y += 2*item_height;

	/* labels */

	for (p = labels; p != &labels[num_labels]; p++) {
		if (p->label_text) {
			int label_width = string_width_in_pixels(fri,
			  p->label_text);

			glColor4f(fade_in*1., fade_in*.6, fade_in*.2, fade_in*1.);

			glPushMatrix();
			glTranslatef(.5f*viewport_width - label_width - 5, y,
			  0);
			render_string(fri, 0, 0, 1, p->label_text);
			glPopMatrix();

			glColor4f(fade_in*1., fade_in*.8, fade_in*.4, fade_in*1.);

			glPushMatrix();
			glTranslatef(.5f*viewport_width + 5 + 6*digit_width[0],
			  y, 0);
			p->draw_value();
			glPopMatrix();
		}

		y += item_height;
	}

	glPopMatrix();
}

static void
draw_game_over(void)
{
	/*-----------------------------
	 *
	 *        Game Over
	 *
	 *       Time: 03m42s
	 *   Accuracy:    72%
	 *      Waves:     12
	 *    Secrets:      8
	 *
	 *      Bonus:  91234
	 *
	 *       Rank: Novice
	 */
	static const struct rank_screen_label labels[] = {
		{ "TIME:", draw_time },
		{ "ACCURACY:", draw_accuracy },
		{ "COMBOS:", draw_secrets },
		{ "WAVES:", draw_waves },
		{ NULL, NULL },
		{ "BONUS:", draw_bonus },
		{ NULL, NULL },
		{ "RANK:", draw_rank },
	};
	float fade_in;

	if (inner_state.state == IS_GAME_OVER) {
		fade_in = inner_state.tics < GAME_OVER_FADE_IN_TICS ?
			(float)inner_state.tics/GAME_OVER_FADE_IN_TICS : 1.f;
	} else {
		fade_in = 1;
	}

	draw_stats_table(labels, 8, "GAME OVER", fade_in);
}

static void
draw_level_cleared(void)
{
	static const struct rank_screen_label labels[] = {
		{ "TIME:", draw_time },
		{ "ACCURACY:", draw_accuracy },
		{ "COMBOS:", draw_secrets },
		{ "WAVES:", draw_waves },
		{ NULL, NULL },
		{ "BONUS:", draw_bonus },
	};
	char str[80];
	float fade_in;

	sprintf(str, "LEVEL %d CLEARED", gc.cur_level + 1);

	if (inner_state.state == IS_LEVEL_CLEARED) {
		fade_in = inner_state.tics < LEVEL_CLEARED_FADE_IN_TICS ?
			(float)inner_state.tics/LEVEL_CLEARED_FADE_IN_TICS : 1.f;
	} else {
		fade_in = inner_state.tics < LEVEL_CLEARED_FADE_OUT_TICS ?
		  1.f - (float)inner_state.tics/LEVEL_CLEARED_FADE_OUT_TICS : 0.f;
	}

	draw_stats_table(labels, 6, str, fade_in);
}

static void
draw_serializing(void)
{
	const char *str = "SERIALIZING";
	int w;

	w = string_width_in_pixels(font_big, str);

	glPushMatrix();
	glLoadIdentity();
	glTranslatef(.5f*(viewport_width - w), .5*viewport_height, 0.f);
	glColor4f(1, 0, 0, 1);
	render_string(font_big, 0, 0, 1, str);

	glPopMatrix();
}

static void
draw_wave_cleared(void)
{
	const int t = inner_state.tics;
	const struct font_render_info *font = font_medium;
	static char str[80];
	float s, f;
	int w, e;

	switch (inner_state.state) {
		case IS_PRE_WAVE_CLEARED:
			s = (float)t/PRE_WAVE_CLEARED_TICS;
			e = inner_state.tics;
			break;

		case IS_WAVE_CLEARED:
#if 0
			if (inner_state.tics < WAVE_CLEARED_TICS/2)
				s = 1.f;
			else
				s = 1.f - (float)(inner_state.tics -
				  WAVE_CLEARED_TICS/2)/(WAVE_CLEARED_TICS/2);
#else
			if (inner_state.tics < WAVE_CLEARED_TICS/2)
				s = 1.f - (float)inner_state.tics/
				  (WAVE_CLEARED_TICS/2);
			else
				s = 0.f;
#endif
			e = PRE_WAVE_CLEARED_TICS + inner_state.tics;
			break;

		default:
			assert(0);
	}

	f = .5f + 4.f*e/(PRE_WAVE_CLEARED_TICS + WAVE_CLEARED_TICS);

	sprintf(str, "wave %d cleared", gc.cur_wave + 1);
	w = f*string_width_in_pixels(font, str);

	glColor4f(s, s*.6, s*.2, s);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(viewport_width/2 - w/2, viewport_height/2, 0.f);
	render_string(font, 0, 0, f, str);

	if (gc.tics_remaining > TIME_BONUS_MIN_TICS) {
		const float TIME_BONUS_SCALE = 1.f;
		int time_bonus = gc.tics_remaining*TIME_BONUS_PER_TIC;

		sprintf(str, "TIME BONUS: %d", time_bonus);

		glColor4f(1, 1, 1, s);

		w = TIME_BONUS_SCALE*string_width_in_pixels(font, str);
		glLoadIdentity();
		glTranslatef(.5f*(viewport_width - w), 3*viewport_height/4, 0.f);
		render_string(font, 0, 0, TIME_BONUS_SCALE, str);
	}

	glPopMatrix();
}

static float
title_displacement(float enter_tics, float leave_tics)
{
	if (inner_state.tics < enter_tics) {
		return -(1. - (float)inner_state.tics/enter_tics);
	} else if (inner_state.tics > WAVE_TITLE_TICS - leave_tics) {
		return ((float)inner_state.tics - (WAVE_TITLE_TICS - leave_tics))/leave_tics;
	} else {
		return 0;
	}
}

static void
draw_wave_title(void)
{
	static char str[80];
	int w;
	const struct wave *wave = levels[gc.cur_level]->waves[gc.cur_wave];
	static const char *GET_READY = "get ready!";

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	if (gc.cur_wave == 0) {
		float level_title_scale = 1.6f;

		sprintf(str, "LEVEL %d", gc.cur_level + 1);
		w = level_title_scale*string_width_in_pixels(font_big, str);

		glLoadIdentity();
		glTranslatef(viewport_width/2 - w/2 + title_displacement(.6*.35*WAVE_TITLE_TICS, .6*.1*WAVE_TITLE_TICS)*600., .5f*viewport_height, 0.f);

		glPushMatrix();
		glScalef(level_title_scale, level_title_scale, level_title_scale);
		glColor4f(1, 1, 1, 1);
		render_string(font_big, 0, 0, 1, str);
		glPopMatrix();
	}

	sprintf(str, "Wave %d / %d", gc.cur_level + 1, gc.cur_wave + 1);
	w = string_width_in_pixels(font_big, str);

	glLoadIdentity();
	glTranslatef(viewport_width/2 - w/2 + title_displacement(.6*.4*WAVE_TITLE_TICS, .6*.15*WAVE_TITLE_TICS)*800.f, .67*viewport_height, 0.f);

	glColor4f(1, 1, 1, 1);
	render_string(font_big, 0, 0, 1, str);

	w = string_width_in_pixels(font_medium, GET_READY);

	glLoadIdentity();
	glTranslatef(viewport_width/2 - w/2 + title_displacement(.6*.4*WAVE_TITLE_TICS, .6*.2*WAVE_TITLE_TICS)*1200., .75*viewport_height, 0);

	glColor4f(1.f, .6, .2, 1);

	render_string(font_medium, 0, 0, 1, GET_READY);

	glPopMatrix();
}

static void
draw_score(void)
{
	glColor3f(1, 1, 1);
	draw_fixed_width_number(gc.score, 1, LOG10_MAX_SCORE);
}

static void
draw_ship_icon(void)
{
	glBegin(GL_QUADS);

#define SHIP_ICON_SIZE 24*.8

	glTexCoord2f(0, 0);
	glVertex3f(0, 0, 0.f);

	glTexCoord2f(1, 0);
	glVertex3f(SHIP_ICON_SIZE, 0, 0.f);

	glTexCoord2f(1, 1);
	glVertex3f(SHIP_ICON_SIZE, SHIP_ICON_SIZE, 0.f);

	glTexCoord2f(0, 1);
	glVertex3f(0, SHIP_ICON_SIZE, 0.f);

	glEnd();
}

static void
draw_timer(void)
{
	const float big_digits_scale = 1;
	const float small_digits_scale = .6;
	const int TIME_CRITICAL = 10*FPS;

	glPushMatrix();
	glLoadIdentity();
	glTranslatef(viewport_width - 8 -
	  big_digits_scale*3*digit_width[0], 23, 0.f);

	if (ship.is_alive && gc.tics_remaining < TIME_CRITICAL) {
		float s = .4f + .6f*(.5f + sin(.2f*gc.level_tics));
		glColor3f(1, s, s);
	} else {
		glColor3f(1, 1, 1);
	}

	glPushMatrix();
	glScalef(big_digits_scale, big_digits_scale, big_digits_scale);
	draw_fixed_width_number(gc.tics_remaining/FPS, 1, 3);
	glPopMatrix();
	glTranslatef((big_digits_scale*3 - small_digits_scale*2)*digit_width[0],
	  .7*big_digits_scale*font_medium->char_height, 0);
	glScalef(small_digits_scale, small_digits_scale, small_digits_scale);
	draw_fixed_width_number((gc.tics_remaining*100/FPS)%100, 1, 2);
	glPopMatrix();

	if (inner_state.state == IS_IN_GAME &&
	  ship.is_alive && gc.tics_remaining < TIME_CRITICAL &&
	  (gc.level_tics/5) & 1) {
		const struct font_render_info *font = font_medium;
		const float s = 1.2f;
		float f, w;
		const char *str = "HURRY UP!";

		f = 1 + (float)gc.multiplier_tic_count/MULTIPLIER_MESSAGE_TTL;

		w = s*string_width_in_pixels(font, str);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glTranslatef(viewport_width/2 - w/2, viewport_height/4, 0.f);
		glColor4f(1, 0, 0, .5f);
		render_string(font, 0, 0, s, str);
		glPopMatrix();
	}
}

static void
draw_score_and_ships(void)
{
	/* gc.score */
	glPushMatrix();
        glTranslatef(4, 23, 0.f);
	draw_score();

	/* gc.multiplier */
	if (gc.multiplier > 1) {
		glColor3f(1, .8, 0.f);
		glTranslatef(viewport_width/2 - 30, 0, 0);
		render_string(font_medium, 0, 0, 1.f, "x");
		glTranslatef(32, 2, 0);
		render_string(font_medium, 0, 0, 1.5f, "%d", gc.multiplier);
	}

	glPopMatrix();

	/* time */
	draw_timer();

	glPushMatrix();

	glColor3f(1, 1, 1);

	glTranslatef(8, 34, 0);

	if (gc.ships_left > 3) {
		/* ship icon */

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, gl_texture_id(ship_outline_texture_id));

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		draw_ship_icon();

		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);

		/* # of ships left */

		glTranslatef(.8*29, .8*15, 0);
		render_string(font_medium, 0, 0, .9*.4, "x");

		glTranslatef(.8*17, .8*1.5, 0);
		render_string(font_medium, 0, 0, .8*.8, "%d", gc.ships_left);
	} else {
		int i;

		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, gl_texture_id(ship_outline_texture_id));

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		for (i = 0; i < gc.ships_left; i++) {
			draw_ship_icon();
			glTranslatef(SHIP_ICON_SIZE, 0, 0);
		}

		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
	}

        glPopMatrix();
}

static void
draw_level_diagram(void)
{
	int i;
	const float scale = 3.5f;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glLineWidth(1.5f);

	glPushMatrix();

        glTranslatef(20, viewport_height - 60, 0);
	glScalef(scale, scale, scale);

	for (i = 0; i < levels[gc.cur_level]->num_waves; i++) {
		const struct arena *p = levels[gc.cur_level]->waves[i]->arena;
		float s, ts, x, y;
		int t, l;

		x = p->center.y;
		y = p->center.x;
		if (x >= 180)
			x -= 180;

		glPushMatrix();
		glTranslatef(.47f*x, .47f*y, 0.f);

		if (i == gc.cur_wave) {
			s = 1.f;
			t = AB_DIAGRAM_SELECTED;
		} else {
			s = .3f;
			t = AB_DIAGRAM;
		}

		glScalef(1, -1, 1);
		draw_arena_outline(p, s, t);

		if (i == gc.cur_wave)
			glColor4f(1, 1, 0, .8f*s);
		else
			glColor4f(1, 1, 1, .8f*s);

		ts = .15f;

		glScalef(ts, -ts, ts);

		l = string_width_in_pixels(font_medium, "%d", i + 1);
		render_string(font_medium, -.5f*l, 0, 1, "%d", i + 1);

		glPopMatrix();
	}

	glPopMatrix();

	glPopAttrib();
} 

#define POWERUP_BAR_HEIGHT 10
#define POWERUP_BAR_RIGHT_BORDER 18
#define POWERUP_BAR_WIDTH 200

static void
draw_powerup_bars(void)
{
	int i;
	float y = viewport_height - 2*POWERUP_BAR_HEIGHT;
	const int POWERUP_BAR_LEFT_BORDER =
	  viewport_width - (POWERUP_BAR_RIGHT_BORDER + POWERUP_BAR_WIDTH);

	glColor3f(1, 1, 1);

	glPushMatrix();

	for (i = 0; i < NUM_POWERUP_TYPES; i++) {
		if (ship.powerup_ttl[i] > 0) {
			int l;
			float r;
			const char *s = powerup_settings[i]->text->text;
			
			r = (float)(POWERUP_BAR_WIDTH - 4)*ship.powerup_ttl[i]/
			  initial_powerup_ttl[i];

			l = string_width_in_pixels(font_small, "%s", s);

			glPushMatrix();
			glTranslatef(POWERUP_BAR_LEFT_BORDER - l - 6, y + 6, 0);
			glColor3f(1, 0, 0);
		        render_string(font_small, 0, 0, 1, "%s", s);
			glPopMatrix();

			glBegin(GL_LINE_LOOP);
			glVertex2f(POWERUP_BAR_LEFT_BORDER, y);
			glVertex2f(POWERUP_BAR_LEFT_BORDER + POWERUP_BAR_WIDTH,
			  y);
			glVertex2f(POWERUP_BAR_LEFT_BORDER + POWERUP_BAR_WIDTH,
			  y + POWERUP_BAR_HEIGHT);
			glVertex2f(POWERUP_BAR_LEFT_BORDER,
			  y + POWERUP_BAR_HEIGHT);
			glEnd();

			glColor3f(1, 1, 1);

			glBegin(GL_QUADS);
			glColor3f(1, 0, 0);
			glVertex2f(POWERUP_BAR_LEFT_BORDER + 2, y + 1);
			glColor3f(1, 0, 0);
			glVertex2f(POWERUP_BAR_LEFT_BORDER + 2 + r, y + 1);
			glColor3f(.5, 0, 0);
			glVertex2f(POWERUP_BAR_LEFT_BORDER + 2 + r,
			  y + POWERUP_BAR_HEIGHT - 2);
			glColor3f(.5, 0, 0);
			glVertex2f(POWERUP_BAR_LEFT_BORDER + 2,
			  y + POWERUP_BAR_HEIGHT - 2);
			glEnd();

			y -= 3*POWERUP_BAR_HEIGHT/2;
		}
	}

	glPopMatrix();
}

static void
draw_multiplier_message(void)
{
	if (gc.multiplier != 1 &&
	  (gc.multiplier_tic_count <= MULTIPLIER_MESSAGE_TTL) &&
	  (gc.multiplier_tic_count/5) & 1) {
		const struct font_render_info *font = font_medium;
		const float s = 1.2f;
		float f, w;
		char str[80];

		f = 1 + (float)gc.multiplier_tic_count/MULTIPLIER_MESSAGE_TTL;

		sprintf(str, "x%d MULTIPLIER", gc.multiplier);
		w = s*string_width_in_pixels(font, str);

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glTranslatef(viewport_width/2 - w/2, 3*viewport_height/4, 0.f);
		glColor4f(1, 1, 1, .5f);
		render_string(font, 0, 0, s, str);
		glPopMatrix();
	}
}

static void
draw_combo_message(void)
{
	if (gc.combo_message_tics > 0) {
		if ((gc.combo_message_tics/5) & 1) {
			const struct font_render_info *fri = font_medium;
			const float s = .9;
			char str[80];

			sprintf(str, "%d", gc.last_combo);

			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadIdentity();
			glTranslatef(4, 74, 0.f);
			glColor4f(1, 1, 0, .7);

			render_string(fri, 0, 0, s, str);
			glTranslatef(s*string_width_in_pixels(fri, str), 0, 0);
			render_string(fri, 0, 0, 0.6, " COMBO!");

			glPopMatrix();
		}

		--gc.combo_message_tics;
	}
}

static void
draw_text(void)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.f, viewport_width, viewport_height, 0.f, -1.f, 1.f);
	glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

	glColor3f(1, 1, 1);

	draw_score_and_ships();
	draw_level_diagram();

	switch (inner_state.state) {
		case IS_WAVE_TITLE:
			draw_wave_title();
			draw_powerup_bars();
			break;

		case IS_IN_GAME:
			draw_multiplier_message();
			draw_combo_message();
			draw_powerup_bars();
			break;

		case IS_GAME_OVER:
		case IS_RANK:
			draw_game_over();
			break;

		case IS_WAVE_CLEARED:
		case IS_PRE_WAVE_CLEARED:
			draw_wave_cleared();
			break;

		case IS_LEVEL_CLEARED:
		case IS_LEVEL_TRANSITION:
			draw_level_cleared();
			break;
	}

	if (serializing)
		draw_serializing();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);

	glPopAttrib();
}

static void
reset_eye(void)
{
	vec3_set(&gc.eye, 0, 0, -BASE_EYE_Z - .5f*WAVE_TITLE_TICS);
	vec3_zero(&delta_eye);
}

static void
reset_wave(void)
{
	const struct wave *wave = levels[gc.cur_level]->waves[gc.cur_wave];

	cur_arena = wave->arena;
	gc.foes_left = wave->foe_count;
	gc.tics_remaining = 45*FPS;
}

static void
reset_level(void)
{
	memset(&level_stat_counters, 0, sizeof(level_stat_counters));

	if (gc.cur_level == settings.static_settings->start_level)
		gc.cur_wave = settings.static_settings->start_wave;
	else
		gc.cur_wave = 0;

	reset_wave();
}

void
reset_game_state(void)
{
	extern int last_highscore;

	SDL_ShowCursor(SDL_DISABLE);

	last_highscore = -1;
	gc.score = 0;
	gc.ships_left = 2;
	gc.level_tics = 0;
	gc.multiplier = 1;

	gc.cur_level = settings.static_settings->start_level;

	ship.is_alive = 0;
	gc.tics_remaining = 0;

	/* last_death_tic = -70; */

	memset(&game_stat_counters, 0, sizeof(game_stat_counters));

	ships_text_width = string_width_in_pixels(font_small, "ships");

	reset_level();
	reset_eye();
	reset_water();
	reset_background();
	reset_arena();
	reset_particles();
	reset_explosions();
	reset_missiles();
	reset_bombs();
	reset_lasers();
	reset_foes();
	reset_powerups();
	reset_in_game_texts();
	reset_ship_powerups();

	set_inner_state(IS_WAVE_TITLE);

	play_music(MUS_STAGE_1);
}

void
initialize_in_game_state(void)
{
	int i;

	for (i = 0; i < 10; i++) {
		char s[2] = { i + '0', '\0' };
		digit_width[i] = string_width_in_pixels(font_medium, s);
	}

	crosshair_arrow_texture_id =
	  png_to_texture("crosshair-arrow.png");
}

void
set_light_pos(void)
{
	extern const GLfloat light_ambient[];
	extern const GLfloat light_diffuse[];
	extern const GLfloat light_specular[];
	extern const GLfloat light_pos[];

	glLightfv(GL_LIGHT0, GL_POSITION, (GLfloat *)&light_pos);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
}

static void
set_modelview_matrix(void)
{
	glTranslatef(gc.eye.x, gc.eye.y, gc.eye.z);
	set_light_pos();
}

void
set_modelview_rotation(void)
{
	float s = cur_arena->center.x;
	float t = cur_arena->center.y;

	if ((inner_state.state == IS_WAVE_CLEARED &&
	  inner_state.tics > WAVE_CLEARED_TICS/2) || inner_state.state == IS_LEVEL_TRANSITION) {
		float k;
		const struct arena *next_arena;
		
		if (inner_state.state == IS_WAVE_CLEARED) {
			const struct level *level = levels[gc.cur_level];
			next_arena = level->waves[(gc.cur_wave + 1)%level->num_waves]->arena;
			k = ((float)inner_state.tics - WAVE_CLEARED_TICS/2)/
			  (WAVE_CLEARED_TICS/2);
		} else {
			next_arena = levels[(gc.cur_level + 1)%NUM_LEVELS]->waves[0]->arena;
			k = (float)inner_state.tics/LEVEL_TRANSITION_TOTAL_TICS;
		}

		s = (1.f - k)*cur_arena->center.x + k*next_arena->center.x;
		t = (1.f - k)*cur_arena->center.y + k*next_arena->center.y;
	} else {
		s = cur_arena->center.x;
		t = cur_arena->center.y;
	}

	glTranslatef(0.f, 0.f, -CELL_RADIUS);
	glRotatef(-s, 1, 0, 0);
	glRotatef(-t, 0, 1, 0);
	glTranslatef(0.f, 0.f, CELL_RADIUS);
}

float
get_arena_alpha(void)
{
	float k;

	if (inner_state.state == IS_LEVEL_CLEARED) {
		k = 1;
	} else if (inner_state.state == IS_LEVEL_TRANSITION) {
		if (inner_state.tics < (LEVEL_TRANSITION_TOTAL_TICS/4)) {
			k = 1.f - (float)inner_state.tics/(LEVEL_TRANSITION_TOTAL_TICS/4);
		} else if (inner_state.tics > 3*LEVEL_TRANSITION_TOTAL_TICS/4) {
			k = (float)(inner_state.tics - 3*LEVEL_TRANSITION_TOTAL_TICS/4)/(LEVEL_TRANSITION_TOTAL_TICS/4);
		} else {
			k = 0;
		}
	} else if (inner_state.state == IS_WAVE_CLEARED) {
		if (inner_state.tics > WAVE_CLEARED_TICS/2)
			k = 1.f;
		else
			k = (float)inner_state.tics/
				(WAVE_CLEARED_TICS/2);
	} else if (inner_state.state == IS_WAVE_TITLE) {
		if (inner_state.tics < WAVE_TITLE_TICS/2)
			k = 1.f;
		else
			k = 1.f - (float)(inner_state.tics -
			  WAVE_TITLE_TICS/2)/(WAVE_TITLE_TICS/2);
	} else {
		k = 0.f;
	}

	return k;
}

static void
update_crosshair(void)
{
	GLdouble modelview[16], projection[16];
	GLdouble x0, y0, z0;
	GLdouble x1, y1, z1;
	float x, y, t;
	GLint viewport[4];

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	set_modelview_matrix();

	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
	glGetDoublev(GL_PROJECTION_MATRIX, projection);

	gluUnProject(last_mouse_x, viewport[3] - last_mouse_y - 1, 0,
	  modelview, projection, viewport,
	  &x0, &y0, &z0);

	gluUnProject(last_mouse_x, viewport[3] - last_mouse_y - 1, 1,
	  modelview, projection, viewport,
	  &x1, &y1, &z1);

	/* (x, y) at z = 0 */

	t = -z0/(z1 - z0);

	x = x0 + t*(x1 - x0);
	y = y0 + t*(y1 - y0);

	vec2_set(&crosshair, x, y);

	glPopMatrix();
}

static void
update_level_tics(void)
{
	++gc.level_tics;
}

static void
update_eye(void)
{
	vector3 wanted, dir;
	float d;
	float max_speed;

	vec3_add_to(&gc.eye, &delta_eye);

	if (inner_state.state == IS_WAVE_CLEARED) {
		vec3_set(&wanted, 0, 0, -3.f*BASE_EYE_Z);
	} else {
		if (!ship.is_alive) {
			vec3_set(&wanted, 0, 0, -BASE_EYE_Z);
		} else {
			const struct foe_common *closest_foe;

			closest_foe = get_closest_foe(&ship.pos);

			if (closest_foe &&
#define D 4.f
					(d = vec2_distance(&closest_foe->pos, &ship.pos)) < D) {
#define R .7
				wanted.z = -(R*BASE_EYE_Z + (d/D)*(1.f - R)*BASE_EYE_Z);
			} else {
				wanted.z = -BASE_EYE_Z;
			}

			wanted.x = -(-BASE_EYE_Z/wanted.z)*.3f*ship.pos.x;
			wanted.y = -(-BASE_EYE_Z/wanted.z)*.3f*ship.pos.y;
		}
	}

	if (inner_state.state == IS_IN_GAME)
		max_speed = .5f;
	else
		max_speed = 1.5f;

	vec3_sub(&dir, &wanted, &gc.eye);
	vec3_mul_scalar(&dir, .05f);

	vec3_add_to(&delta_eye, &dir);
	vec3_clamp_length(&delta_eye, max_speed);

	/* damp */
	vec3_mul_scalar(&delta_eye, .7);
}

static void
update_multiplier(void)
{
	/* XXX: NOT YET */
	if (++gc.multiplier_tic_count >= MULTIPLIER_CHANGE_TICS &&
	  gc.multiplier < MAX_MULTIPLIER) {
		++gc.multiplier;
		gc.multiplier_tic_count = 0;
	}
}

static void
update_combo(void)
{
	if (last_score_tic != -1 &&
	  inner_state.tics - last_score_tic > MAX_COMBO_TIC_INTERVAL) {
		if (cur_combo > MIN_COMBO_KILLS) {
			gc.last_combo = cur_combo;
			gc.combo_message_tics = COMBO_MESSAGE_TTL;

			level_stat_counters.combo_count++;
			game_stat_counters.combo_count++;

			gc.score += cur_combo*30;
		}

		last_score_tic = -1;
		cur_combo = 0;
	}
}

static int
ship_is_visible(void)
{
	return !(
	  (inner_state.state == IS_WAVE_CLEARED ||
	   inner_state.state == IS_LEVEL_CLEARED ||
	   inner_state.state == IS_LEVEL_TRANSITION) ||
	  (inner_state.state == IS_WAVE_TITLE &&
	    inner_state.tics < WAVE_TITLE_TICS/2));
}
 
static void
update(struct game_state *gs)
{
	if (inner_state.state != IS_LEVEL_CLEARED &&
	  inner_state.state != IS_LEVEL_TRANSITION)
		update_eye();

	update_crosshair();
	update_background();
	update_water();
	update_particles();
	update_explosions();

	if (ship_is_visible()) {
		update_ship();
		update_missiles();
		update_bombs();
		update_lasers();
		update_foes();
	}

	update_powerups();
	update_in_game_texts();

	if (inner_state.state == IS_IN_GAME) {
		update_multiplier();
		update_combo();
	}

	if (inner_state.state == IS_GAME_OVER) {
		if (inner_state.tics >= GAME_OVER_FADE_IN_TICS) {
			if (!update_stats_table()) {
				gc.score += game_over_stats.bonus;
				set_inner_state(IS_RANK);
			}
		}
	}

	if (inner_state.state == IS_LEVEL_CLEARED) {
		if (inner_state.tics >= LEVEL_CLEARED_FADE_IN_TICS) {
			if (!update_stats_table()) {
				gc.score += game_over_stats.bonus;
				set_inner_state(IS_LEVEL_TRANSITION);
			}
		}
	}

	update_level_tics();
	update_inner_state();

	if (serializing)
		serialize();
}

static void
draw_level_background()
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	if (inner_state.state != IS_WAVE_CLEARED &&
	  inner_state.state != IS_LEVEL_CLEARED &&
	  inner_state.state != IS_LEVEL_TRANSITION) {
		glDisable(GL_BLEND);
		draw_water();
	} else {
		int level;

		if (inner_state.state == IS_LEVEL_TRANSITION && inner_state.tics > LEVEL_TRANSITION_TOTAL_TICS/2)
			level = (gc.cur_level + 1)%NUM_LEVELS;
		else
			level = gc.cur_level;

		glPushMatrix();
		set_modelview_rotation();
		draw_background(level, get_arena_alpha(), 1);
		glPopMatrix();
	}

	glDisable(GL_LIGHTING);
	glDisable(GL_LIGHT0);

	/* arena flash */

	if (inner_state.state == IS_WAVE_TITLE &&
	  inner_state.tics < WAVE_TITLE_TICS/4) {
		float k = 1.f - (float)inner_state.tics/(WAVE_TITLE_TICS/4);

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glColor3f(k, k, k);
		draw_filled_arena(cur_arena);
	}

	glPopAttrib();
}

static void
draw_crosshair(void)
{
	const int NUM_ARROWS = 4;
	int i;
	float a, da, r;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glShadeModel(GL_FLAT);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_TEXTURE_2D);

	glColor3f(.1, .1, .1);

	glPushMatrix();
	glTranslatef(crosshair.x, crosshair.y, 0.f);

	glBegin(GL_LINES);

	glVertex3f(-15.f, 0.f, 0.f);
	glVertex3f(15.f, 0.f, 0.f);
	glVertex3f(0.f, -15.f, 0.f);
	glVertex3f(0.f, 15.f, 0.f);
	glEnd();

	glPopMatrix();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor3f(1, 1, 1);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(crosshair_arrow_texture_id));

	da = 2.f*M_PI/NUM_ARROWS;

	a = sin(.1*gc.level_tics) - (.3f + cos(.2*gc.level_tics));

	r = .2 + .05*sin(.2*gc.level_tics);

	glBegin(GL_QUADS);

	for (i = 0; i < NUM_ARROWS; i++) {
		float x = r*sin(a);
		float y = r*cos(a);

		float ox = -y;
		float oy = x;

		glTexCoord2f(1, 0);
		glVertex2f(crosshair.x + x + .5f*ox, crosshair.y + y + .5f*oy);

		glTexCoord2f(1, 1);
		glVertex2f(crosshair.x + 2*x + .5f*ox, crosshair.y + 2*y + .5f*oy);

		glTexCoord2f(0, 1);
		glVertex2f(crosshair.x + 2*x - .5f*ox, crosshair.y + 2*y - .5f*oy);

		glTexCoord2f(0, 0);
		glVertex2f(crosshair.x + x - .5f*ox, crosshair.y + y - .5f*oy);

		a += da;
	}

	glEnd();

	glPopAttrib();
}
      
static void
draw(struct game_state *gs)
{
	extern struct game_state *cur_state;
	extern struct game_state playback_state;

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	set_modelview_matrix();

	if (!settings.static_settings || settings.static_settings->background)
		draw_level_background();

	if (cur_state != &playback_state &&
	  settings.control_type == CONTROL_HYBRID &&
	  ship_is_visible())
		draw_crosshair();

	if (inner_state.state != IS_LEVEL_CLEARED &&
	  inner_state.state != IS_LEVEL_TRANSITION &&
	  (inner_state.state != IS_WAVE_CLEARED ||
	  inner_state.tics < WAVE_CLEARED_TICS/2))
		draw_arena_outline(cur_arena, 1, cur_arena->border_type);

	draw_explosions();
	draw_particles();

	if (ship_is_visible()) {
		draw_ship();
		draw_missiles();
		draw_bombs();
		draw_lasers();
		draw_foes();
		draw_powerups();
	}

	draw_in_game_texts();
	draw_text();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

static void
on_mouse_motion(struct game_state *gs, int mouse_x, int mouse_y)
{
}

static void
on_mouse_button_pressed(struct game_state *gs, int x, int y)
{
}

static void
on_mouse_button_released(struct game_state *gs, int x, int y)
{
}

static void
on_key_pressed(struct game_state *gs, int key)
{
	switch (key) {
		case SDLK_ESCAPE:
			SDL_ShowCursor(SDL_ENABLE);
			start_in_game_menu();
			break;

		case SDLK_TAB:
			if (settings.static_settings->serialization) {
				if (!serializing)
					start_serialization();
				else
					end_serialization();
			}
			break;
	}
}

static void
on_stick_pressed(struct game_state *gs, int stick)
{
}

struct game_state in_game_state =
  { update, draw,
    on_mouse_motion, on_mouse_button_pressed, on_mouse_button_released,
    on_key_pressed,
    on_stick_pressed };
