#include <stdio.h>
#include <SDL.h>
#include <GL/gl.h>

#include "audio.h"
#include "common.h"
#include "menu.h"
#include "font.h"
#include "background.h"
#include "settings_menu.h"
#include "in_game.h"
#include "in_game_menu.h"

static struct menu *in_game_menu;

void
reset_in_game_menu_state(void)
{
}

static void
menu_quit(void *extra)
{
	running = 0;
}

static void
menu_end_game(void *extra)
{
	fade_music();
	start_main_menu();
}

static void
menu_continue(void *extra)
{
	SDL_ShowCursor(SDL_DISABLE);
	continue_game();
}

void
initialize_in_game_menu(void)
{
	in_game_menu = menu_make("in_game", font_medium, 1, MA_CENTER, -1, -1);

	menu_add_action_item(in_game_menu, "QUIT", menu_quit, NULL);
	add_settings_menu(in_game_menu);
	menu_add_action_item(in_game_menu, "END GAME", menu_end_game, NULL);
	menu_add_action_item(in_game_menu, "CONTINUE", menu_continue, NULL);
}

static void
update(struct game_state *gs)
{
	menu_update(in_game_menu);
}

enum {
	MENU_WINDOW_BORDER = 40
};

static void
menu_render_background(void)
{
	const int viewport_width = 800;
	const int viewport_height = 600;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.f, viewport_width, viewport_height, 0.f, -1.f, 1.f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glColor4f(.1f, .2f, .2f, .6f);

	glBegin(GL_QUADS);
	glVertex2f(MENU_WINDOW_BORDER, 0);
	glVertex2f(viewport_width - MENU_WINDOW_BORDER, 0);
	glVertex2f(viewport_width - MENU_WINDOW_BORDER, viewport_height);
	glVertex2f(MENU_WINDOW_BORDER, viewport_height);
	glEnd();

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glPopAttrib();
}

static void
draw(struct game_state *gs)
{
	in_game_state.draw(&in_game_state);
	menu_render_background();
	menu_render(in_game_menu);
}

static void
on_mouse_motion(struct game_state *gs, int x, int y)
{
	menu_on_mouse_motion(in_game_menu, x, y);
}

static void
on_mouse_button_pressed(struct game_state *gs, int x, int y)
{
	menu_on_mouse_click(in_game_menu, x, y);
}

static void
on_mouse_button_released(struct game_state *gs, int x, int y)
{
}

static void
on_key_pressed(struct game_state *gs, int key)
{
	menu_on_key_pressed(in_game_menu, key);
}

static void
on_stick_pressed(struct game_state *gs, int stick)
{
	menu_on_stick_pressed(in_game_menu, stick);
}

struct game_state in_game_menu_state =
  { update, draw,
    on_mouse_motion, on_mouse_button_pressed, on_mouse_button_released,
    on_key_pressed,
    on_stick_pressed };
