#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <GL/glew.h>

#include "settings.h"
#include "matrix.h"
#include "vector.h"
#include "mesh.h"
#include "gl_util.h"
#include "panic.h"
#include "dna.h"

#define SCALE .111337f

#define INNER_TUBE_STEPS 5 /* 8 */
#define INNER_TUBE_SEGS 7 /* 12 */
#define INNER_TUBE_RADIUS SCALE*2.8f
#define INNER_TUBE_LENGTH SCALE*50.f

#define OUTER_TUBE_STEPS 8
#define OUTER_TUBE_RADIUS SCALE*7.f
#define CURVE_PERIOD 4.f*M_PI

#define NUM_STEPS (380/2) /* 450 */

static int texture_id;

static vector3 outer_coords[OUTER_TUBE_STEPS];

static const struct material outer_tube_material =
  { { .1f, .1f, .1f }, { .8f, .8f, .8f }, { 1, 1, 1 }, 95 };

static const struct material inner_tube_material =
  { { .1f, .1f, .1f }, { .3f, .3f, .3f }, { .45f, .45f, .45f }, 5 };

struct gl_vertex_texuv {
	GLfloat position[3];
	GLfloat normal[3];
	GLfloat texuv[2];
};

struct gl_vertex {
	GLfloat position[3];
	GLfloat normal[3];
};

static struct {
	GLushort quad_indices[4*NUM_STEPS*OUTER_TUBE_STEPS];
	struct gl_vertex_texuv vertices[2][(NUM_STEPS + 1)*(OUTER_TUBE_STEPS + 1)];
} outer_tube_vertex_array;

static GLuint outer_tube_vertex_vbo_id[2];
static GLuint outer_tube_index_vbo_id;

static struct {
	GLushort quad_indices[4*INNER_TUBE_STEPS*(INNER_TUBE_SEGS - 1)];
	struct gl_vertex vertices[INNER_TUBE_STEPS*INNER_TUBE_SEGS];
} inner_tube_vertex_array;

static GLuint inner_tube_vertex_vbo_id;
static GLuint inner_tube_index_vbo_id;

static vector3 curve_coords[NUM_STEPS];
static matrix curve_axes[NUM_STEPS];

static void
initialize_inner_tube(void)
{
	int i, j;
	float a, da, s, c;
	static vector2 outline[INNER_TUBE_SEGS]; /* x and y */
	static vector2 normal[INNER_TUBE_SEGS];
	struct gl_vertex *p;
	GLushort *q;

	/* outline */

	outline[0].y = outline[INNER_TUBE_SEGS - 1].y = INNER_TUBE_RADIUS;
	outline[0].x = -.5f*INNER_TUBE_LENGTH;
	outline[INNER_TUBE_SEGS - 1].x = .5f*INNER_TUBE_LENGTH;

#define M 2.f*SCALE

	a = 0.f;
	da = .5f*M_PI/((INNER_TUBE_SEGS - 2)/2);

	for (i = 0; i < (INNER_TUBE_SEGS - 2)/2; i++) {
		float y = INNER_TUBE_RADIUS*cos(a);
		float x = -M + M*sin(a);

		outline[i + 1].y = outline[INNER_TUBE_SEGS - 2 - i].y = y;
		outline[i + 1].x = x;
		outline[INNER_TUBE_SEGS - 2 - i].x = -x;

		a += da;
	}

	normal[0].x = normal[INNER_TUBE_SEGS - 1].x = 0.f;
	normal[0].y = normal[INNER_TUBE_SEGS - 1].y = 1.f;

	for (i = 1; i < INNER_TUBE_SEGS - 1; i++) {
		normal[i].x = -(outline[i + 1].y - outline[i].y) +
		  -(outline[i].y - outline[i - 1].y);
		normal[i].y = (outline[i + 1].x - outline[i].x) +
		  (outline[i].x - outline[i - 1].x);
		vec2_normalize(&normal[i]);
	}

	/* 3d geometry */ 

	da = 2.f*M_PI/INNER_TUBE_STEPS;
	a = 0.f;

	p = inner_tube_vertex_array.vertices;

	for (i = 0; i < INNER_TUBE_STEPS; i++) {
		s = sin(a);
		c = cos(a);

		for (j = 0; j < INNER_TUBE_SEGS; j++) {
			p->position[1] = outline[j].x;
			p->position[2] = s*outline[j].y;
			p->position[0] = c*outline[j].y;

			p->normal[1] = normal[j].x;
			p->normal[2] = s*normal[j].y;
			p->normal[0] = c*normal[j].y;

			p++;
		}

		a += da;
	}

	assert(p == &inner_tube_vertex_array.vertices[INNER_TUBE_SEGS*INNER_TUBE_STEPS]);

	q = inner_tube_vertex_array.quad_indices;

	for (i = 0; i < INNER_TUBE_STEPS; i++) {
		for (j = 0; j < INNER_TUBE_SEGS - 1; j++) {
			const int k = i < INNER_TUBE_STEPS - 1 ? i + 1 : 0;

			*q++ = k*INNER_TUBE_SEGS + j;
			*q++ = k*INNER_TUBE_SEGS + j + 1;
			*q++ = i*INNER_TUBE_SEGS + j + 1;
			*q++ = i*INNER_TUBE_SEGS + j;
		}
	}

	assert(q == &inner_tube_vertex_array.quad_indices[4*INNER_TUBE_STEPS*(INNER_TUBE_SEGS - 1)]);

	if (settings.static_settings->use_vbo) {
		/* upload data to vbos */

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, inner_tube_vertex_vbo_id);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, inner_tube_index_vbo_id);

		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(inner_tube_vertex_array.vertices),
		  inner_tube_vertex_array.vertices,
		  GL_STATIC_DRAW_ARB);

		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(inner_tube_vertex_array.quad_indices),
		  inner_tube_vertex_array.quad_indices,
		  GL_STATIC_DRAW_ARB);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}
}

static void
initialize_outer_tube(void)
{
	int i;
	float a, da, s, c;

	da = 2.f*M_PI/OUTER_TUBE_STEPS;
	a = 0.f;

	for (i = 0; i < OUTER_TUBE_STEPS; i++) {
		s = sin(a);
		c = cos(a);

		outer_coords[i].x = 1.2*OUTER_TUBE_RADIUS*s;
		outer_coords[i].y = .4*OUTER_TUBE_RADIUS*c;
		outer_coords[i].z = 0.f;

		a += da;
	}
}

static void
initialize_texture(void)
{
	if (!texture_id)
		texture_id = png_to_texture("dna-texture.png");
}

static void
initialize_ribbon_quad_indices(void)
{
	GLushort *p;
	int i, j;

	p = outer_tube_vertex_array.quad_indices;

	for (j = 0; j < NUM_STEPS; j++) {
		for (i = 0; i < OUTER_TUBE_STEPS; i++) {
			*p++ = j*(OUTER_TUBE_STEPS + 1) + i;
			*p++ = j*(OUTER_TUBE_STEPS + 1) + (i + 1);
			*p++ = (j + 1)*(OUTER_TUBE_STEPS + 1) + (i + 1);
			*p++ = (j + 1)*(OUTER_TUBE_STEPS + 1) + i;
		}
	}

	if (settings.static_settings->use_vbo) {
		/* upload data to vbo */

		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, outer_tube_index_vbo_id);

		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(outer_tube_vertex_array.quad_indices),
		  outer_tube_vertex_array.quad_indices,
		  GL_STATIC_DRAW_ARB);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, outer_tube_vertex_vbo_id[0]);

		/* allocate only */
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(outer_tube_vertex_array.vertices),
		  0, GL_STATIC_DRAW_ARB);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, outer_tube_vertex_vbo_id[1]);

		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(outer_tube_vertex_array.vertices),
		  0, GL_STATIC_DRAW_ARB);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}
}

static void
initialize_vbo(void)
{
	glGenBuffersARB(1, &inner_tube_vertex_vbo_id);
	glGenBuffersARB(1, &inner_tube_index_vbo_id);

	glGenBuffersARB(2, outer_tube_vertex_vbo_id);
	glGenBuffersARB(1, &outer_tube_index_vbo_id);
}

static void
tear_down_vbo(void)
{
	glDeleteBuffersARB(1, &inner_tube_vertex_vbo_id);
	glDeleteBuffersARB(1, &inner_tube_index_vbo_id);

	glDeleteBuffersARB(2, outer_tube_vertex_vbo_id);
	glDeleteBuffersARB(1, &outer_tube_index_vbo_id);
}

static void
update_vertex_buffers(void);

void
initialize_dna(void)
{
	if (settings.static_settings->use_vbo)
		initialize_vbo();
	initialize_texture();
	initialize_inner_tube();
	initialize_outer_tube();
	initialize_ribbon_quad_indices();
	update_vertex_buffers();
}

static float delta_s = 0.f;
static float delta_v = 0.f;
static float fc = 0.f;		/* WHAT THE FUCK IS THIS */

static void
get_curve_coords_and_axes_at(float t, float s, vector3 *coord, matrix *axes);

static void
initialize_outer_tube_vertices(struct gl_vertex_texuv *vertices, float x_offset);

static void
update_vertex_buffers(void)
{
	float t, dt;
	int i;

	t = 0.f;
	dt = CURVE_PERIOD/NUM_STEPS;

	for (i = 0; i < NUM_STEPS; i++) {
		get_curve_coords_and_axes_at(t, 6.f*t + delta_s,
		  &curve_coords[i], &curve_axes[i]);
		t += dt;
	}

	initialize_outer_tube_vertices(outer_tube_vertex_array.vertices[0], .5f*INNER_TUBE_LENGTH);
	initialize_outer_tube_vertices(outer_tube_vertex_array.vertices[1], -.5f*INNER_TUBE_LENGTH);

	if (settings.static_settings->use_vbo) {
		/* yes, I'm aware of glMapBufferARB. are *you* aware that it explodes with Intel's drivers? */
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, outer_tube_vertex_vbo_id[0]);
		glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(outer_tube_vertex_array.vertices),
		  outer_tube_vertex_array.vertices[0]);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, outer_tube_vertex_vbo_id[1]);
		glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, 0, sizeof(outer_tube_vertex_array.vertices),
		  outer_tube_vertex_array.vertices[1]);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
}

void
update_dna(void)
{
	delta_s += .025f;
	fc += .05f;

	update_vertex_buffers();
}

void
reset_dna(void)
{
	delta_s = delta_v = 0.f;
	fc = 0.f;
}

static void
get_curve_at(vector3 *v, float t)
{
	float c, s, c15, s15;

	c = cos(t);
	s = sin(t);
	c15 = cos(1.5*t);
	s15 = sin(1.5*t);

	v->x = SCALE*60.f*(2.f + c15)*c;
	v->y = SCALE*60.f*(2.f + c15)*s;
	v->z = SCALE*60.f*s15;
}

static void
get_curve_direction_at(vector3 *v, float t)
{
	vector3 t0, t1;

	get_curve_at(&t1, t + 1e-3);
	get_curve_at(&t0, t);

	vec3_sub(v, &t1, &t0);

	vec3_normalize(v);
}

static void
get_curve_coords_and_axes_at(float t, float s,
			  vector3 *coord, matrix *axes)
{
	vector3 p, q, u;
	vector3 bz, bx, by;
	float m, s0, c0;

	get_curve_at(&p, t);

	get_curve_direction_at(&bz, t);
	vec3_normalize(&bz);

	q = p;
	vec3_normalize(&q);

	m = vec3_dot_product(&q, &bz);

	q.x -= m*bz.x;
	q.y -= m*bz.y;
	q.z -= m*bz.z;
	vec3_normalize(&q);

	vec3_cross_product(&u, &q, &bz);

	/* orthonormal base: u, q, d */

	/* now rotate q and u around d by s radians */

	c0 = cos(s);
	s0 = sin(s);

	bx.x = c0*q.x + s0*u.x;
	bx.y = c0*q.y + s0*u.y;
	bx.z = c0*q.z + s0*u.z;

	vec3_cross_product(&by, &bx, &bz);

	*coord = p;

        axes->m11 = bx.x; axes->m12 = by.x; axes->m13 = bz.x; axes->m14 = 0.f;
        axes->m21 = bx.y; axes->m22 = by.y; axes->m23 = bz.y; axes->m24 = 0.f;
        axes->m31 = bx.z; axes->m32 = by.z; axes->m33 = bz.z; axes->m34 = 0.f;
}

static void
draw_inner_tube(matrix *om, vector3 *p)
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(p->x, p->y, p->z);
	set_opengl_matrix(om);

	glDrawElements(GL_QUADS, 4*INNER_TUBE_STEPS*(INNER_TUBE_SEGS - 1),
	  GL_UNSIGNED_SHORT, 
	  settings.static_settings->use_vbo ? NULL : inner_tube_vertex_array.quad_indices);

	glPopMatrix();
}

static void
initialize_outer_tube_vertices(struct gl_vertex_texuv *vertices, float x_offset)
{
	int i, j;
	struct gl_vertex_texuv *p;
	float u, du, v, dv;

	/* vertices */

	p = vertices;

	for (j = 0; j < NUM_STEPS + 1; j++) {
		const struct matrix *om = &curve_axes[j % NUM_STEPS];
		const struct vector3 *o = &curve_coords[j % NUM_STEPS];

		for (i = 0; i < OUTER_TUBE_STEPS + 1; i++) {
			vector3 v;

			mat_rotate_copy(&v, om,
			  &outer_coords[i % OUTER_TUBE_STEPS]);

			p->position[0] = v.x + om->m12*x_offset + o->x;
			p->position[1] = v.y + om->m22*x_offset + o->y;
			p->position[2] = v.z + om->m32*x_offset + o->z;

			++p;
		}
	}

	/* normals */

	p = vertices;

	for (j = 0; j < NUM_STEPS + 1; j++) {
		for (i = 0; i < OUTER_TUBE_STEPS + 1; i++) {
			const GLfloat *p0, *p1, *p2;
			vector3 a, b, n;

			p0 = vertices[j*(OUTER_TUBE_STEPS + 1) + i].position;
			p1 = vertices[j*(OUTER_TUBE_STEPS + 1) +
			    (i + 1)%OUTER_TUBE_STEPS].position;
			p2 = vertices[
			  ((j + 1)%NUM_STEPS)*(OUTER_TUBE_STEPS + 1) + i].
			  position;

			a.x = p1[0] - p0[0];
			a.y = p1[1] - p0[1];
			a.z = p1[2] - p0[2];

			b.x = p2[0] - p0[0];
			b.y = p2[1] - p0[1];
			b.z = p2[2] - p0[2];

			vec3_cross_product(&n, &a, &b);
			vec3_normalize(&n);

			p->normal[0] = n.x;
			p->normal[1] = n.y;
			p->normal[2] = n.z;

			++p;
		}
	}

	/* texture uv */

	p = vertices;

	v = 0.f;
	dv = 100.f/NUM_STEPS;

	for (j = 0; j < NUM_STEPS + 1; j++) {
		u = 0.f;
		du = 1.f/OUTER_TUBE_STEPS;

		for (i = 0; i < OUTER_TUBE_STEPS + 1; i++) {
			p->texuv[0] = u;
			p->texuv[1] = v;

			u += du;

			++p;
		}

		v += dv;
	}
}

static void
draw_outer_tube(struct gl_vertex_texuv *vertices)
{
	struct gl_vertex_texuv *bv;

	bv = settings.static_settings->use_vbo ? NULL : vertices;

	glVertexPointer(3, GL_FLOAT, sizeof(struct gl_vertex_texuv),
	  &bv->position);

	glNormalPointer(GL_FLOAT, sizeof(struct gl_vertex_texuv),
	  &bv->normal);

	glTexCoordPointer(2, GL_FLOAT, sizeof(struct gl_vertex_texuv),
	  &bv->texuv);

	glDrawElements(GL_QUADS, 4*NUM_STEPS*OUTER_TUBE_STEPS,
	  GL_UNSIGNED_SHORT,
	  settings.static_settings->use_vbo ? NULL : outer_tube_vertex_array.quad_indices);
}

void
draw_dna(void)
{
	int i;
	struct gl_vertex *bv;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	/* inner tubes */

	glDisable(GL_TEXTURE_2D);
	set_opengl_material(&inner_tube_material, 1.f);

	glColor3f(1, 1, 1);

	if (settings.static_settings->use_vbo) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, inner_tube_vertex_vbo_id);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, inner_tube_index_vbo_id);
		bv = NULL;
	} else {
		bv = inner_tube_vertex_array.vertices;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	glVertexPointer(3, GL_FLOAT, sizeof(struct gl_vertex), &bv->position);
	glNormalPointer(GL_FLOAT, sizeof(struct gl_vertex), &bv->normal);

	for (i = 0; i < NUM_STEPS; i += /* 4 */ 2)
		draw_inner_tube(&curve_axes[i], &curve_coords[i]);

	/* outer tubes */

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(texture_id));

	set_opengl_material(&outer_tube_material, 1.f);

	if (settings.static_settings->use_vbo) {
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, outer_tube_index_vbo_id);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, outer_tube_vertex_vbo_id[0]);
	}

	draw_outer_tube(outer_tube_vertex_array.vertices[0]);

	if (settings.static_settings->use_vbo)
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, outer_tube_vertex_vbo_id[1]);

	draw_outer_tube(outer_tube_vertex_array.vertices[1]);

	if (settings.static_settings->use_vbo) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	glPopAttrib();
}

void
tear_down_dna(void)
{
	if (settings.static_settings->use_vbo)
		tear_down_vbo();
}
