#include <stdio.h>
#include <GL/gl.h>

#include "common.h"
#include "menu.h"
#include "font.h"
#include "background.h"
#include "gl_util.h"
#include "settings_menu.h"
#include "highscores.h"
#include "main_menu.h"

enum {
	TICS_BEFORE_PLAYBACK = 300,
	LOGO_FADE_IN_TICS = 40,
	START_HIGHSCORE_SPREAD = 30
};

static float eye_z;
static int tics, idle_tics;
static struct menu *main_menu;
static int logo_texture_id;
static int highscore_spread;

void
reset_main_menu_state(void)
{
	tics = idle_tics = 0;
	highscore_spread = START_HIGHSCORE_SPREAD;
	eye_z = 80.f;
	reset_background();
	main_menu->spread = -140;
}

static void
menu_play(void *extra)
{
	menu_on_hide(main_menu);
	start_game();
}

static void
menu_credits(void *extra)
{
	menu_on_hide(main_menu);
	start_credits();
}

static void
menu_quit(void *extra)
{
	running = 0;
}

void
initialize_main_menu(void)
{
	main_menu = menu_make("main", font_medium, 1.05, MA_LEFT, 20, 160);

	menu_add_action_item(main_menu, "Play", menu_play, NULL);
	menu_add_action_item(main_menu, "Credits", menu_credits, NULL);
	add_settings_menu(main_menu);
	menu_add_space_item(main_menu);
	menu_add_action_item(main_menu, "Quit", menu_quit, NULL);

	logo_texture_id = png_to_texture("intro-logo.png");
}

static int
menu_is_visible(void)
{
	return tics > LOGO_FADE_IN_TICS;
}

static void
main_menu_background_update(void)
{
	update_background();

	if (eye_z > 50.f)
		eye_z -= .2f;

	++tics;

	if (menu_is_visible()) {
		if (highscore_spread > 0)
			highscore_spread--;

		if (menu_is_on_top_level(main_menu)) {
			if (++idle_tics > TICS_BEFORE_PLAYBACK)
				start_playback();
		}

		if (main_menu->spread < 0)
			main_menu->spread += 10.f;
	}
}

static void
update(struct game_state *gs)
{
	main_menu_background_update();
	menu_update(main_menu);
}

static void
draw_logo(void)
{
	float w, h;
	float s;

	w = h = 10.f;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, gl_texture_id(logo_texture_id));
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glPushMatrix();
	glTranslatef(0, -12, -40.f);

	if (tics > LOGO_FADE_IN_TICS)
		s = 1;
	else
		s = (float)tics/LOGO_FADE_IN_TICS;

	glColor4f(1, 1, 1, s);

	glBegin(GL_QUADS);

	glTexCoord2f(0, 1);
	glVertex3f(-w, -h, 0.f);

	glTexCoord2f(1, 1);
	glVertex3f(w, -h, 0.f);

	glTexCoord2f(1, 0);
	glVertex3f(w, h, 0.f);

	glTexCoord2f(0, 0);
	glVertex3f(-w, h, 0.f);

	glEnd();

	glPopMatrix();

	glPopAttrib();
}

const GLfloat light_ambient[] = { 0.1f, 0.1f, 0.1f, 1.f };
const GLfloat light_diffuse[] = { .8f, .8f, .8f, 1.f };
const GLfloat light_specular[] = { .8f, .8f, .8f, 1.f };
/* const GLfloat light_pos[] = { -2000, 1000, 1000, 0 }; */
const GLfloat light_pos[] = { -2000, 1500, 1000, 0 };

static void
main_menu_background_draw(void)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(20, -5, -eye_z);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_POSITION, (GLfloat *)&light_pos);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
        glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);
        glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);

	draw_background(0, 0, 0);

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (menu_is_on_top_level(main_menu)) {
		draw_logo();

		if (menu_is_visible()) {
			draw_highscore_table(380, 20 +
			  (float)highscore_spread*(400 - 20)/
			    START_HIGHSCORE_SPREAD, highscore_spread);
		}
	}

	glPopAttrib();
}

static void
draw(struct game_state *gs)
{
	main_menu_background_draw();

	if (menu_is_visible())
		menu_render(main_menu);
}

static void
on_mouse_motion(struct game_state *gs, int x, int y)
{
	idle_tics = 0;

	if (menu_is_visible())
		menu_on_mouse_motion(main_menu, x, y);
}

static void
on_mouse_button_pressed(struct game_state *gs, int x, int y)
{
	idle_tics = 0;

	if (menu_is_visible())
		menu_on_mouse_click(main_menu, x, y);
}

static void
on_mouse_button_released(struct game_state *gs, int x, int y)
{
	idle_tics = 0;
}

static void
on_key_pressed(struct game_state *gs, int key)
{
	idle_tics = 0;

	if (menu_is_visible())
		menu_on_key_pressed(main_menu, key);
}

static void
on_stick_pressed(struct game_state *gs, int stick)
{
	idle_tics = 0;
	menu_on_stick_pressed(main_menu, stick);
}

struct game_state main_menu_state =
  { update, draw,
    on_mouse_motion, on_mouse_button_pressed, on_mouse_button_released,
    on_key_pressed,
    on_stick_pressed };
