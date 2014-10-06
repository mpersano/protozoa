#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <GL/gl.h> 
#include "audio.h"
#include "missile.h"
#include "laser.h"
#include "foe.h"
#include "arena.h"
#include "powerup.h"
#include "particle.h"
#include "explosion.h"
#include "settings.h"
#include "common.h"
#include "image.h"
#include "gl_util.h"
#include "mesh.h"
#include "water.h"
#include "panic.h"
#include "in_game.h"
#include "ship.h"

struct ship ship;
struct ship_settings *ship_settings;
struct shield_settings *shield_settings;

static struct mesh *ship_mesh;
int ship_outline_texture_id;

static int missile_glow_texture_id;
static int power_missile_glow_texture_id;
static int shield_texture_id;

int initial_powerup_ttl[NUM_POWERUP_TYPES] = {
	350,	/* extra cannon */
	100,	/* bouncy shots */
	350,	/* side shots */
	200,	/* power shots */
	150,	/* shield */
	0,	/* extra ship */
	150,	/* homing missile */
	100,	/* laser */
};

enum {
	TICS_BEFORE_REFIRE = 3,
	TICS_BEFORE_REFIRE_POWER = 2,
	NUM_SPAWN_PARTICLES = 300,
	SPAWN_TICS = 40
};

const float MAX_TILT_ANGLE = 30.f;
const float ANGLE_DELTA = 20.f;

struct vector2 spawn_particles[NUM_SPAWN_PARTICLES];

static void
set_missile_origin(vector2 *p)
{
	vector2 o;
	vec2_set(&o, .8f*SHIP_RADIUS, 0.f);
	vec2_rotate(&o, ((-ship.cur_angle - 90)*2.f*M_PI)/360.f);
	*p = ship.pos;
	vec2_add_to(p, &o);
}

void
gen_ship_implosion(void)
{
	int i;
	particle *p;

	/* debris */

	for (i = 0; i < 400; i++) {
		vector2 offs;

		p = add_particle();

		if (!p)
			break;

		p->ttl = 30 + irand(30);
		p->t = 0;
		p->width = .3f*(1.6f + .1f*frand());
		p->height = .3f*(2.8f + .4f*frand());
		p->pos = ship.pos;
		p->palette = PAL_BLUE;

		vec2_set(&p->dir, frand() - .5f, frand() - .5f);
		vec2_normalize(&p->dir);

		offs = p->dir;
		vec2_mul_scalar(&offs, 1.3f*frand());
		vec2_add_to(&p->pos, &offs);

		p->speed = .2f + .05f*frand();
		p->friction = .8f;

		vec2_mul_scalar_copy(&p->acc, &p->dir, -.6f);

		p->acc.x += .2f*frand() - .1f;
		p->acc.y += .2f*frand() - .1f;
	}
}

void
reset_ship_powerups(void)
{
	memset(ship.powerup_ttl, 0, sizeof ship.powerup_ttl);
}

void
reset_ship(void)
{
	vec2_set(&ship.pos, 0.f, 0.f);
	ship.angle = ship.cur_angle = 0.f;
	ship.refire_count = 0;
	ship.powerup_ttl[PU_SHIELD] = 100;
	ship.is_alive = 1;
	ship.tilt = MAX_TILT_ANGLE;

	gen_ship_implosion();
	perturb_water(&ship.pos, 800.f);
}

static inline int
tics_before_refire(void)
{
	return ship_has_powerup(PU_POWERSHOT) ?
	  TICS_BEFORE_REFIRE_POWER : TICS_BEFORE_REFIRE;
}

static void
draw_ship_shape(void)
{
	float s;
	static const struct rgb glow_color = { 1., .8, .8 };

	if (!ship.is_alive)
		return;

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);

	glPushMatrix();
	glTranslatef(ship.pos.x, ship.pos.y, 0.f);
	glRotatef(ship.cur_angle, 0, 0, 1.f);

	glRotatef(ship.tilt, 0, 1, 0);

	s = 1.f - (float)ship.refire_count/tics_before_refire();
	if (s < 0.f)
		s = 0.f;
	if (s > 1.f)
		s = 1.f;

	mesh_render_modulate_color(ship_mesh, &glow_color, s*.4 + .6);

	glPopMatrix();

	glPopAttrib();

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glPushMatrix();
	glTranslatef(ship.pos.x, ship.pos.y, 0.f);
	glRotatef(ship.cur_angle, 0, 0, 1.f);

	glColor3f(1, 1, 1);

	/* shield */

	if (ship_has_powerup(PU_SHIELD)) {
		float s, q, w, h;

		glBindTexture(GL_TEXTURE_2D, gl_texture_id(shield_texture_id));
		glBlendFunc(GL_ONE, GL_ONE);

		if (ship.powerup_ttl[PU_SHIELD] > 30)
			s = 1.f;
		else
			s = (float)ship.powerup_ttl[PU_SHIELD]/30;

		q = sin(.1f*gc.level_tics);
		s *= .6f + .4f*q;
		
		glColor3f(s, s, s);

		w = .5f*ship_settings->shield_sprite->width;
		h = .5f*ship_settings->shield_sprite->height;

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
	}

	glPopMatrix();

	/* missile glow */

	if (ship.refire_count > 0) {
		vector2 p;
		float s, r;

		s = 1.f - (float)ship.refire_count/tics_before_refire();

		set_missile_origin(&p);

		glPushMatrix();
		glTranslatef(p.x, p.y, 0.f);

		glBindTexture(GL_TEXTURE_2D,
		  gl_texture_id(ship_has_powerup(PU_POWERSHOT) ?
		    power_missile_glow_texture_id : missile_glow_texture_id));

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		r = s*1.2f;

		glColor3f(1, 1, 1);

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
	}

	glPopAttrib();
}

void
draw_ship(void)
{
	draw_ship_shape();
}

static void
fire_missile(const vector2 *d)
{
	vector2 n, p;
	vector2 d0, d1, t;

	set_missile_origin(&p);

	vec2_set(&t, -d->y, d->x);
	n = t;

	if (ship_has_powerup(PU_EXTRA_CANNON))
		vec2_mul_scalar(&n, .1f);
	else
		vec2_mul_scalar(&n, .03f);

	d0 = *d;
	vec2_add_to(&d0, &n);

	d1 = *d;
	vec2_sub_from(&d1, &n);

	add_missile(&p, &d0);
	add_missile(&p, &d1);

	if (ship_has_powerup(PU_EXTRA_CANNON))
		add_missile(&p, d);

	if (ship_has_powerup(PU_SIDE_CANNON)) {
		vector2 ot;
		add_missile(&p, &t);
		vec2_set(&ot, -t.x, -t.y);
		add_missile(&p, &ot);
	}

	if (ship_has_powerup(PU_HOMING_MISSILE)) {
		int i, r;
#define NUM_SIMULTANEOUS_HOMING_MISSILES 3

		r = 0;

		for (i = 0; i < NUM_SIMULTANEOUS_HOMING_MISSILES; i++) {
			vector2 dir;

			dir.x = frand() - .5f;
			dir.y = frand() - .5f;

			r = r || add_homing_missile(&p, &dir);
		}

		if (r)
			play_fx(FX_SHOT_3);
	}

	if (ship_has_powerup(PU_LASER)) {
		float a = ship.cur_angle*2.f*M_PI/360.f;
		float da = 2.f*M_PI/MAX_LASERS;
		int i, r;

		r = 0;

		for (i = 0; i < MAX_LASERS; i++) {
			vector2 dir;

			dir.x = sin(a);
			dir.y = cos(a);

			r = r || add_laser(&p, &dir);

			a += da;
		}

		if (r)
			play_fx(FX_SHOT_3);
	}

	play_fx(ship_has_powerup(PU_POWERSHOT) ? FX_SHOT_2 : FX_SHOT_1);
}

static void
add_ship_trail(const vector2 *pos, const vector2 *prev_pos)
{
	const int nparticles = irand(7);
	int i;
	vector2 d = *pos, tp = *prev_pos;
	vec2_mul_scalar(&d, 1.02f);
	vec2_sub_from(&d, prev_pos);
	vec2_sub_from(&tp, &d);

	for (i = 0; i < nparticles; i++) {
		particle *p = add_particle();

		if (!p)
			break;

		p->ttl = 15 + irand(15);
		p->t = 0;
		p->palette = PAL_YELLOW;
		p->width = p->height = .3f + .2f*frand();
		p->pos.x = tp.x + .2f*frand() - .1f;
		p->pos.y = tp.y + .2f*frand() - .1f;
		vec2_set(&p->dir, 1, 0);
		p->speed = 0.f;
		p->friction = .9f;
	}
}

static void
get_ship_stick_direction(vector2 *dir)
{
	if (pad_state & PAD_1_LEFT)
		dir->x = -1.f;
	else if (pad_state & PAD_1_RIGHT)
		dir->x = 1.f;
	else
		dir->x = 0.f;

	if (pad_state & PAD_1_UP)
		dir->y = 1.f;
	else if (pad_state & PAD_1_DOWN)
		dir->y = -1.f;
	else
		dir->y = 0.f;

	vec2_normalize(dir);
}

static void
get_ship_firing_stick_direction(vector2 *dir)
{
	if (settings.control_type == CONTROL_HYBRID) {
		if (!(pad_state & PAD_BTN1)) {
			dir->x = dir->y = 0.f;
		} else {
			vector2 p;
			*dir = crosshair;
			set_missile_origin(&p);
			vec2_sub_from(dir, &p);
		}
	} else {
		if (pad_state & PAD_2_LEFT)
			dir->x = -1.f;
		else if (pad_state & PAD_2_RIGHT)
			dir->x = 1.f;
		else
			dir->x = 0.f;

		if (pad_state & PAD_2_UP)
			dir->y = 1.f;
		else if (pad_state & PAD_2_DOWN)
			dir->y = -1.f;
		else
			dir->y = 0.f;
	}

	vec2_normalize(dir);
}

static float
fmod360(const float a)
{
	if (a < 0.f)
		return a + 360.f;
	else if (a >= 360.f)
		return a - 360.f;
	else
		return a;
}

void
update_ship(void)
{
	float a, b;
	vector2 dir, fire_dir;
	int i;

	if (!ship.is_alive)
		return;

	get_ship_stick_direction(&dir);

	if (dir.x != 0 || dir.y != 0) {
		vector2 pos, d, n;

		vec2_mul_scalar_copy(&d, &dir, ship_settings->speed);
		vec2_add(&pos, &ship.pos, &d);

		if (bounces_with_arena(&pos, .3f, &n)) {
			float s;
			vector2 o = { -n.y, n.x }, od;

			s = vec2_dot_product(&o, &d)/vec2_length_squared(&o);

			vec2_mul_scalar_copy(&od, &o, s);
			vec2_add(&pos, &ship.pos, &od);
		} 

		if (!bounces_with_arena(&pos, .3f, &n)) {
			add_ship_trail(&pos, &ship.pos);
			perturb_water(&pos, 50.f);
			ship.pos = pos;
		}

		ship.angle = 360.f*atan2(-dir.x, dir.y)/2.f/M_PI;
	}

	a = fmod360(ship.angle);
	b = fmod360(ship.cur_angle);

	if (a != b) {
		float d = a - b;

		if (fabs(d) <= ANGLE_DELTA) {
			ship.cur_angle = ship.angle;
		} else {
			float e;

			if (d > 0) {
				if (d < 180)
					e = ANGLE_DELTA;
				else
					e = -ANGLE_DELTA;
			} else {
				if (-d < 180)
					e = -ANGLE_DELTA;
				else
					e = ANGLE_DELTA;
			}

			if (e > 0.f)
				ship.tilt = -MAX_TILT_ANGLE;
			else
				ship.tilt = MAX_TILT_ANGLE;

			ship.cur_angle = fmod360(ship.cur_angle + e);
		}
	} else {
		if (fabs(ship.tilt *= .7) < .1f)
			ship.tilt = 0.f;
	}

	get_ship_firing_stick_direction(&fire_dir);

	if ((fire_dir.x != 0 || fire_dir.y != 0) && ship.refire_count <= 0) {
		fire_missile(&fire_dir);
		ship.refire_count = tics_before_refire();
	} else {
		ship.refire_count--;
	}

	for (i = 0; i < NUM_POWERUP_TYPES; i++) {
		if (ship.powerup_ttl[i] > 0) {
			--ship.powerup_ttl[i];
		}
	}
}

static void
initialize_textures(void)
{
	if (ship_settings == NULL || ship_settings->mesh == NULL || ship_settings->shield_sprite == NULL)
		panic("ship settings not properly initialized");

	ship_mesh = mesh_load_obj(ship_settings->mesh->source);
	mesh_transform(ship_mesh, ship_settings->mesh->transform);
	mesh_setup(ship_mesh);

	shield_texture_id =
	  png_to_texture(ship_settings->shield_sprite->source);

	ship_outline_texture_id =
	  png_to_texture("spaceship-white.png");

	missile_glow_texture_id = png_to_texture("shot-particle.png");
	power_missile_glow_texture_id =
	  png_to_texture("power-shot-particle.png");
}

void
initialize_ship(void)
{
	initialize_textures();
}

void
ship_add_powerup(int type)
{
	if (type == PU_EXTRA_SHIP) {
		gc.ships_left++;
		play_fx(FX_POWERUP_2);
	} else {
		ship.powerup_ttl[type] = initial_powerup_ttl[type];
		play_fx(FX_POWERUP_1);
	}
}

int
ship_has_powerup(int type)
{
	return settings.static_settings->all_powerups ||
	  ship.powerup_ttl[type] > 0;
}

#define PARTICLE_SPEED .2f

static void
gen_ship_explosion(void)
{
	int i;
	particle *p;

	for (i = 0; i < 1200; i++) {
		vector2 offs;

		p = add_particle();

		if (!p)
			break;

		p->ttl = 30 + irand(30);
		p->t = 0;
		p->width = .3f*(.6f + .1f*frand());
		p->height = .3f*(2.8f + .4f*frand());
		p->pos = ship.pos;
		p->palette = PAL_BLUE;

		vec2_set(&p->dir, frand() - .5f, frand() - .5f);
		vec2_normalize(&p->dir);

		offs = p->dir;
		vec2_mul_scalar(&offs, 1.3f*frand());
		vec2_add_to(&p->pos, &offs);

		p->speed = PARTICLE_SPEED + .05f*frand();
		p->friction = .96f;
	}

	perturb_water(&ship.pos, 3000.f);

	play_fx(FX_EXPLOSION_2);
}

void
hit_ship(const vector2 *pos, float radius)
{
	float r2 = (radius + SHIP_RADIUS)*(radius + SHIP_RADIUS);

	if ((!settings.static_settings ||
	  !settings.static_settings->god_mode) &&
	  ship.is_alive &&
	  !ship_has_powerup(PU_SHIELD) &&
	  vec2_distance_squared(pos, &ship.pos) < r2) {
		gen_ship_explosion();
		ship.is_alive = 0;
	}
}

void
serialize_ship(FILE *out)
{
	fwrite(&ship, sizeof ship, 1, out);
}

int
unserialize_ship(const char *data)
{
	memcpy(&ship, data, sizeof ship);
	return sizeof ship;
}
