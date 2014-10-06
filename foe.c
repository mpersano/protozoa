#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <GL/gl.h>

#include "audio.h"
#include "vector.h"
#include "list.h"
#include "gl_util.h"
#include "arena.h"
#include "common.h"
#include "settings.h"
#include "mesh.h"
#include "particle.h"
#include "explosion.h"
#include "ship.h"
#include "panic.h"
#include "in_game.h"
#include "powerup.h"
#include "bomb.h"
#include "in_game_text.h"
#include "water.h"
#include "foe.h"

enum {
	MAX_FOES = 50,
	JARHEAD_TTL_AFTER_HIT = 30,
	MAX_NUM_CLUSTER_FOES = 30,
	MAX_NUM_CIRCLE_FOES = 80,
	BRUTE_HIT_COUNT = 10,
	BRUTE_RECOVER_TICS = 10,
	SPAWN_TICS = 20
};

#define BRUTE_MAX_ANGLE_SPEED 5.f

struct mesh_settings *foe_mesh_settings[NUM_FOE_TYPES];

static struct mesh *foe_meshes[NUM_FOE_TYPES];

typedef union foe foe;

typedef void (*foe_fn)(foe *);
typedef int (*foe_hit_fn)(foe *, const vector2 *);

struct foe_sitting_duck {
	struct foe_common common;
};

struct foe_jarhead {
	struct foe_common common;
	int state;
	int ttl;		/* if exploding */
	vector2 hit_dir;	/* direction after hit */
	float spin_angle;	/* when exploding */
};

struct foe_brute {
	struct foe_common common;
	int hit_count;
	int last_hit_tic;
};

enum {
	MAX_TRAIL_SEGS = 20
};

static int duck_trail;
static int ninja_trail;
static int bomb_flash;

struct foe_evolved_duck {
	struct foe_common common;
	int num_trail_segs;
	int trail_seg_head;
	struct vector2 trail_seg[MAX_TRAIL_SEGS];
	float max_turn_angle;
	int trail_texture_id;
};

struct foe_ninja {
	union {
		struct foe_common common;
		struct foe_evolved_duck evolved_duck;
	};
	int state;
	int recover_ttl;	/* if recovering */
	vector2 hit_dir;	/* direction after hit */
	float spin_angle;	/* when exploding */
	int hit_count;
};

struct foe_bomber {
	union {
		struct foe_common common;
		struct foe_jarhead jarhead;
	};
	int refire_count;
};

union foe {
	struct foe_common common;
	struct foe_sitting_duck sitting_duck;
	struct foe_jarhead jarhead;
	struct foe_brute brute;
	struct foe_evolved_duck evolved_duck;
	struct foe_ninja ninja;
	struct foe_bomber bomber;
};

static foe foes[MAX_FOES];

static void foe_common_draw(const struct foe_common *f);

/* sitting duck interface */
static void sitting_duck_update(struct foe_sitting_duck *);
static int sitting_duck_hit(struct foe_sitting_duck *, const vector2 *);
static void sitting_duck_create(struct foe_sitting_duck *);

/* jarhead interface */
static void jarhead_update(struct foe_jarhead *);
static int jarhead_hit(struct foe_jarhead *, const vector2 *);
static void jarhead_draw(struct foe_jarhead *);
static void jarhead_create(struct foe_jarhead *);

/* brute interface */
static void brute_update(struct foe_brute *);
static int brute_hit(struct foe_brute *, const vector2 *);
static void brute_draw(struct foe_brute *);
static void brute_create(struct foe_brute *);

/* evolved duck interface */
static void evolved_duck_create(struct foe_evolved_duck *);
static void evolved_duck_update(struct foe_evolved_duck *);
static void evolved_duck_draw(struct foe_evolved_duck *);

/* ninja interface */
static void ninja_create(struct foe_ninja *);
static void ninja_update(struct foe_ninja *);
static int ninja_hit(struct foe_ninja *, const vector2 *);
static void ninja_draw(struct foe_ninja *);

/* bomber interface */
static void bomber_create(struct foe_bomber *);
static void bomber_update(struct foe_bomber *);
static void bomber_draw(struct foe_bomber *);

static struct {
	float radius;
	float max_speed;
	int points;
	void (*update_fn)(foe *);
	int (*hit_fn)(foe *, const vector2 *);
	void (*draw_fn)(foe *);
	void (*create_fn)(foe *);
} foe_data[NUM_FOE_TYPES] = {
	/* sitting duck */
	{ .4f, .07f, 25,
	  (void(*)(foe *))sitting_duck_update,
	  (int(*)(foe *, const vector2 *))sitting_duck_hit,
	  (void(*)(foe *))foe_common_draw,
	  (void(*)(foe *))sitting_duck_create },

	/* jarhead */
	{ .4f, .065f, 35,
	  (void(*)(foe *))jarhead_update,
	  (int(*)(foe *, const vector2 *))jarhead_hit,
	  (void(*)(foe *))jarhead_draw,
	  (void(*)(foe *))jarhead_create },

	/* brute */
	{ .8f, .06f, 110,
	  (void(*)(foe *))brute_update,
	  (int(*)(foe *, const vector2 *))brute_hit,
	  (void(*)(foe *))brute_draw,
	  (void(*)(foe *))brute_create },

	/* evolved duck */
	{ .4f, .18f, 65,
	  (void(*)(foe *))evolved_duck_update,
	  (int(*)(foe *, const vector2 *))sitting_duck_hit,
	  (void(*)(foe *))evolved_duck_draw,
	  (void(*)(foe *))evolved_duck_create },

	/* ninja */
	{ .4f, .20f, 95,
	  (void(*)(foe *))ninja_update,
	  (int(*)(foe *, const vector2 *))ninja_hit,
	  (void(*)(foe *))ninja_draw,
	  (void(*)(foe *))ninja_create },

	/* fast duck */
	{ .4f, .085f, 30,
	  (void(*)(foe *))sitting_duck_update,
	  (int(*)(foe *, const vector2 *))sitting_duck_hit,
	  (void(*)(foe *))foe_common_draw,
	  (void(*)(foe *))sitting_duck_create },

	/* fast jarhead */
	{ .4f, .12f, 35,
	  (void(*)(foe *))jarhead_update,
	  (int(*)(foe *, const vector2 *))jarhead_hit,
	  (void(*)(foe *))jarhead_draw,
	  (void(*)(foe *))jarhead_create },

	/* bomber */
	{ .4f, .065f, 70,
	  (void(*)(foe *))bomber_update,
	  (int(*)(foe *, const vector2 *))jarhead_hit,
	  (void(*)(foe *))bomber_draw,
	  (void(*)(foe *))bomber_create },
};

/*
 *	F O E - C O M M O N
 */

#define PARTICLE_SPEED .15f

void
bump_score(const vector2 *pos, int points);

static void
foe_common_bump_score(const struct foe_common *foe)
{
	bump_score(&foe->pos, foe_data[foe->type].points);
}

static void
spawn_powerup(const vector2 *pos, int powerup_type)
{
	add_powerup(pos, powerup_type);
}

static void
kill_foe(struct foe_common *foe)
{
	foe->used = 0;

	if (foe->powerup_type != -1)
		spawn_powerup(&foe->pos, foe->powerup_type);

	assert(gc.foes_left > 0);
	--gc.foes_left;
}

static foe *
add_foe(void)
{
	int i;

	for (i = 0; i < MAX_FOES; i++) {
		if (!foes[i].common.used)
			return &foes[i];
	}

	return NULL;
}

static void
foe_common_spawn(struct foe_common *f, const vector2 *source, int type)
{
	vector2 *pos = &f->pos;
	vector2 *dir = &f->dir;
	float speed;

	f->used = 1;

	*pos = *source;

	speed = .05f + .05f*frand();

	dir->x = frand() - .5f;
	dir->y = frand() - .5f;
	vec2_mul_scalar(dir, speed/vec2_length(dir));

	f->tics = 0;
	f->type = type;

	f->angle = 0;
	f->angle_speed = 5.f*frand();

	f->rot_axis.x = frand() - .5f;
	f->rot_axis.y = frand() - .5f;
	f->rot_axis.z = frand() - .5f;
	vec3_normalize(&f->rot_axis);

	foe_data[f->type].create_fn((foe *)f);
}

static int foe_bounced;

static void
foe_common_update(struct foe_common *f)
{
	vector2 normal;
	vector2 pos = f->pos;
	vector2 *dir = &f->dir;
	const float radius = foe_data[f->type].radius;
	int i;

	foe_bounced= 0;

	vec2_add_to(&pos, dir);

	/* bounce with arena */

	if (bounces_with_arena(&pos, radius, &normal)) {
		float k = 2.f*(normal.x*dir->x + normal.y*dir->y);

		dir->x = -(k*normal.x - dir->x);
		dir->y = -(k*normal.y - dir->y);

		pos = f->pos;
		vec2_add_to(&pos, dir);

		foe_bounced = 1;
	}
	
	f->pos = pos;

	/* bounce with other foes */

	for (i = 0; i < MAX_FOES; i++) {
		const foe *other = &foes[i];

		if (other != (foe *)f && other->common.used) {
			vector2 d;
			float l;

			d.x = other->common.pos.x - f->pos.x;
			d.y = other->common.pos.y - f->pos.y;

			l = vec2_length_squared(&d);

			if (l < radius*radius) {
				dir->x -= d.x/(1.f + l);
				dir->y -= d.y/(1.f + l);

				foe_bounced = 1;
			}
		}
	}

	/* rotate */

	f->angle += f->angle_speed;

	f->tics++;
}

enum {
	MAX_EXPLOSION_PARTICLES = 20
};

static void
foe_common_gen_explosion(struct foe_common *f, float scale)
{
	int i;
	particle *p;
	int ndebris, nballs;

	ndebris = scale*(40 + irand(30));

	/* debris */

	for (i = 0; i < ndebris; i++) {
		p = add_particle();

		if (!p)
			break;

		p->ttl = 20 + irand(15);
		p->t = 0;
		p->width = scale*.4f*(.6f + .15f*frand());
		p->height = scale*.3f*(2.8f + .4f*frand());
		p->pos = f->pos;
		p->palette = PAL_YELLOW;
		p->friction = .9;

		vec2_set(&p->dir, frand() - .5f, frand() - .5f);
		vec2_normalize(&p->dir);

		p->speed = scale*(PARTICLE_SPEED + .05f*frand());
	}

#define RADIUS .4f

	/* fireballs */

	nballs = scale*(8 + irand(8));

	for (i = 0; i < nballs; i++) {
		vector2 pos;
		float r, l;
		l = 1.2*scale;
		pos.x = f->pos.x + l*frand() - .5f*l;
		pos.y = f->pos.y + l*frand() - .5f*l;
		r = scale*(.2f + .15f*frand());
		add_explosion(&pos, r, EFF_EXPLOSION, 13 + irand(4));
	}

	/* shockwave */
	add_explosion(&f->pos, scale*.3f, EFF_RING, 18);

/*
	p = add_particle();

	if (p) {
		p->t = 0;
		p->ttl = 20;
		p->width = p->height = scale*.3f;
		p->pos = f->pos;
		p->palette = PAL_RED;
		p->type = PT_SHOCKWAVE;
	}
*/

	perturb_water(&f->pos, scale*1200.f);

	play_fx(FX_EXPLOSION_1);
}

static void
foe_common_draw_modulated(const struct foe_common *f, const struct rgb *rgb, float s)
{
	glPushMatrix();
	glTranslatef(f->pos.x, f->pos.y, 0.f);
	glRotatef(f->angle, f->rot_axis.x, f->rot_axis.y, f->rot_axis.z);
	mesh_render_modulate_color(foe_meshes[f->type], rgb, s);
	glPopMatrix();
}

static void
foe_common_draw_pure(const struct foe_common *f)
{
	glPushMatrix();
	glTranslatef(f->pos.x, f->pos.y, 0.f);
	glRotatef(f->angle, f->rot_axis.x, f->rot_axis.y, f->rot_axis.z);
	mesh_render(foe_meshes[f->type]);
	glPopMatrix();
}

static void
foe_common_draw(const struct foe_common *f)
{	
	if (f->tics < SPAWN_TICS) {
		static const struct rgb white = { 0.f, 1.f, 1.f };
		float s = (float)f->tics/SPAWN_TICS;
		foe_common_draw_modulated(f, &white, s);
	} else {
		foe_common_draw_pure(f);
	}
}

/*
 *	S I T T I N G   D U C K
 */

static void
sitting_duck_create(struct foe_sitting_duck *f)
{
}

static void
sitting_duck_update(struct foe_sitting_duck *f)
{
	foe_common_update(&f->common);
	hit_ship(&f->common.pos, foe_data[f->common.type].radius);
	vec2_clamp_length(&f->common.dir, foe_data[f->common.type].max_speed);
}

static int
sitting_duck_hit(struct foe_sitting_duck *f, const vector2 *hit)
{
	const vector2 *pos = &f->common.pos;
	const float radius = foe_data[f->common.type].radius;

	if (vec2_distance_squared(hit, pos) < radius*radius) {
		foe_common_gen_explosion(&f->common, 1.f);
		kill_foe(&f->common);
		foe_common_bump_score(&f->common);
		return 1;
	}

	return 0;
}

/*
 *	J A R H E A D
 */

enum jarhead_state {
	JARHEAD_ALIVE,
	JARHEAD_EXPLODING
};

static void
jarhead_create(struct foe_jarhead *f)
{
	f->state = JARHEAD_ALIVE;
	vec2_zero(&f->common.dir);
}

static void
foe_add_trail(const vector2 *pos, const vector2 *dir, int palette)
{
	const int nparticles = 3 + irand(7);
	int i;
	vector2 tp = *pos, e = *dir;
	vec2_mul_scalar(&e, 1.02f);
	vec2_sub_from(&tp, &e);

	for (i = 0; i < nparticles; i++) {
		particle *p = add_particle();

		if (!p)
			break;

		p->ttl = 15 + irand(15);
		p->t = 0;
		p->palette = palette;
		p->width = p->height = .2f + .1f*frand();
		p->pos.x = tp.x + .2f*frand() - .1f;
		p->pos.y = tp.y + .2f*frand() - .1f;
		vec2_set(&p->dir, 1, 0);
		p->speed = 0.f;
		p->friction = .9f;
	}
}

#define JARHEAD_MAX_EXPLODING_SPEED .22

static void
jarhead_update(struct foe_jarhead *f)
{
	switch (f->state) {
		case JARHEAD_ALIVE:
			{ vector2 *dir = &f->common.dir;
				vector2 s = ship.pos;
				vec2_sub_from(&s, &f->common.pos);
				vec2_mul_scalar(&s, .0003f);
				vec2_add_to(dir, &s);
				foe_common_update(&f->common);
				hit_ship(&f->common.pos,
				  foe_data[f->common.type].radius);
				vec2_clamp_length(dir,
				  foe_data[f->common.type].max_speed);
			}
			break;

		case JARHEAD_EXPLODING:
			if (--f->ttl <= 0) {
				foe_common_gen_explosion(&f->common, 1.f);
				kill_foe(&f->common);
			} else {
				vector2 *dir = &f->common.dir;

				foe_common_update(&f->common);

				/* spiral */
				vec2_rotate(dir, f->spin_angle);
				vec2_add_to(dir, &f->hit_dir);

				vec2_clamp_length(dir,
				  JARHEAD_MAX_EXPLODING_SPEED);

				/* trail */
				foe_add_trail(&f->common.pos, &f->common.dir,
				  PAL_YELLOW);
			}
			break;

		default:
			assert(0);
	}
}

static int
jarhead_hit(struct foe_jarhead *f, const vector2 *hit)
{
	const float radius = foe_data[f->common.type].radius;

	if (vec2_distance_squared(hit, &f->common.pos) < radius*radius) {
		if (f->state == JARHEAD_ALIVE) {
			if (irand(3) == 0) {
				foe_common_gen_explosion(&f->common, 1.f);
				kill_foe(&f->common);
			} else {
				vector2 s = ship.pos;
				f->state = JARHEAD_EXPLODING;
				vec2_mul_scalar(&s, .1f);
				vec2_sub_from(&f->common.dir, &s);

				f->hit_dir = f->common.dir;
				vec2_mul_scalar(&f->hit_dir,
						.02f/vec2_length(&f->hit_dir));

				f->common.angle_speed *= -4.f;
				f->ttl = JARHEAD_TTL_AFTER_HIT;
				f->spin_angle = .2f + .2f*frand();
				if (irand(2))
					f->spin_angle = -f->spin_angle;
			}
			foe_common_bump_score(&f->common);
		}
		return 1;
	}

	return 0;
}

static void
jarhead_draw(struct foe_jarhead *f)
{
	if (f->state == JARHEAD_ALIVE) {
		foe_common_draw(&f->common);
	} else {
		static const struct rgb yellowish = { 1.f, .8f, .4f };
		float s;

		s = (float)f->ttl/JARHEAD_TTL_AFTER_HIT;

		if (s < 0.f)
			s = 0.f;

		if (s >= 1.f)
			s = 1.f;

		foe_common_draw_modulated(&f->common, &yellowish, s);
	}
}

/*
 *	B R U T E
 */

static void
brute_create(struct foe_brute *f)
{
	f->hit_count = BRUTE_HIT_COUNT;
	f->last_hit_tic = -1;
}

static int 
brute_is_recovering(struct foe_brute *f)
{
	return
	  f->last_hit_tic != -1 &&
	  gc.level_tics - f->last_hit_tic <= BRUTE_RECOVER_TICS;
}

static void
brute_update(struct foe_brute *f)
{
	vector2 *dir = &f->common.dir;
	vector2 s = ship.pos;
	vec2_sub_from(&s, &f->common.pos);
	vec2_mul_scalar(&s, .004f);
	vec2_add_to(dir, &s);
	foe_common_update(&f->common);
	hit_ship(&f->common.pos, foe_data[FOE_BRUTE].radius);

	if (vec2_length(dir) > foe_data[FOE_BRUTE].max_speed);
		vec2_mul_scalar(dir, .7);

	if (f->common.angle_speed > BRUTE_MAX_ANGLE_SPEED)
		f->common.angle_speed *= .7;

	if (brute_is_recovering(f))
		foe_add_trail(&f->common.pos, &f->common.dir, PAL_YELLOW);
}

static void
brute_draw(struct foe_brute *f)
{
	glPushMatrix();
	glTranslatef(f->common.pos.x, f->common.pos.y, 0.f);
	glRotatef(f->common.angle, f->common.rot_axis.x, f->common.rot_axis.y,
	  f->common.rot_axis.z);

	if (f->common.tics < SPAWN_TICS) {
		static const struct rgb white = { .8f, 1.f, 1.f };
		float s = (float)f->common.tics/SPAWN_TICS;
		mesh_render_modulate_color(foe_meshes[f->common.type], &white,
		  s);
	} else {
		static const struct rgb red = { .4f, 0.f, 0.f };
		float s;

		if (!brute_is_recovering(f)) {
			s = 1.f;
		} else {
			int t = gc.level_tics - f->last_hit_tic;
			s = (float)t/BRUTE_RECOVER_TICS;
		}

		mesh_render_modulate_color(foe_meshes[f->common.type], &red, s);
	}

	glPopMatrix();
}

static int
brute_hit(struct foe_brute *f, const vector2 *hit)
{
	const vector2 *pos = &f->common.pos;
	const float radius = foe_data[FOE_BRUTE].radius;

	if (vec2_distance_squared(hit, pos) < radius*radius) {
		if (--f->hit_count == 0) {
			foe_common_gen_explosion(&f->common, 1.3f);
			kill_foe(&f->common);
			foe_common_bump_score(&f->common);
		} else {
			vector2 s, os;
			float t;
			f->last_hit_tic = gc.level_tics;

			s = f->common.pos;
			vec2_sub_from(&s, &ship.pos);
			vec2_normalize(&s);

			vec2_set(&os, -s.y, s.x);

			t = 5.f*(frand() - .5f);
			s.x += t*os.x;
			s.y += t*os.y;
			vec2_mul_scalar(&s, .3f/vec2_length(&s));

			vec2_add_to(&f->common.dir, &s);

			f->common.angle_speed *= 2;
		}

		return 1;
	}

	return 0;
}

/*
 * 	E V O L V E D   D U C K
 */

#define MAX_EVOLVED_DUCK_TURN_ANGLE .05f

static void
evolved_duck_create(struct foe_evolved_duck *f)
{
	f->num_trail_segs = 1;
	f->trail_seg_head = 0;
	f->trail_seg[f->trail_seg_head] = f->common.pos;
	f->trail_texture_id = duck_trail;
	f->max_turn_angle = MAX_EVOLVED_DUCK_TURN_ANGLE;
	vec2_mul_scalar(&f->common.dir, .1f);
}

static void
evolved_duck_update(struct foe_evolved_duck *f)
{
	vector2 d, od, t;
	float r;
	int ship_is_behind;

	foe_common_update(&f->common);

	if (foe_bounced) {
		f->num_trail_segs = 0;
	} else {
		if (f->num_trail_segs < MAX_TRAIL_SEGS)
			f->num_trail_segs++;

		if (++f->trail_seg_head >= MAX_TRAIL_SEGS)
			f->trail_seg_head = 0;

		f->trail_seg[f->trail_seg_head] = f->common.pos;
	}

	hit_ship(&f->common.pos, foe_data[f->common.type].radius);

	od.x = ship.pos.x - f->common.pos.x;
	od.y = ship.pos.y - f->common.pos.y;
	vec2_normalize(&od);

	t.x = -f->common.dir.y;
	t.y = f->common.dir.x;

	ship_is_behind = vec2_dot_product(&od, &t) > 0;

	/* accelerate if behind ship, brake otherwise */

	if (ship_is_behind)
		vec2_mul_scalar(&f->common.dir, .99f);
	else
		vec2_mul_scalar(&f->common.dir, 1.2f);

	/* turn towards ship */

	d.x = -(ship.pos.y - f->common.pos.y);
	d.y = ship.pos.x - f->common.pos.x;
	vec2_normalize(&d);

	r = 1.6f*vec2_dot_product(&d, &f->common.dir);

	if (r < 0) {
		if (ship_is_behind)
			r = -f->max_turn_angle;
		else {
			if (r < -f->max_turn_angle)
				r = -f->max_turn_angle;
		}
	} else {
		if (ship_is_behind) {
			r = f->max_turn_angle;
		} else {
			if (r > f->max_turn_angle)
				r = f->max_turn_angle;
		}
	}

	vec2_rotate(&f->common.dir, r);

	vec2_clamp_length(&f->common.dir, foe_data[f->common.type].max_speed);
}

static void
evolved_duck_draw(struct foe_evolved_duck *f)
{
	float u, du;
	float t, s, hs;
	int i, j, k;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_LIGHTING);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(f->trail_texture_id));
	glDisable(GL_DEPTH_TEST);

	s = vec2_length(&f->common.dir);
	hs = .5f*foe_data[f->common.type].max_speed;

	if (s < hs)
		t = 0.f;
	else
		t = (s - hs)/hs;

	glColor4f(t, t, t, t);

	glBegin(GL_TRIANGLE_STRIP);

	if (f->num_trail_segs > 1) {
		vector2 d, n, *from, *to;

		/* head */

		j = f->trail_seg_head;
		k = f->trail_seg_head - 1;

		if (k < 0)
			k = MAX_TRAIL_SEGS - 1;

		from = &f->trail_seg[j];
		to = &f->trail_seg[k];

		d.x = -(to->y - from->y);
		d.y = to->x - from->x;

#define HS 3.f
#define HL 1.5
		vec2_mul_scalar(&d, .5f*HL/vec2_length(&d));

		assert(from->x == f->common.pos.x);
		assert(from->y == f->common.pos.y);

		n.x = from->x + HS*f->common.dir.x;
		n.y = from->y + HS*f->common.dir.y;

		glTexCoord2f(0, 0);
		glVertex3f(n.x - d.x, n.y - d.y, 0);

		glTexCoord2f(1, 0);
		glVertex3f(n.x + d.x, n.y + d.y, 0);

		/* tail */

		/* draw tail */
#define START_U .15f
		du = (1.f - START_U)/f->num_trail_segs;
		u = START_U;

		j = f->trail_seg_head;

		for (i = 0; i < f->num_trail_segs - 1; i++) {
			vector2 d, *from, *to;

			k = j - 1;
			if (k < 0)
				k = MAX_TRAIL_SEGS - 1;

			from = &f->trail_seg[j];
			to = &f->trail_seg[k];

			d.x = -(to->y - from->y);
			d.y = to->x - from->x;

			if (i < f->num_trail_segs - 2) {
				vector2 *next_to;
				int l = k - 1;
				if (l < 0)
					l = MAX_TRAIL_SEGS - 1;

				next_to = &f->trail_seg[l];

				d.x += -(next_to->y - to->y);
				d.y += next_to->x - to->x;
			}

			assert(vec2_length_squared(&d) > 0.f);

			vec2_mul_scalar(&d, .5f*HL/vec2_length(&d));

			glTexCoord2f(0, u);
			glVertex3f(from->x - d.x, from->y - d.y, 0.f);

			glTexCoord2f(1, u);
			glVertex3f(from->x + d.x, from->y + d.y, 0.f);

			j = k;

			u += du;
		}
	}

	glEnd();
	glPopAttrib();
	
	{
		float dir_angle = f->common.angle = 180.f +
		  360.f*atan2(-f->common.dir.x, f->common.dir.y)/
		  2.f/M_PI;

	glPushMatrix();
	glTranslatef(f->common.pos.x, f->common.pos.y, 0.f);
	glRotatef(dir_angle, 0, 0, 1);
	glRotatef(f->common.angle, 0, 1, 0);

	if (f->common.tics < SPAWN_TICS) {
		static const struct rgb white = { 0.f, 1.f, 1.f };
		float s = (float)f->common.tics/SPAWN_TICS;
		mesh_render_modulate_color(foe_meshes[f->common.type], &white, s);
	} else {
		mesh_render(foe_meshes[f->common.type]);
	}

	glPopMatrix();
	}

}

/*
 *	N I N J A
 */

enum ninja_state {
	NINJA_ALIVE,
	NINJA_RECOVERING
};

enum {
	NINJA_RECOVER_TTL = 15
};

static void
ninja_create(struct foe_ninja *f)
{
	f->state = NINJA_ALIVE;
	f->hit_count = 10;
	evolved_duck_create(&f->evolved_duck);
	f->evolved_duck.trail_texture_id = ninja_trail;
	f->evolved_duck.max_turn_angle = .2f*MAX_EVOLVED_DUCK_TURN_ANGLE;
	vec3_set(&f->common.rot_axis, 0.f, 0.f, 1.f);
}

static void
ninja_draw(struct foe_ninja *f)
{
	switch (f->state) {
		case NINJA_ALIVE:
			evolved_duck_draw(&f->evolved_duck);
			break;

		case NINJA_RECOVERING:
			foe_common_draw(&f->common);
			break;

		default:
			assert(0);
	}
}

static void
ninja_update(struct foe_ninja *f)
{
	switch (f->state) {
		case NINJA_ALIVE:
			evolved_duck_update(&f->evolved_duck);
			break;

		case NINJA_RECOVERING:
			if (--f->recover_ttl <= 0) {
/* THIS DOESN'T WORK! */
				float a = (-f->common.angle + 270.f + 180.f)*
				  2.f*M_PI/360.f;
				f->common.dir.x = .01f*cos(a);
				f->common.dir.y = .01f*sin(a);
				f->evolved_duck.num_trail_segs = 0;
				f->state = NINJA_ALIVE;
			} else {
				vector2 *dir = &f->common.dir;
				foe_common_update(&f->common);

				vec2_rotate(dir, f->spin_angle);
				vec2_add_to(dir, &f->hit_dir);

				vec2_clamp_length(dir,
				  JARHEAD_MAX_EXPLODING_SPEED);

				foe_add_trail(&f->common.pos, &f->common.dir,
				  PAL_GREEN);

				f->common.angle_speed *= .8f;
			}
			break;

		default:
			assert(0);
	}
}

static int
ninja_hit(struct foe_ninja *f, const vector2 *hit)
{
	int r = 0;
	const float radius = foe_data[FOE_NINJA].radius;

	if (vec2_distance_squared(hit, &f->common.pos) < radius*radius) {
		r = 1;

		if (--f->hit_count <= 0) {
			foe_common_gen_explosion(&f->common, 1.f);
			kill_foe(&f->common);
			foe_common_bump_score(&f->common);
		} else {
			vector2 s = ship.pos;

			f->state = NINJA_RECOVERING;
			vec2_mul_scalar(&s, .15f);
			vec2_sub_from(&f->common.dir, &s);

			f->hit_dir = f->common.dir;
			vec2_mul_scalar(&f->hit_dir,
					.03f/vec2_length(&f->hit_dir));

			f->common.angle_speed *= -4.f;
			f->recover_ttl = NINJA_RECOVER_TTL;
			f->spin_angle = .2f + .2f*frand();
			if (irand(2))
				f->spin_angle = -f->spin_angle;
		}
	}

	return r;
}

/*
 *	B O M B E R
 */

enum {
	BOMBER_TICS_BEFORE_REFIRE = 30,
	BOMBER_FIRE_TICS = 10,
};

static void
bomber_create(struct foe_bomber *f)
{
	jarhead_create(&f->jarhead);
	f->refire_count = 0;
}

static void
bomber_update(struct foe_bomber *f)
{
	jarhead_update(&f->jarhead);

	if (f->jarhead.state == JARHEAD_ALIVE) {
		if (f->jarhead.state == JARHEAD_ALIVE &&
		  f->refire_count > BOMBER_TICS_BEFORE_REFIRE &&
		  irand(20) == 0) {
			add_bomb(&f->common.pos);
			f->refire_count = 0;
		} else {
			f->refire_count++;
		}
	}
}

static void
bomber_draw(struct foe_bomber *f)
{
	if (f->jarhead.state == JARHEAD_ALIVE && f->refire_count < BOMBER_FIRE_TICS) {
		static const struct rgb yellowish = { 1.f, .8f, .4f };
		float s;
		float r;

		s = (float)f->refire_count/BOMBER_FIRE_TICS;
		r = (1. - s)*1.2f;

		foe_common_draw_modulated(&f->common, &yellowish, s);

		/* bomb flash */
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glDisable(GL_LIGHTING);
		glShadeModel(GL_FLAT);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, gl_texture_id(bomb_flash));

		glColor4f(1, 1, 1, s);

		glPushMatrix();
		glTranslatef(f->common.pos.x, f->common.pos.y, 0);

		glBegin(GL_QUADS);

		glTexCoord2f(0, 1);
		glVertex3f(-r, -r, 0.f);

		glTexCoord2f(1, 1);
		glVertex3f(r, -r, 0.f);

		glTexCoord2f(1, 0);
		glVertex3f(r, r, 0.f);

		glTexCoord2f(0, 0);
		glVertex3f(-r, r, 0.f);

		glEnd();

		glPopMatrix();

		glPopAttrib();
	} else {
		jarhead_draw(&f->jarhead);
	}
}

/*
 *	F O E   S P A W N E R
 */

enum {
	MAX_FOE_GENERATORS = 32
};

enum foe_generator_type {
	FS_BORDER,
	FS_CIRCLE,
	FS_CLUSTER
};

struct foe_generator {
	int generator_type;
	int foe_type;
	int foe_count;
	vector2 center;		/* circle or cluster */
	float radius;		/* circle */
	int border;		/* if border */
	int powerup_type;	/* to be carried by last foe */
};

static struct foe_generator foe_generators[MAX_FOE_GENERATORS];

static foe *
spawn_foe(int type, vector2 *pos)
{
	foe *f = add_foe();

	if (f != NULL) {
		foe_common_spawn(&f->common, pos, type);
		add_explosion(pos, 3.f*foe_data[type].radius, EFF_FLARE, 20);
		perturb_water(pos, 300.f);
	}

	return f;
}

static int
is_safe_pos(const vector2 *p, float radius)
{
	vector2 dummy;
	float r2;
	int i;

	/* arena */

	if (bounces_with_arena(p, radius, &dummy))
		return 0;

	/* ship */

	r2 = (radius + 5.f*SHIP_RADIUS)*(radius + 5.f*SHIP_RADIUS);

	if (vec2_distance_squared(p, &ship.pos) <= r2)
		return 0;

	/* XXX: other foes */

	for (i = 0; i < MAX_FOES; i++) {
		const foe *f = &foes[i];

		if (f->common.used) {
			r2 = (radius + foe_data[f->common.type].radius);
			r2 *= r2;

			if (vec2_distance_squared(p, &f->common.pos) <= .5f*r2)
				return 0;
		}
	}

	return 1;
}

static void
safe_pos(vector2 *p, float radius)
{
	do {
		p->x = 5.f*frand() - 2.5f;
		p->y = 5.f*frand() - 2.5f;
	} while (!is_safe_pos(p, radius));
}

static void
get_foe_pos_for_border_generator(struct foe_generator *generator, vector2 *pos,
  int foe_type)
{
	const int border = generator->border;
	const vector2 *p0 = &cur_arena->verts[border];
	const vector2 *p1 = &cur_arena->verts[(border + 1)%cur_arena->nverts];
	vector2 n;
	float t;

	n.x = -(p1->y - p0->y);
	n.y = p1->x - p0->x;
	vec2_mul_scalar(&n, 1.5*foe_data[foe_type].radius/vec2_length(&n));

	t  = frand();

	pos->x = p0->x + t*(p1->x - p0->x) + n.x;
	pos->y = p0->y + t*(p1->y - p0->y) + n.y;
}

static void
get_foe_pos_for_circle_generator(struct foe_generator *generator, vector2 *pos)
{
	float a = frand()*2.f*M_PI;
	float c = cos(a);
	float s = sin(a);

	pos->x = generator->center.x + generator->radius*c;
	pos->y = generator->center.y + generator->radius*s;
}

static void
get_foe_pos_for_cluster_generator(struct foe_generator *generator, vector2 *pos)
{
	pos->x = generator->center.x + (frand() - .5f)*generator->radius;
	pos->y = generator->center.y + (frand() - .5f)*generator->radius;
}

static int active_foes;

static void
update_foe_generator(struct foe_generator *p)
{
	const int foe_type = p->foe_type;
	vector2 pos;

	assert(p->foe_count > 0);

	switch (p->generator_type) {
		case FS_BORDER:
			get_foe_pos_for_border_generator(p, &pos, foe_type);
			break;

		case FS_CIRCLE:
			get_foe_pos_for_circle_generator(p, &pos);
			break;

		case FS_CLUSTER:
			get_foe_pos_for_cluster_generator(p, &pos);
			break;

		default:
			assert(0);
	}

	if (is_safe_pos(&pos, foe_data[foe_type].radius)) {
		foe *f = spawn_foe(foe_type, &pos);

		if (f != NULL) {
			--p->foe_count;
			++active_foes;

			if (p->foe_count == 0)
				f->common.powerup_type = p->powerup_type;
			else
				f->common.powerup_type = -1;
		}
	}
}

static void
spawn_random_foe(void)
{
	vector2 source;
	int type = irand(NUM_FOE_TYPES);
	float radius = foe_data[type].radius;

	safe_pos(&source, radius);
	spawn_foe(type, &source);
}

#define CLUSTER_RADIUS 2.f
#define CIRCLE_RADIUS 4.f

static struct foe_generator *
get_foe_generator(void)
{
	int i;

	for (i = 0; i < MAX_FOE_GENERATORS; i++)
		if (foe_generators[i].foe_count <= 0)
			return &foe_generators[i];

	return NULL;
}

static void
add_generator(int type, const struct event *ev)
{
	struct foe_generator *p = get_foe_generator();

	if (p != NULL) {
		p->generator_type = type;
		p->foe_count = ev->foe_count;
		p->foe_type = ev->foe_type;
		if (ev->center)
			p->center = *ev->center;
		else
			vec2_zero(&p->center);
		p->radius = ev->radius;
		p->border = ev->wall;
		p->powerup_type = ev->powerup_type;
	}
}

void
spawn_new_foes(void)
{
#if 0
	if (gc.level_tics % 50 == 0)
		spawn_random_foe();

	if (gc.level_tics % 150 == 0) {
		switch (irand(2)) {
			case 0:
				add_border_generator();
				break;
			
			case 1:
				add_circle_generator();
				break;

			default:
				break;
		}
	}
#else
	const struct level *l = levels[gc.cur_level];
	const struct list_node *ln;

	assert(inner_state.state == IS_IN_GAME);

	for (ln = l->waves[gc.cur_wave]->events->first; ln; ln = ln->next) {
		const struct event *ev = (const struct event *)ln->data;

		if (ev->time == inner_state.tics) {
			switch (ev->type) {
				case EV_CIRCLE:
					add_generator(FS_CIRCLE, ev);
					break;

				case EV_CLUSTER:
					add_generator(FS_CLUSTER, ev);
					break;

				case EV_WALL:
					add_generator(FS_BORDER, ev);
					break;

				default:
					assert(0);
			}
		}
	}
#endif
}

/*
 *	P U B L I C   I N T E R F A C E
 */


void
reset_foes(void)
{
	int i;

	for (i = 0; i < MAX_FOES; i++)
		foes[i].common.used = 0;

	for (i = 0; i < MAX_FOE_GENERATORS; i++)
		foe_generators[i].foe_count = 0;
}

void
draw_foes(void)
{
	int i;

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);

	for (i = 0; i < MAX_FOES; i++) {
		foe *f = &foes[i];

		if (f->common.used) {
			foe_data[f->common.type].draw_fn(f);
		}
	}

	glPopAttrib();
}

static int
count_active_foes(void)
{
	union foe *p;
	int count;

	count = 0;

	for (p = foes; p != &foes[MAX_FOES]; p++) {
		if (p->common.used)
			++count;
	}

	return count;
}

void
update_foes(void)
{
	int i;
	const struct level *l = levels[gc.cur_level];
	const int max_active_foes = l->waves[gc.cur_wave]->max_active_foes;

	for (i = 0; i < MAX_FOES; i++) {
		foe *f = &foes[i];

		if (f->common.used) {
			foe_data[f->common.type].update_fn(f);
		}
	}

	active_foes = count_active_foes();

	if (max_active_foes == 0 || active_foes < max_active_foes) {
		for (i = 0; i < MAX_FOE_GENERATORS; i++) {
			assert(active_foes >= 0 && active_foes < max_active_foes);

			if (foe_generators[i].foe_count > 0) {
				update_foe_generator(&foe_generators[i]);

				if (max_active_foes != 0 && active_foes >= max_active_foes)
					break;
			}
		}
	}
}

int
hit_foes(const vector2 *pos)
{
	int i;

	for (i = 0; i < MAX_FOES; i++) {
		foe *f = &foes[i];

		if (f->common.used) {
			if (foe_data[f->common.type].hit_fn(f, pos))
				return 1;
		}
	}

	return 0;
}

static struct {
	float scale;
	const char *name;
} foe_models[NUM_FOE_TYPES] = {
	{ 9.f, "duck.obj" },
	{ 1.f, "jarhead.obj" },
	{ 1.f, "brute.obj" },
	{ 6.f, "evolved-duck.obj" },
	{ 6.f, "ninja.obj" },
	{ 1.f, "fast-duck.obj" },
	{ 5.5f, "evolved-duck.obj" },
};

void
initialize_foes(void)
{
	int i;

	for (i = 0; i < NUM_FOE_TYPES; i++) {
		struct mesh *p;
		const struct mesh_settings *q = foe_mesh_settings[i];

#if 0
		struct matrix s;

		mat_make_scale(&s, foe_models[i].scale);
		p = mesh_load_obj(foe_models[i].name);
		mesh_transform(p, &s);
#else
		if (q == NULL)
			panic("foe %d not properly initialized!", i);

		p = mesh_load_obj(q->source);
		mesh_transform(p, q->transform);
#endif
		mesh_setup(p);

		foe_meshes[i] = p;
	}

	duck_trail = png_to_texture("duck-trail.png");
	ninja_trail = png_to_texture("ninja-trail.png");
	bomb_flash = png_to_texture("bomb-flash.png");
}

void
tear_down_foes(void)
{
	struct mesh **p;

	for (p = foe_meshes; p != &foe_meshes[NUM_FOE_TYPES]; p++)
		mesh_release(*p);
}

const struct foe_common *
get_random_foe(void)
{
	const foe *p;
	const struct foe_common *r = NULL;
	int n = 1;

	for (p = foes; p != &foes[MAX_FOES]; p++) {
		if (p->common.used) {
			/* the first used foe will be selected with
			 * probability 1/1; the second one with probability
			 * 1/2; the third one with probability 1/3; and so
			 * on. */

			if (irand(n) == 0)
				r = &p->common;
			++n;
		}
	}

	return r;
}

const struct foe_common *
get_closest_foe(const vector2 *pos)
{
	const foe *p;
	const struct foe_common *r = NULL;
	float r2 = -1;

	for (p = foes; p != &foes[MAX_FOES]; p++) {
		if (p->common.used) {
			float t = vec2_distance_squared(&p->common.pos, pos);

			if (r2 == -1 || t < r2) {
				r = &p->common;
				r2 = t;
			}
		}
	}

	return r;
}

void
serialize_foes(FILE *out)
{
	int count = 0;
	foe *p;

	for (p = foes; p != &foes[MAX_FOES]; p++) {
		if (p->common.used)
			count++;
	}

	fwrite(&count, sizeof count, 1, out);

	if (count) {
		for (p = foes; p != &foes[MAX_FOES]; p++) {
			if (p->common.used)
				fwrite(p, sizeof *p, 1, out);
		}
	}
}

int
unserialize_foes(const char *data)
{
	int count;
	foe *p;

	memcpy(&count, data, sizeof count);
	data += sizeof count;

	assert(count < MAX_FOES);

	if (count)
		memcpy(foes, data, count*sizeof *foes);

	for (p = &foes[count]; p != &foes[MAX_FOES]; p++)
		p->common.used = 0;

	return sizeof count + count*sizeof *foes;
}
