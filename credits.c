#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <GL/gl.h>

#include "arena.h"
#include "font_render.h"
#include "panic.h"
#include "common.h"
#include "in_game.h"
#include "water.h"
#include "background.h"
#include "particle.h"
#include "credits.h"

static struct credits {
	const char *name;
	const char *credits;
} credits[] = {
	{ "Cristina Saito", "Project Management, Design" },
	{ "Mauro Persano", "Programming, Design, Graphics, Sounds" },
	{ "Rafael Coelho", "Design, Graphics" },
	{ "Oz", "Music" },
	{ NULL, NULL },
};

static int tics;

#define PARTICLE_SPEED .2f

static void
gen_explosion(float x, float y)
{
	int i, n, c;
	particle *p;

	n = 150 + irand(150);
	c = irand(NUM_PALETTES);

	for (i = 0; i < n; i++) {
		vector2 offs;

		p = add_particle();

		if (!p)
			break;

		p->ttl = 20 + irand(20);
		p->t = 0;
		p->width = .3f*(.6f + .1f*frand());
		p->height = .3f*(2.8f + .4f*frand());
		p->pos.x = x;
		p->pos.y = y;
		p->palette = c;

		vec2_set(&p->dir, frand() - .5f, frand() - .5f);
		vec2_normalize(&p->dir);

		offs = p->dir;
		vec2_mul_scalar(&offs, 1.3f*frand());
		vec2_add_to(&p->pos, &offs);

		p->speed = PARTICLE_SPEED + .05f*frand();
		p->friction = .96f;
	}
}

void
reset_credits_state(void)
{
	tics = 0;
	cur_arena = NULL; /* to prevent particles from bouncing */
	reset_particles();
}

void
initialize_credits(void)
{
}

static void
update(struct game_state *gs)
{
	update_particles();
	update_background();

	if (irand(18) == 0)
		gen_explosion(10.f*(frand() - .5f), 10.f*(frand() - .5f));

	++tics;
}

static void
draw_credits(void)
{
	struct credits *p;
	const struct font_render_info *fri = font_medium;
	const int viewport_width = 800;
	const int viewport_height = 600;
	const char *title = "THE PROTOZOA TEAM";
	const float title_scale = 1.6;
	int y, offs, t;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.f, viewport_width, viewport_height, 0.f, -1.f, 1.f);

	glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

	glColor3f(1, 1, 1);

	glPushMatrix();
	glTranslatef(.5f*viewport_width, 40, 0);
	glScalef(title_scale, title_scale, title_scale);
	glTranslatef(-.5f*string_width_in_pixels(fri, "%s", title), 0, 0);
	render_string(fri, 0, 0, 1, "%s", title);
	glPopMatrix();
	glPushMatrix();

	y = 120;
	offs = 800;
	t = 20*tics;

	for (p = credits; p->name; p++) {
		static const float name_scale = 1.2;
		static const float credits_scale = .8;
		int d;

		if (offs < t)
			d = 0;
		else 
			d = offs - t;

		glColor3f(1, 1, 1);

		glPushMatrix();
		glTranslatef(20 - d, y, 0);
		glScalef(name_scale, name_scale, name_scale);
		render_string(fri, 0, 0, 1, "%s", p->name);
		glPopMatrix();
		glPushMatrix();

		offs += 300;

		if (offs < t)
			d = 0;
		else 
			d = offs - t;

		glColor3f(.8, .8, 1.);

		glTranslatef(60 - d, y + 40, 0);
		glScalef(credits_scale, credits_scale, credits_scale);
		render_string(fri, 0, 0, 1, "%s", p->credits);
		glPopMatrix();

		y += 100;

		offs += 300;
	}

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);

	glPopAttrib();

}

static void
draw(struct game_state *gs)
{
	glPushMatrix();
	glTranslatef(0, 0, -12);
	draw_skybox();
	draw_particles();
	glPopMatrix();

	draw_credits();
}

static void
on_mouse_motion(struct game_state *gs, int x, int y)
{
}

static void
on_mouse_button_pressed(struct game_state *gs, int x, int y)
{
	start_main_menu();
}

static void
on_mouse_button_released(struct game_state *gs, int x, int y)
{
}

static void
on_stick_pressed(struct game_state *gs, int stick)
{
	start_main_menu();
}

static void
on_key_pressed(struct game_state *gs, int key)
{
	start_main_menu();
}

struct game_state credits_state =
  { update, draw,
    on_mouse_motion, on_mouse_button_pressed, on_mouse_button_released,
    on_key_pressed,
    on_stick_pressed };
