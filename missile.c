#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <GL/gl.h>

#include "arena.h"
#include "foe.h"
#include "particle.h"
#include "panic.h"
#include "common.h"
#include "powerup.h"
#include "ship.h"
#include "image.h"
#include "gl_util.h"
#include "foe.h"
#include "settings.h"
#include "in_game.h"
#include "missile.h"

struct missile_settings *missile_settings[NUM_MISSILE_TYPES];

#define SMOOTH_HOMING_MISSILE_EXPIRE
enum {
	MAX_MISSILES = 100,
	MAX_HOMING_MISSILE_TRAILS = 1024,
	HOMING_MISSILE_TRAIL_SEGS = 15,
	MAX_HOMING_MISSILES = 8
};

struct missile {
	int used;
	enum missile_type type;
	vector2 pos;
	vector2 dir;
	float angle;
	int is_bouncy;
	const struct foe_common *target; 		/* if homing missile */
	struct homing_missile_trail *trail_head;	/* if homing missile */
	int start_seg;					/* if homing missile */
#ifdef SMOOTH_HOMING_MISSILE_EXPIRE
	int expiring;					/* if homing missile */
#endif
};

struct homing_missile_trail {
	int used;
	vector2 pos;
	struct homing_missile_trail *next;
};

static struct missile missiles[MAX_MISSILES];
static struct homing_missile_trail
  homing_missile_trails[MAX_HOMING_MISSILE_TRAILS];

static int homing_missile_count;

static int missile_texture_ids[NUM_MISSILE_TYPES];

void
reset_missiles(void)
{
	int i;

	for (i = 0; i < MAX_MISSILES; i++)
		missiles[i].used = 0;

	for (i = 0; i < MAX_HOMING_MISSILE_TRAILS; i++)
		homing_missile_trails[i].used = 0;

	homing_missile_count = 0;
}

#ifdef MISSILE_TRAIL
static void
add_missile_trail(const vector2 *pos, const vector2 *prev_pos)
{
	const int nparticles = irand(3);
	int i;
	vector2 d = *pos, tp = *prev_pos;
	vec2_mul_scalar(&d, 1.02f);
	vec2_sub_from(&d, prev_pos);
	vec2_sub_from(&tp, &d);

	for (i = 0; i < nparticles; i++) {
		particle *p = add_particle();

		if (!p)
			break;

		p->ttl = 5 + irand(5);
		p->t = 0;
		p->palette = PAL_BLUE;
		p->width = p->height = .4f*(.3f + .2f*frand());
		p->pos.x = tp.x + .2f*frand() - .1f;
		p->pos.y = tp.y + .2f*frand() - .1f;
		vec2_set(&p->dir, 1, 0);
		p->speed = 0.f;
		p->friction = .9f;
	}
}
#endif

static void
draw_missile(const struct missile *m)
{
	float w, h;
	struct sprite_settings *s;
	struct rgba *rgba;

	s = missile_settings[m->type]->sprite;

	glPushMatrix();
	glTranslatef(m->pos.x, m->pos.y, 0.f);
	glRotatef(m->angle, 0, 0, 1.f);

	glBindTexture(GL_TEXTURE_2D, gl_texture_id(missile_texture_ids[m->type]));

	w = .5f*s->width;
	h = .5f*s->height;

	rgba = s->color;

	glColor4f(rgba->r, rgba->g, rgba->b, rgba->a);

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
add_homing_missile_particles(const vector2 *pos, const vector2 *prev_pos)
{
	const int nparticles = irand(7);
	int i;
	vector2 d = *pos, tp = *prev_pos;
	vec2_sub_from(&d, prev_pos);
	vec2_mul_scalar(&d, 1.52f);
	vec2_sub_from(&tp, &d);

	for (i = 0; i < nparticles; i++) {
		particle *p = add_particle();

		if (!p)
			break;

		p->ttl = 10 + irand(10);
		p->t = 0;
		p->palette = PAL_BLUE;
		p->width = p->height = .3f + .2f*frand();
		p->pos.x = tp.x + .2f*frand() - .1f;
		p->pos.y = tp.y + .2f*frand() - .1f;
		vec2_set(&p->dir, 1, 0);
		p->speed = 0.f;
		p->friction = .9f;
	}
}

static void
draw_homing_missile_trail(const struct homing_missile_trail *head,
	int start_seg)
{
	vector2 d = { 0 }; /* shut up gcc */
	float u, du;
	const struct homing_missile_trail *p;
	struct sprite_settings *s;
	struct rgba *rgba;

	if (!head || !head->next)
		return;

	s = missile_settings[MT_HOMING]->sprite;

	glBindTexture(GL_TEXTURE_2D, gl_texture_id(missile_texture_ids[MT_HOMING]));

	rgba = missile_settings[MT_HOMING]->sprite->color;
	glColor4f(rgba->r, rgba->g, rgba->b, rgba->a);

	glBegin(GL_TRIANGLE_STRIP);

	du = 1.f/HOMING_MISSILE_TRAIL_SEGS;
	u = start_seg*du;

	for (p = head; p && p->next; p = p->next) { 
		const vector2 *from = &p->pos;
		const vector2 *to = &p->next->pos;

		d.x = -(to->y - from->y);
		d.y = to->x - from->x;

		if (p->next->next) {
			const vector2 *next_to = &p->next->next->pos;
			d.x += -(next_to->y - to->y);
			d.y += next_to->x - to->x;
		}

		vec2_mul_scalar(&d, .5f*s->width/vec2_length(&d));

		glTexCoord2f(0, u);
		glVertex3f(from->x - d.x, from->y - d.y, 0.f);

		glTexCoord2f(1, u);
		glVertex3f(from->x + d.x, from->y + d.y, 0.f);

		u += du;
	}

	if (p) {
		glTexCoord2f(0, u);
		glVertex3f(p->pos.x - d.x, p->pos.y - d.y, 0.f);

		glTexCoord2f(1, u);
		glVertex3f(p->pos.x + d.x, p->pos.y + d.y, 0.f);
	}

	glEnd();
}

#define SPARK_SPEED .25f

static void
add_sparks(const vector2 *pos, const vector2 *normal, int type)
{
	const int nparticles = 5 + irand(5);
	vector2 ndir;
	int i;

	vec2_set(&ndir, -normal->y, normal->x);

	for (i = 0; i < nparticles; i++) {
		particle *p = add_particle();

		if (!p)
			break;

		p->ttl = 5 + irand(5);
		p->t = 0;
		p->width = .2f*(.2f + .1f*frand());
		p->height = .2f*(1.8f + .4f*frand());

		p->palette = type == MT_NORMAL ? PAL_BLUE : PAL_ORANGE;

		p->dir.x = -normal->x + (2.5f*frand() - 1.25f)*ndir.x;
		p->dir.y = -normal->y + (2.5f*frand() - 1.25f)*ndir.y;

		vec2_normalize(&p->dir);

		p->speed = SPARK_SPEED + .02f*frand();

		p->pos.x = pos->x - .15f*normal->x + p->speed*p->dir.x;
		p->pos.y = pos->y - .15f*normal->y + p->speed*p->dir.y;

		p->friction = .9f;
	}
}

static struct homing_missile_trail *
add_homing_missile_trail(const vector2 *pos, struct homing_missile_trail *next)
{
	struct homing_missile_trail *p = NULL;
	int i;

	for (i = 0; i < MAX_HOMING_MISSILE_TRAILS; i++) {
		if (!homing_missile_trails[i].used) {
			p = &homing_missile_trails[i];
			break;
		}
	}

	if (p) {
		p->used = 1;
		p->pos = *pos;
		p->next = next;
	}

	return p;
}

static void
release_missile(struct missile *m)
{
	if (m->type == MT_HOMING) {
		--homing_missile_count;

#ifdef SMOOTH_HOMING_MISSILE_EXPIRE
		m->expiring = 1;
#else
		for (p = m->trail_head; p; p = p->next)
			p->used = 0;

		m->used = 0;
#endif
	} else {
		m->used = 0;
	}
}

static void
update_expiring_missile(struct missile *m)
{
	struct homing_missile_trail *p, *q;
	int n;

	++m->start_seg;

	n = m->start_seg;

	for (q = NULL, p = m->trail_head; p->next; q = p, p = p->next)
		++n;

	assert(n <= HOMING_MISSILE_TRAIL_SEGS);

	if (n == HOMING_MISSILE_TRAIL_SEGS) {
		assert(p->next == NULL);

		if (q == NULL) {
			p->used = 0;
			m->used = 0;
		} else {
			q->next->used = 0;
			q->next = NULL;
		}
	}
}

static void
update_missile(struct missile *m)
{
	vector2 normal;
	vector2 pos = m->pos;

	vec2_add_to(&pos, &m->dir);

	if (m->type == MT_HOMING) {
		struct homing_missile_trail *trail = 
		  add_homing_missile_trail(&pos, m->trail_head);

		if (trail) {
			struct homing_missile_trail *p;
			int i;

			m->trail_head = trail;

			p = m->trail_head;

			for (i = 0; i < HOMING_MISSILE_TRAIL_SEGS - 1; i++) {
				if (!(p = p->next))
					break;
			}

			if (p && p->next) {
				p->next->used = 0;
				p->next = NULL;
			}
		}

		add_homing_missile_particles(&pos, &m->pos);
	} else {
#ifdef MISSILE_TRAIL
		add_missile_trail(&pos, &m->pos);
#endif
	}

	m->pos = pos;

	if (bounces_with_arena(&pos, .15f, &normal)) {
		add_sparks(&pos, &normal, m->type);

		if (!m->is_bouncy || !ship_has_powerup(PU_BOUNCY_SHOTS)) {
			game_stat_counters.missiles_missed++;
			level_stat_counters.missiles_missed++;
			release_missile(m);
		} else {
			vector2 *dir = &m->dir;
			float k = 2.f*(normal.x*dir->x + normal.y*dir->y);
			dir->x = -(k*normal.x - dir->x);
			dir->y = -(k*normal.y - dir->y);
			m->angle = 360.f*atan2(-m->dir.x, m->dir.y)/2.f/M_PI;
		}
	} else if (hit_foes(&pos)) {
		if (m->type == MT_NORMAL)
			release_missile(m);
	} else {
		if (m->type == MT_HOMING) {
			vector2 d;
			float r;

			/* follow target */

			if (!m->target || !m->target->used)
				m->target = get_random_foe();

			if (m->target) {
				d.x = -(m->target->pos.y - pos.y);
				d.y = m->target->pos.x - pos.x;
				vec2_normalize(&d);

				r = 1.6f*vec2_dot_product(&d, &m->dir);

#define MAX_TURN_ANGLE .18f
				if (r < -MAX_TURN_ANGLE)
					r = -MAX_TURN_ANGLE;
				else if (r > MAX_TURN_ANGLE)
					r = MAX_TURN_ANGLE;

				vec2_rotate(&m->dir, r);
			}
		}
	} 
}

void
draw_missiles(void)
{
	const struct missile *p;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_FLAT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	for (p = missiles; p != &missiles[MAX_MISSILES]; p++) {
		if (p->used) {
			if (p->type != MT_HOMING)
				draw_missile(p);
			else
				draw_homing_missile_trail(p->trail_head,
				  p->start_seg);
		}
	}

#if 0
	glBlendFunc(GL_ONE, GL_ONE);

	glBindTexture(GL_TEXTURE_2D, homing_trail_texture_id);

	for (i = 0; i < MAX_HOMING_MISSILE_TRAILS; i++) {
		if (homing_missile_trails[i].used)
			draw_homing_missile_trail(&homing_missile_trails[i]);
	}
#endif

	glPopAttrib();
}

void
update_missiles(void)
{
	struct missile *p;

	for (p = missiles; p != &missiles[MAX_MISSILES]; p++) {
		if (p->used) {
			if (p->type == MT_HOMING && p->expiring)
				update_expiring_missile(p);
			else
				update_missile(p);
		}
	}
}

static struct missile *
get_missile(void)
{
	int i;

	for (i = 0; i < MAX_MISSILES; i++) {
		if (!missiles[i].used)
			return &missiles[i];
	}

	return NULL;
}

void
add_missile(const vector2 *origin, const vector2 *direction)
{
	struct missile *m;
	float speed;

	m = get_missile();

	if (m) {
		m->used = 1;

		m->type = ship_has_powerup(PU_POWERSHOT) ? MT_POWER : MT_NORMAL;

		m->is_bouncy = ship_has_powerup(PU_BOUNCY_SHOTS);
		m->pos = *origin;
		m->dir = *direction;

		speed = missile_settings[m->type]->speed;
		vec2_mul_scalar(&m->dir, speed/vec2_length(&m->dir));

		m->angle = 360.f*atan2(-m->dir.x, m->dir.y)/2.f/M_PI;

		level_stat_counters.missiles_fired++;
		game_stat_counters.missiles_fired++;
	}
}

int
add_homing_missile(const vector2 *origin, const vector2 *direction)
{
	struct missile *m;
	int r;

	r = 0;

	if (homing_missile_count < MAX_HOMING_MISSILES) {
		m = get_missile();

		if (m) {
			float speed;

			m->used = 1;

			m->type = MT_HOMING;

			m->is_bouncy = 0;

			m->pos = *origin;
			m->dir = *direction;

			speed = missile_settings[MT_HOMING]->speed;
			vec2_mul_scalar(&m->dir, speed/vec2_length(&m->dir));

			m->angle = 0.f;
			m->target = NULL;
			m->trail_head = add_homing_missile_trail(&m->pos, NULL);
			m->expiring = 0;
			m->start_seg = 0;

			++homing_missile_count;

			r = 1;
		}
	}

	return r;
}

void
initialize_missiles(void)
{
	int i;

	for (i = 0; i < NUM_MISSILE_TYPES; i++) {
		const struct missile_settings *p = missile_settings[i];

		if (p == NULL)
			panic("missile %d not properly initialized!", i);

		missile_texture_ids[i] =
		  png_to_texture(p->sprite->source);
	}
}

void
serialize_missiles(FILE *out)
{
	int count = 0;
	struct missile *p;

	for (p = missiles; p != &missiles[MAX_MISSILES]; p++) {
		if (p->used && p->type != MT_HOMING) /* FIXME */
			++count;
	}

	fwrite(&count, sizeof count, 1, out);

	if (count) {
		for (p = missiles; p != &missiles[MAX_MISSILES]; p++) {
			if (p->used && p->type == MT_NORMAL) {
				fwrite(&p->pos, sizeof p->pos, 1, out);
				fwrite(&p->angle, sizeof p->angle, 1, out);
			}
		}
	}
}

int
unserialize_missiles(const char *data)
{
	int count;
	struct missile *p;
	const char *q;

	q = data;

	memcpy(&count, q, sizeof count);
	q += sizeof count;

	assert(count < MAX_MISSILES);

	for (p = missiles; p != &missiles[count]; p++) {
		p->used = 1;

		memcpy(&p->pos, q, sizeof p->pos);
		q += sizeof p->pos;

		memcpy(&p->angle, q, sizeof p->angle);
		q += sizeof p->angle;
	}

	for (; p != &missiles[MAX_MISSILES]; p++)
		p->used = 0;

	return q - data;
}
