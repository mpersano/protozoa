#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <GL/gl.h>

#include "vector.h"
#include "arena.h"
#include "in_game.h"
#include "foe.h"
#include "gl_util.h"
#include "laser.h"

#define MAX_LASER_LENGTH 4.f

enum {
	MAX_LASER_SEGS = 30
};

struct laser {
	int used;
	vector2 pos;
	vector2 dir;
	int num_segs;
	vector2 segments[MAX_LASER_SEGS];
	int ttl;
};

static struct laser lasers[MAX_LASERS];

static int laser_seg_texture_id;
static int laser_bounce_texture_id;

void
reset_lasers(void)
{
	struct laser *p;

	for (p = lasers; p != &lasers[MAX_LASERS]; p++)
		p->used = 0;
}

#define LR .4f

static float
draw_laser_seg(const vector2 *from, const vector2 *to, float length_left)
{
	vector2 d, o, e;
	float l, t;
	float lr = LR + .1f*LR*sin(.5f*gc.level_tics);

	vec2_sub(&d, from, to);
	l = vec2_length(&d);

	if (l < length_left)
		t = 1.f;
	else
		t = length_left/l;

	o.x = -d.y;
	o.y = d.x;
	vec2_mul_scalar(&o, lr/vec2_length(&o));

	e.x = from->x + t*(to->x - from->x);
	e.y = from->y + t*(to->y - from->y);

	glBindTexture(GL_TEXTURE_2D, gl_texture_id(laser_seg_texture_id));

	glBegin(GL_QUADS);

	glTexCoord2f(0, 1);
	glVertex2f(from->x + o.x, from->y + o.y);

	glTexCoord2f(0, 1);
	glVertex2f(e.x + o.x, e.y + o.y);

	glTexCoord2f(1, 1);
	glVertex2f(e.x - o.x, e.y - o.y);

	glTexCoord2f(1, 0);
	glVertex2f(from->x - o.x, from->y - o.y);
	
	glEnd();

	return l;
}

static void
draw_laser_bounce(const vector2 *u)
{
	float br = 1.5f*LR + .5f*LR*sin(.5f*gc.level_tics);

	glBindTexture(GL_TEXTURE_2D, gl_texture_id(laser_bounce_texture_id));

	glBegin(GL_QUADS);

	glTexCoord2f(0, 0);
	glVertex2f(u->x - br, u->y - br);

	glTexCoord2f(0, 1);
	glVertex2f(u->x + br, u->y - br);

	glTexCoord2f(1, 1);
	glVertex2f(u->x + br, u->y + br);

	glTexCoord2f(1, 0);
	glVertex2f(u->x - br, u->y + br);

	glEnd();
}

static void
draw_laser(const struct laser *p)
{
	const vector2 *u;
	float left, l, t;

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_FLAT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

#define ETL 5

	if (p->ttl < ETL)
		t = (float)p->ttl/ETL;
	else
		t = 1;

	glColor4f(t, t, t, t);

	left = MAX_LASER_LENGTH;

	l = draw_laser_seg(&p->pos, &p->segments[p->num_segs - 1], left);

	if (l < left) {
		left -= l;

		for (u = &p->segments[p->num_segs - 1];
		  u >= &p->segments[1]; u--) {
			draw_laser_bounce(u);

			l = draw_laser_seg(&u[0], &u[-1], left);

			if (l > left)
				break;

			left -= l;
		}
	}

	glPopAttrib();
}

void
draw_lasers(void)
{
	const struct laser *p;

	for (p = lasers; p != &lasers[MAX_LASERS]; p++) {
		if (p->used)
			draw_laser(p);
	}
}

static float
laser_seg_hit(const vector2 *from, const vector2 *to, float length_left)
{
	vector2 d;
	float l, t;
	int i;

	vec2_sub(&d, from, to);
	l = vec2_length(&d);

	if (l < length_left)
		t = 1.f;
	else
		t = length_left/l;

	/* HACK HACK HACK */

#define NUM_TEST_SEGS 5

	for (i = 0; i < NUM_TEST_SEGS; i++) {
		float f = (float)i/(NUM_TEST_SEGS - 1);
		vector2 e;

		e.x = from->x + t*f*(to->x - from->x);
		e.y = from->y + t*f*(to->y - from->y);

		hit_foes(&e);
	}

	return l;
}

static void
update_laser(struct laser *p)
{
	vector2 pos, normal;
	if (--p->ttl <= 0) {
		p->used = 0;
	} else {
		float left = MAX_LASER_LENGTH;
		float l;

		vec2_add(&pos, &p->pos, &p->dir);

		if (bounces_with_arena(&pos, 0, &normal)) {
			if (p->num_segs >= MAX_LASER_SEGS) {
				p->used = 0;
			} else {
				vector2 *d = &p->dir;
				float k = 2.f*(normal.x*d->x + normal.y*d->y);
				d->x = -(k*normal.x - d->x);
				d->y = -(k*normal.y - d->y);

				p->segments[p->num_segs].x =
				  .5f*(p->pos.x + pos.x);
				p->segments[p->num_segs].y =
				  .5f*(p->pos.y + pos.y);
				p->num_segs++;

				vec2_add(&pos, &p->pos, &p->dir);
			}
		}

		p->pos = pos;

		l = laser_seg_hit(&p->pos, &p->segments[p->num_segs - 1], left);

		if (l < left) {
			vector2 *u;

			left -= l;

			for (u = &p->segments[p->num_segs - 1];
			  u >= &p->segments[1]; u--) {
				l = laser_seg_hit(&u[0], &u[-1], left);

				if (l > left)
					break;

				left -= l;
			}
		}
	}
}

void
update_lasers(void)
{
	struct laser *p;

	for (p = lasers; p != &lasers[MAX_LASERS]; p++) {
		if (p->used)
			update_laser(p);
	}
}

int
add_laser(const vector2 *origin, const vector2 *direction)
{
	struct laser *p;
	int r;

	r = 0;

	for (p = lasers; p != &lasers[MAX_LASERS]; p++) {
		if (!p->used) {
			r = 1;
			break;
		}
	}

	if (r) {
		p->used = 1;
		p->pos = p->segments[0] = *origin;
		p->num_segs = 1;
		p->dir = *direction;
		vec2_mul_scalar(&p->dir, .4f/vec2_length(&p->dir));
		p->ttl = 100;
	}

	return r;
}

void
initialize_lasers(void)
{
	laser_seg_texture_id = png_to_texture("laser-segment.png");
	laser_bounce_texture_id = png_to_texture("laser-bounce.png");
}
