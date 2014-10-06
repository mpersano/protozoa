#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <GL/gl.h>
#include <SDL_keysym.h>

#include "gl_util.h"
#include "list.h"
#include "font_render.h"
#include "menu.h"
#include "common.h"
#include "settings.h"
#include "audio.h"

static const int viewport_width = 800;
static const int viewport_height = 600;

static int
menu_item_is_selectable(const struct menu_item *item);

static struct menu_item *
menu_first_selectable_item(struct menu *menu);

static void
menu_reset_hilite(struct menu *menu)
{
	struct list_node *ln;

	menu->hilite = NULL;

	for (ln = menu->items->first; ln; ln = ln->next) {
		struct menu_item *item = (struct menu_item *)ln->data;
		item->hilite_scale = 0;
	}
}

int
menu_is_on_top_level(struct menu *menu)
{
	return menu->cur_submenu == NULL;
}

static void
menu_on_back(struct menu *menu)
{
	assert(menu->parent);
	menu->parent->cur_submenu = NULL;

	menu_reset_hilite(menu);
/*
	menu_reset_hilite(menu->parent);
*/
}

struct menu *
menu_make(const char *name, const struct font_render_info *fri, float scale,
  int align, float x, float y)
{
	struct menu *m;

	m = malloc(sizeof *m);

	m->name = strdup(name);
	m->fri = fri;
	m->items = list_make();
	m->max_text_width = 0;
	m->hilite = NULL;
	m->parent = NULL;
	m->cur_submenu = NULL;
	m->scale = scale;
	m->item_height = m->scale*3*fri->char_height/2;
	m->defining_key = 0;
	m->on_activate = NULL;
	m->align = align;
	m->x = x;
	m->y = y;
	m->spread = 0;

	return m;
}

void menu_free(struct menu *);

static void
menu_item_free(struct menu_item *item)
{
	if (item->submenu)
		menu_free(item->submenu);

	if (item->text)
		free(item->text);
}

void
menu_free(struct menu *menu)
{
	list_free(menu->items, (void(*)(void *))menu_item_free);
	free(menu->name);
	free(menu);
}

static struct menu_item *
menu_item_make(enum menu_item_type type, const char *text)
{
	struct menu_item *mi;

	mi = malloc(sizeof *mi);

	mi->type = type;
	mi->text = text ? strdup(text) : NULL;;
	mi->visible = 1;
	mi->parent = NULL;
	mi->on_action = NULL;
	mi->get_status = NULL;
	mi->submenu = NULL;
	mi->hilite_scale = 0;

	return mi;
}

static void
menu_add_item(struct menu *menu, struct menu_item *mi)
{
	int w;

	list_append(menu->items, mi);

	if (menu->hilite == NULL && menu_item_is_selectable(mi)) {
		menu->hilite = mi;
		mi->hilite_scale = 1;
	}

	mi->parent = menu;

	if (mi->text) {
		w = string_width_in_pixels(menu->fri, mi->text);

		if (w > menu->max_text_width)
			menu->max_text_width = w;
	}
}

struct menu *
menu_add_submenu_item(struct menu *menu, const char *name, float scale,
  int align)
{
	struct menu_item *mi;

	mi = menu_item_make(MI_SUBMENU, name);
	mi->submenu = menu_make(name, menu->fri, scale, align, -1, -1);
	mi->submenu->parent = menu;

	menu_add_item(menu, mi);

	return mi->submenu;
}

void
menu_add_space_item(struct menu *menu)
{
	menu_add_item(menu, menu_item_make(MI_SPACE, NULL));
}

void
menu_add_title_item(struct menu *menu, const char *name)
{
	menu_add_item(menu, menu_item_make(MI_TITLE, name));
}

void
menu_add_action_item(struct menu *menu, const char *text,
  void(*on_action)(void *extra), void *extra)
{
	struct menu_item *mi;

	mi = menu_item_make(MI_ACTION, text);
	mi->on_action = on_action;
	mi->extra_on_action = extra;

	menu_add_item(menu, mi);
}

void
menu_add_previous_menu_item(struct menu *menu)
{
	menu_add_action_item(menu, "Go Back", (void(*)(void *))menu_on_back,
	  menu);
}

void
menu_add_toggle_item(struct menu *menu, const char *text,
  void(*on_action)(void *extra), void *extra_on_action,
  const char *(*get_status)(void *extra_get_status),
  void *extra_get_status)
{
	struct menu_item *mi;

	mi = menu_item_make(MI_TOGGLE, text);

	mi->on_action = on_action;
	mi->extra_on_action = extra_on_action;

	mi->get_status = get_status;
	mi->extra_get_status = extra_get_status;

	menu_add_item(menu, mi);
}

void
menu_add_define_key_item(struct menu *menu, const char *text,
  const char *(*get_key_status)(void *), void *extra_get_key_status,
  int(*set_key)(int, void *), void *extra_set_key,
  int(*set_stick)(int, void *), void *extra_set_stick)
{
	struct menu_item *mi;

	mi = menu_item_make(MI_DEFINE_KEY, text);

	mi->get_status = get_key_status;
	mi->extra_get_status = extra_get_key_status;

	mi->set_key = set_key;
	mi->extra_set_key = extra_set_key;

	mi->set_stick = set_stick;
	mi->extra_set_stick = extra_set_stick;

	menu_add_item(menu, mi);
}

static const char *
menu_item_text(const struct menu *menu, const struct menu_item *mi)
{
	char *text;

	switch (mi->type) {
		case MI_TOGGLE:
		case MI_DEFINE_KEY:
			{
				static char str[256];
				snprintf(str, sizeof str, "%s %s", mi->text,
				  mi->get_status(mi->extra_get_status));
				text = str;
			}
			break;

		default:
			text = mi->text ? mi->text : "";
			break;
	}

	return text;
}

static int
menu_item_index(const struct menu *menu, const struct menu_item *mi)
{
	struct list_node *ln;
	int idx = -1, i;

	for (i = 0, ln = menu->items->first; ln; ++i, ln = ln->next) {
		if ((const struct menu_item *)ln->data == mi) {
			idx = i;
			break;
		}
	}

	return idx;
}

static int
menu_item_width(const struct menu *menu, const struct menu_item *mi)
{
	return string_width_in_pixels(menu->fri, menu_item_text(menu, mi));
}

static int
menu_width(const struct menu *menu)
{
	struct list_node *ln;
	int width = 0;

	for (ln = menu->items->first; ln; ln = ln->next) {
		int w = menu_item_width(menu,
		  (const struct menu_item *)ln->data);

		if (w > width)
			width = w;
	}

	return width;
}

static int
menu_num_items(const struct menu *menu)
{
	return menu->items->length;
}

static int
menu_item_is_selectable(const struct menu_item *item)
{
	return item->type != MI_SPACE && item->type != MI_TITLE &&
	  item->visible;
}

static int
menu_item_height(const struct menu_item *item)
{
	const int item_height = item->parent->item_height;

	if (item->type != MI_SPACE)
		return item_height;
	else
		return .333f*item_height;
}

static int
menu_height(const struct menu *menu)
{
	struct list_node *ln;
	int h = 0;

	for (ln = menu->items->first; ln; ln = ln->next)
		h += menu_item_height((const struct menu_item *)ln->data);

	return h;
}

static void
menu_get_position(const struct menu *menu, int *px, int *py)
{
	if (menu->x == -1 || menu->y == -1) {
		*px = .5*(viewport_width - menu_width(menu));
		*py = .5*(viewport_height - menu_height(menu));
	} else {
		*px = menu->x;
		*py = menu->y;
	}
}

static void
menu_render_item(const struct menu *menu, const struct menu_item *mi, int y, int dx)
{
	if (mi->type != MI_SPACE && mi->visible) {
		float scale, left_offset;

		if (mi->type == MI_TITLE) {
			glColor4f(.2, .2, 1, 1);
			scale = 1.16f;
		} else {
			static float neutral[3] = { 1, 1, 1 };
			static float selected[3];

			static float item_scale[2] = { 1, 1.12 };
			const float k = mi->hilite_scale;

			if (menu->defining_key) {
				selected[0] = 1;
				selected[1] = .1;
				selected[2] = .1;
			} else {
				selected[0] = 1;
				selected[1] = 1;
				selected[2] = .1;
			}

			glColor4f((1 - k)*neutral[0] + k*selected[0],
			  (1 - k)*neutral[1] + k*selected[1],
			  (1 - k)*neutral[2] + k*selected[2], 1);

			scale = (1 - k)*item_scale[0] + k*item_scale[1];
		}

		scale *= menu->scale;

		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();

		if (menu->align == MA_CENTER) {
			glTranslatef(.5f*viewport_width, y, 0);
			glScalef(scale, scale, scale);

			if (mi->type == MI_TOGGLE || mi->type == MI_DEFINE_KEY) {
				left_offset = -string_width_in_pixels(menu->fri,
						mi->text);
			} else {
				left_offset = -.5f*menu_item_width(menu, mi);
			}

			glTranslatef(left_offset + dx, 8, 0);
		} else if (menu->align == MA_LEFT) {
			glTranslatef(menu->x + dx, y, 0);
			glScalef(scale, scale, scale);
			glTranslatef(0, 8, 0);
		} else {
			assert(0);
		}

		render_string(menu->fri, 0, 0, 1, menu_item_text(menu, mi));

		glPopMatrix();
	}
}

void
menu_render(const struct menu *menu)
{
	const struct list_node *ln;
	int x, y, i;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.f, viewport_width, viewport_height, 0.f, -1.f, 1.f);

	while (menu->cur_submenu)
		menu = menu->cur_submenu;

	menu_get_position(menu, &x, &y);

	y += 26; /* HACK */

	i = 0;

	for (ln = menu->items->first; ln; ln = ln->next) {
		const struct menu_item *mi = (const struct menu_item *)ln->data;
		menu_render_item(menu, mi, y, menu->spread*(i + 1));
		y += menu_item_height(mi);
		++i;
	}

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

static void
menu_update_item(struct menu_item *item)
{
	const float hilite_delta = .25;

	assert(item->parent);

	if (item->parent->hilite == item) {
		if (item->hilite_scale < 1)
			item->hilite_scale += hilite_delta;
	} else {
		if (item->hilite_scale > 0)
			item->hilite_scale -= hilite_delta;
	}

	if (item->submenu)
		menu_update(item->submenu);
}

void
menu_update(struct menu *menu)
{
	struct list_node *ln;

	for (ln = menu->items->first; ln; ln = ln->next)
		menu_update_item((struct menu_item *)ln->data);
}

static struct menu_item *
menu_item_at(const struct menu *menu, float xl, float yl)
{
	int menu_x, menu_y, height;
	struct menu_item *mi;
	float x, y;

	x = xl*viewport_width/window_width;
	y = yl*viewport_height/window_height;

	menu_get_position(menu, &menu_x, &menu_y);

	mi = NULL;

	height = menu_height(menu);

	if (y >= menu_y && y <= menu_y + height) {
		struct list_node *ln;
		int item_width;
		float base_y, base_x;

		base_y = menu_y;

		for (ln = menu->items->first; ln; ln = ln->next) {
			int item_height;

			mi = (struct menu_item *)ln->data;

			item_height = menu_item_height(mi);

			if (y >= base_y && y <= base_y + item_height)
				break;

			base_y += item_height;
		}

		item_width = menu_item_width(menu, mi);

		if (menu->align == MA_CENTER) {
			if (mi->type == MI_TOGGLE || mi->type == MI_DEFINE_KEY) {
				base_x = .5f*viewport_width -
					menu->scale*
					string_width_in_pixels(menu->fri, mi->text);
			} else {
				base_x = .5f*(viewport_width - menu->scale*item_width);
			}
		} else {
			base_x = menu_x;
		}

		if (x < base_x || x > base_x + item_width)
			mi = NULL;
	}

	return mi;
}

static void
menu_set_hilite(struct menu *menu, struct menu_item *mi)
{
	menu->hilite = mi;
	play_fx(FX_SHOT_1);
}

int
menu_on_mouse_motion(struct menu *menu, int x, int y)
{
	struct menu_item *prev, *next;

	while (menu->cur_submenu)
		menu = menu->cur_submenu;

	prev = menu->hilite;

	next = menu_item_at(menu, x, y);

	if (next && next != prev && menu_item_is_selectable(next)) {
		menu_set_hilite(menu, next);

		if (menu->defining_key && next->type != MI_DEFINE_KEY)
			menu->defining_key = 0;
	}

	return menu->hilite != prev;
}

static void
menu_item_select(struct menu_item *mi)
{
	struct menu_item *t;

	switch (mi->type) {
		case MI_SUBMENU:
			mi->parent->cur_submenu = mi->submenu;
			t = menu_first_selectable_item(mi->submenu);
			t->hilite_scale = 1;
			mi->submenu->hilite = t;
			if (mi->submenu->on_activate)
				mi->submenu->on_activate(mi->submenu);
			break;

		case MI_DEFINE_KEY:
			mi->parent->defining_key = 1;
			break;

		default:
			mi->on_action(mi->extra_on_action);
			break;
	}

	play_fx(FX_SHOT_2);
}

int
menu_on_mouse_click(struct menu *menu, int x, int y)
{
	struct menu_item *mi;

	while (menu->cur_submenu)
		menu = menu->cur_submenu;

	if ((mi = menu_item_at(menu, x, y)) == NULL ||
		  !menu_item_is_selectable(mi)) {
		return 0;
	} else {
		menu_item_select(mi);
		menu_on_mouse_motion(menu, x, y);
		return 1;
	}
}

static struct menu_item *
menu_first_selectable_item(struct menu *menu)
{
	struct list_node *ln;

	for (ln = menu->items->first; ln; ln = ln->next) {
		struct menu_item *mi = (struct menu_item *)ln->data;

		if (menu_item_is_selectable(mi))
			return mi;
	}

	return NULL;
}

static void
menu_on_arrow_down(struct menu *menu)
{
	if (menu->hilite == NULL) {
		menu_set_hilite(menu, menu_first_selectable_item(menu));
	} else {
		struct menu_item *mi;

		int index = menu_item_index(menu, menu->hilite);

		do {
			if (++index >= menu->items->length)
				index = 0;

			mi = (struct menu_item *)
				list_element_at(menu->items, index);
		} while (!menu_item_is_selectable(mi));

		menu_set_hilite(menu, mi);
	}
}

static void
menu_on_arrow_up(struct menu *menu)
{
	if (menu->hilite == NULL) {
		menu_set_hilite(menu, menu_first_selectable_item(menu));
	} else {
		struct menu_item *mi;

		int index = menu_item_index(menu, menu->hilite);

		do {
			if (--index < 0)
				index = menu->items->length - 1;

			mi = (struct menu_item *)
				list_element_at(menu->items, index);
		} while (!menu_item_is_selectable(mi));

		menu_set_hilite(menu, mi);
	}
}

static void
menu_on_enter(struct menu *menu)
{
	struct menu_item *mi = menu->hilite;

	if (mi) {
		assert(menu_item_is_selectable(mi));
		menu_item_select(mi);
	}
}

void
menu_on_key_pressed(struct menu *menu, int key)
{
	while (menu->cur_submenu)
		menu = menu->cur_submenu;

	if (menu->defining_key) {
		const struct menu_item *mi = menu->hilite;

		assert(mi->type == MI_DEFINE_KEY);

		if (mi->set_key(key, mi->extra_set_key))
			menu->defining_key = 0;
	} else {
		switch (key) {
			case SDLK_UP:
				menu_on_arrow_up(menu);
				break;

			case SDLK_DOWN:
				menu_on_arrow_down(menu);
				break;

			case SDLK_RETURN:
			case SDLK_KP_ENTER:
				menu_on_enter(menu);
				break;
		}
	}
}

void
menu_on_stick_pressed(struct menu *menu, int stick)
{
	while (menu->cur_submenu)
		menu = menu->cur_submenu;

	if (menu->defining_key) {
		const struct menu_item *mi = menu->hilite;

		assert(mi->type == MI_DEFINE_KEY);

		if (mi->set_stick(stick, mi->extra_set_stick))
			menu->defining_key = 0;
	} else {
		if (stick == settings.pad_sticks[0]) {
			menu_on_arrow_up(menu);
		} else if (stick == settings.pad_sticks[1]) {
			menu_on_arrow_down(menu);
		} else if (stick == settings.pad_sticks[4] ||
		  stick == settings.pad_sticks[5] ||
		  stick == settings.pad_sticks[6] ||
		  stick == settings.pad_sticks[7]) {
			menu_on_enter(menu);
		}
	}
}

void
menu_on_hide(struct menu *menu)
{
}

void
menu_on_show(struct menu *menu)
{
}

void
menu_reset(struct menu *menu)
{
	menu->cur_submenu = NULL;
	menu_on_show(menu);
}
