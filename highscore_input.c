#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <GL/gl.h>
#include <SDL_keysym.h>

#include "common.h"
#include "font_render.h"
#include "gl_util.h"
#include "in_game.h"
#include "settings.h"
#include "highscores.h"
#include "highscore_input.h"

const int viewport_width = 800;
const int viewport_height = 600;

const float key_border = 4.f;

enum {
	KEY_SMALL,
	KEY_BIG,
	NUM_KEY_TYPES,
};

enum {
	NUM_KEYS = 48,
	ACCEPT_KEY = NUM_KEYS - 1,
	BACK_KEY = NUM_KEYS - 2,
};

static struct key_info {
	float width;
	float height;
	float font_scale;
	const char *texture_source;
	const char *sel_texture_source;
	int texture_id;
	int sel_texture_id;
} key_info[NUM_KEY_TYPES] = {
	{ 50.f, 50.f, .8f, "key.png", "key-selected.png", 0, 0 },
	{ 200.f, 50.f, .8f, "key-big.png", "key-big-selected.png", 0, 0 },
};

struct key {
	float x, y;
	char str[16];
	struct key_info *info;
	void (*action)(const struct key *);
	struct key *up, *down, *left, *right;
};

static struct key keys[NUM_KEYS];

static struct key *selected_key;

static char name[MAX_NAME_LEN];
static int name_char_index;
static int tic_count;

void
reset_highscore_input_state(void)
{
	memset(name, '-', MAX_NAME_LEN);
	name_char_index = 0;
	selected_key = &keys[0];
	tic_count = 0;
}

static void
load_textures(void)
{
	struct key_info *p;

	for (p = key_info; p != &key_info[NUM_KEY_TYPES]; p++) {
		p->texture_id = png_to_texture(p->texture_source);
		p->sel_texture_id = png_to_texture(p->sel_texture_source);
	}
}

enum {
	KEY_ROWS = 4,
};

static void
default_key_action(const struct key *key)
{
	if (name_char_index < MAX_NAME_LEN)
		name[name_char_index++] = *key->str;
}

static void
back_key_action(const struct key *key)
{
	if (name_char_index > 0)
		--name_char_index;
}

static void
accept_key_action(const struct key *key)
{
	highscores_add(name, gc.score, gc.cur_level + 1, gc.cur_wave + 1);
	start_main_menu();
}

static void
initialize_keys(void)
{
	static char *key_chars[] = {
	  "ABCDEFGHIJKL", "MNOPQRSTUVW",
	  "XYZ%<>&/!;,.", "0123456789*" };
	int i, j;
	float y, x;
	struct key *p;

	p = keys;

	y = 220;

	for (i = 0; i < KEY_ROWS; i++) {
		const char *row = key_chars[i];
		int l;

		l = strlen(row);
		
		x = .5f*(viewport_width - (l*key_info[KEY_SMALL].width + (l - 1)*key_border));

		for (j = 0; j < l; j++) {
			p->x = x;
			p->y = y;
			sprintf(p->str, "%c", row[j]);
			p->info = &key_info[KEY_SMALL];
			p->action = default_key_action;

			if (i > 0) {
				int prev_row_len = strlen(key_chars[i - 1]);
				int next_index = j >= prev_row_len ? prev_row_len - 1 : j;
				p->up = &p[-j - prev_row_len + next_index];
			}

			if (i < KEY_ROWS - 1) {
				int next_row_len = strlen(key_chars[i + 1]);
				int next_index = j >= next_row_len ? next_row_len - 1 : j;
				p->down = &p[-j + l + next_index];
			} else if (row[j] >= '1' && row[j] <= '4') {
				p->down = &keys[BACK_KEY];
			} else if (row[j] >= '6' && row[j] <= '9') {
				p->down = &keys[ACCEPT_KEY];
			}

			if (j > 0)
				p->left = &p[-1];

			if (j < l - 1)
				p->right = &p[1];

			++p;

			x += key_info[KEY_SMALL].width + key_border;
		}

		y += key_info[KEY_SMALL].height + key_border;
	}

	x = .5f*(viewport_width - (2*key_info[KEY_BIG].width + 32));

	p->x = x;
	p->y = y;
	strcpy(p->str, "BACK");
	p->info = &key_info[KEY_BIG];
	p->action = back_key_action;
	p->right = &keys[ACCEPT_KEY];
	p->up = &keys[37]; /* eeeewww */
	++p;

	x += key_info[KEY_BIG].width + 32;

	p->x = x;
	p->y = y;
	strcpy(p->str, "ACCEPT!");
	p->info = &key_info[KEY_BIG];
	p->action = accept_key_action;
	p->left = &keys[BACK_KEY];
	p->up = &keys[43]; /* yuck */
	++p;

	assert(p = &keys[NUM_KEYS]);
}

void
initialize_highscore_input(void)
{
	load_textures();
	initialize_keys();
}

static struct key *
key_at(int x, int y)
{
	struct key *p;

	float xl = (float)viewport_width*x/window_width;
	float yl = (float)viewport_height*y/window_height;

	for (p = keys; p != &keys[NUM_KEYS]; p++) {
		if (xl >= p->x && xl <= p->x + p->info->width &&
		  yl >= p->y && yl <= p->y + p->info->height) {
			return p;
		}
	}

	return NULL;
}

static void
draw_key(struct key *key)
{
	int str_width;
	const struct font_render_info *fri = font_medium;
	const struct key_info *info = key->info;
	const float x = key->x;
	const float y = key->y;
	const float w = info->width;
	const float h = info->height;
	const char *str = key->str;

	str_width = string_width_in_pixels(fri, "%s", str);

	glDisable(GL_TEXTURE_2D);

	glPushMatrix();
	glTranslatef(x + .5f*w, y + .5f*h, 0);
	glScalef(info->font_scale, info->font_scale, 0);
	glTranslatef(-.5f*str_width - 3, -.5f*fri->char_height + 16, 0);
	render_string(fri, 0, 0, 1, "%s", str);
	glPopMatrix();

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(key == selected_key ? info->sel_texture_id : info->texture_id));

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glColor3f(1, 1, 1);

	glBegin(GL_QUADS);

	glTexCoord2f(0, 1);
	glVertex3f(x, y, 0.f);

	glTexCoord2f(1, 1);
	glVertex3f(x + w, y, 0.f);

	glTexCoord2f(1, 0);
	glVertex3f(x + w, y + h, 0.f);

	glTexCoord2f(0, 0);
	glVertex3f(x, y + h, 0.f);

	glEnd();
}

static void
draw_keyboard(void)
{
	struct key *p;

	glColor4f(1., 1., 1., 1.);

	for (p = keys; p != &keys[NUM_KEYS]; p++)
		draw_key(p);
}

enum {
	TEXT_LINES = 3
};

static void
draw_centered_text(float y, const struct font_render_info *fri, float scale, const char *str)
{
	const int width = string_width_in_pixels(fri, "%s", str);

	glPushMatrix();
	glTranslatef(.5f*viewport_width, y, 0);
	glScalef(scale, scale, 0);
	glTranslatef(-.5f*width, .5f*fri->char_height, 0);
	render_string(fri, 0, 0, 1, "%s", str);
	glPopMatrix();
}

static void
draw_text(void)
{
	glColor4f(1., .6, .2, 1.);
	draw_centered_text(80, font_medium, 1.f, "YOU GOT A HIGH SCORE!");
	draw_centered_text(110, font_medium, .5f, "PLEASE ENTER YOUR NAME!");
}

static void
draw_name(void)
{
	int i;
	const struct font_render_info *fri = font_medium;
	float char_width = char_width_in_pixels(fri, 'M');
	const float scale = 1;
	float x;

	glColor4f(1., .1, .1, 1.);

	x = .5f*(viewport_width - scale*char_width*2*(MAX_NAME_LEN - 1));

	for (i = 0; i < MAX_NAME_LEN; i++) {
		if (!(i == name_char_index && (tic_count/16)&1)) {
			char ch = name[i];

			glPushMatrix();
			glTranslatef(x, 150, 0);
			glScalef(scale, scale, 0);
			glTranslatef(-.5f*char_width_in_pixels(fri, ch), .5f*fri->char_height, 0);
			render_string(fri, 0, 0, 1, "%c", ch);
			glPopMatrix();
		}

		x += 2.f*scale*char_width;
	}
}

static void
draw(struct game_state *gs)
{
	in_game_state.draw(&in_game_state);

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.f, viewport_width, viewport_height, 0.f, -1.f, 1.f);

	glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);

	draw_text();
	draw_keyboard();
	draw_name();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);

	glPopAttrib();
}

static void
update(struct game_state *gs)
{
	in_game_state.update(&in_game_state);
	tic_count++;
}

static void
on_mouse_motion(struct game_state *gs, int x, int y)
{
	struct key *p = key_at(x, y);

	if (p != NULL)
		selected_key = p;
}

static void
on_mouse_button_pressed(struct game_state *gs, int x, int y)
{
	struct key *p = key_at(x, y);

	if (p != NULL) {
		selected_key = p;
		p->action(p);
	}
}

static void
on_mouse_button_released(struct game_state *gs, int x, int y)
{
}

static void
on_up_key(void)
{
	if (selected_key && selected_key->up)
		selected_key = selected_key->up;
}

static void
on_down_key(void)
{
	if (selected_key && selected_key->down)
		selected_key = selected_key->down;
}

static void
on_left_key(void)
{
	if (selected_key && selected_key->left)
		selected_key = selected_key->left;
}

static void
on_right_key(void)
{
	if (selected_key && selected_key->right)
		selected_key = selected_key->right;
}

static void
on_return_key(void)
{
	if (selected_key)
		selected_key->action(selected_key);
}

static void
on_key_pressed(struct game_state *gs, int key)
{
	switch (key) {
		case SDLK_UP:
			on_up_key();
			break;

		case SDLK_DOWN:
			on_down_key();
			break;

		case SDLK_LEFT:
			on_left_key();
			break;

		case SDLK_RIGHT:
			on_right_key();
			break;

		case SDLK_RETURN:
		case SDLK_KP_ENTER:
			on_return_key();
			break;
	}
}

static void
on_stick_pressed(struct game_state *gs, int stick)
{
	if (stick == settings.pad_sticks[0]) {
		on_up_key();
	} else if (stick == settings.pad_sticks[1]) {
		on_down_key();
	} else if (stick == settings.pad_sticks[2]) {
		on_left_key();
	} else if (stick == settings.pad_sticks[3]) {
		on_right_key();
	} else if (stick == settings.pad_sticks[4] ||
	  stick == settings.pad_sticks[5] ||
	  stick == settings.pad_sticks[6] ||
	  stick == settings.pad_sticks[7]) {
		on_return_key();
	}
}

struct game_state highscore_input_state =
  { update, draw,
    on_mouse_motion, on_mouse_button_pressed, on_mouse_button_released,
    on_key_pressed, on_stick_pressed };
