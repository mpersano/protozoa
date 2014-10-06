#include <stdio.h>
#include <string.h>
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
#include "in_game_text.h"
#include "powerup.h"

#define PU_RADIUS .4f
#define PU_SPEED .05f

enum {
	MAX_POWERUPS = 6,
	PU_NAME_TTL = 30
};

static int powerup_texture_ids[NUM_POWERUP_TYPES];

struct powerup_settings *powerup_settings[NUM_POWERUP_TYPES];

struct powerup {
	int used;
	int type;
	vector2 pos;
	vector2 dir;
	float angle;
	int ttl;
};

static struct powerup powerups[MAX_POWERUPS];

void
reset_powerups(void)
{
	struct powerup *p;

	for (p = powerups; p != &powerups[MAX_POWERUPS]; p++)
		p->used = 0;
}

static void
draw_powerup(const struct powerup *p)
{
	struct sprite_settings *s;
	float a, r, g, b, w, h, t;

	s = powerup_settings[p->type]->sprite;

	glBindTexture(GL_TEXTURE_2D, gl_texture_id(powerup_texture_ids[p->type]));

	glPushMatrix();

	glTranslatef(p->pos.x, p->pos.y, 0.f);

#define POWERUP_EXPIRING_TICS 100

	t = p->ttl < POWERUP_EXPIRING_TICS ?
	  (float)p->ttl/POWERUP_EXPIRING_TICS : 1;

	r = s->color->r;
	g = s->color->g;
	b = s->color->b;
	a = t*s->color->a;

	glColor4f(r, g, b, a);

	w = .5f*s->width;
	h = .5f*s->height;

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
}

static void
update_powerup(struct powerup *p)
{
	if (--p->ttl <= 0) {
		p->used = 0;
	} else if (ship.is_alive &&
	    vec2_distance_squared(&p->pos, &ship.pos) <=
			(PU_RADIUS + SHIP_RADIUS)*(PU_RADIUS + SHIP_RADIUS)) {
		static char str[300], *q;
		const struct text_settings *text =
			powerup_settings[p->type]->text;

		strncpy(str, text->text, sizeof str - 1);

		for (q = str; *q; ++q) {
			if (*q == ' ')
				*q = '\n';
		}

		ship_add_powerup(p->type);
		add_in_game_text(&p->pos, text->color,
				text->scale, text->ttl, "%s", str);
		p->used = 0;
	} else {
		vector2 normal;
		vector2 pos = p->pos;
		vector2 *dir = &p->dir;

		vec2_add_to(&pos, dir);

		if (bounces_with_arena(&pos, PU_RADIUS, &normal)) {
			float k = 2.f*(normal.x*dir->x + normal.y*dir->y);

			dir->x = -(k*normal.x - dir->x);
			dir->y = -(k*normal.y - dir->y);

			pos = p->pos;
			vec2_add_to(&pos, dir);
		}

		p->pos = pos;

		p->angle += 1.f;
	}
}

void
draw_powerups(void)
{
	struct powerup *p;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);

	for (p = powerups; p != &powerups[MAX_POWERUPS]; p++) {
		if (p->used)
			draw_powerup(p);
	}

	glPopAttrib();
}

void
update_powerups(void)
{
	struct powerup *p;

	for (p = powerups; p != &powerups[MAX_POWERUPS]; p++) {
		if (p->used)
			update_powerup(p);
	}
}

void
initialize_powerups(void)
{
	int i;

	for (i = 0; i < NUM_POWERUP_TYPES; i++) {
		const struct powerup_settings *p = powerup_settings[i];

		if (p == NULL)
			panic("powerup %d not properly initialized!", i);

		powerup_texture_ids[i] =
		  png_to_texture(p->sprite->source);
	}
}

void
add_powerup(const vector2 *origin, int type)
{
	struct powerup *p = NULL;

	for (p = powerups; p != &powerups[MAX_POWERUPS]; p++) {
		if (!p->used)
			break;
	}

	if (p != &powerups[MAX_POWERUPS]) {
		p->used = 1;
		p->pos = *origin;
		p->dir.x = frand() -.5f;
		p->dir.y = frand() - .5f;
		vec2_mul_scalar(&p->dir, PU_SPEED/vec2_length(&p->dir));
		p->type = type;
		p->ttl = powerup_settings[type]->ttl;
	}
}
