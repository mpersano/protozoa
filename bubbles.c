#include <stdio.h>
#include <assert.h>
#include <GL/glew.h>

#include "settings.h"
#include "mesh.h"
#include "gl_util.h"
#include "background.h"
#include "common.h"
#include "vector.h"
#include "bubbles.h"

enum {
	SPHERE_DENSITY = 20,
	MAX_BUBBLES = 20,
	BUBBLE_FRAMES = 20
};

static const float MIN_Y = 0.f;
static const float MAX_Y = 5.f;
static const float cell_radius = .9f*CELL_RADIUS;
static const float SPHERE_RADIUS = .4;

static struct sphere_vertex {
	GLfloat pos[3];
	GLfloat normal[3];
} sphere_verts[BUBBLE_FRAMES][SPHERE_DENSITY*SPHERE_DENSITY];

static GLushort sphere_indices[4*SPHERE_DENSITY*SPHERE_DENSITY];

struct bubble {
	vector3 pos;
	int frame;
	float dy;
};

static struct bubble bubbles[MAX_BUBBLES];

static GLuint bubble_vertex_vbo_id;
static GLuint bubble_index_vbo_id;

static const struct material sphere_material =
  { { 0, 0, 0 }, { .05f, .05f, .05f }, { .1f, .1f, .1f }, 20 };

static void
draw_bubble(const struct bubble *b)
{
	struct sphere_vertex *base;

	glPushMatrix();
	glTranslatef(b->pos.x, b->pos.y, b->pos.z);

	if (settings.static_settings->use_vbo) {
		assert(b->frame >= 0 && b->frame < BUBBLE_FRAMES);
		base = &((struct sphere_vertex *)NULL)[b->frame*SPHERE_DENSITY*SPHERE_DENSITY];
	} else {
		base = sphere_verts[b->frame];
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(struct sphere_vertex), &base->pos);

	glEnableClientState(GL_NORMAL_ARRAY);
	glNormalPointer(GL_FLOAT, sizeof(struct sphere_vertex), &base->normal);

	glDrawElements(GL_QUADS, 4*SPHERE_DENSITY*SPHERE_DENSITY, GL_UNSIGNED_SHORT,
	  settings.static_settings->use_vbo ? NULL : sphere_indices);

	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);

	glPopMatrix();
}

void
draw_bubbles(void)
{
#if 1
	const struct bubble *b;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glShadeModel(GL_SMOOTH);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glDisable(GL_TEXTURE_2D);

	set_opengl_material(&sphere_material, .2f);

	if (settings.static_settings->use_vbo) {
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bubble_index_vbo_id);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, bubble_vertex_vbo_id);
	}

	for (b = bubbles; b != &bubbles[MAX_BUBBLES]; b++)
		draw_bubble(b);

	if (settings.static_settings->use_vbo) {
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}

	glPopAttrib();
#endif
}

static void initialize_bubble(struct bubble *b);

static void
update_bubble(struct bubble *b)
{
	b->pos.y += b->dy;

	b->frame = (b->frame + 1)%BUBBLE_FRAMES;

	if (vec3_length_squared(&b->pos) > cell_radius*cell_radius)
		initialize_bubble(b);
}

void
update_bubbles(void)
{
	struct bubble *b;

	for (b = bubbles; b != &bubbles[MAX_BUBBLES]; b++)
		update_bubble(b);
}

static void
sphere_point(vector3 *p, float phi, float theta, float a, float b, float c)
{
	float z = SPHERE_RADIUS*sin(phi);
	float mr = SPHERE_RADIUS*cos(phi);
	float d = 1. + a*sin(b*(z/SPHERE_RADIUS) + c);

	p->x = d*mr*cos(theta);
	p->y = d*z;
	p->z = d*mr*sin(theta);
}

static void
gen_sphere(float a, float b)
{
	int i, j, k;
	struct sphere_vertex *p;
	GLushort *q;
	float c;

	if (settings.static_settings->use_vbo) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, bubble_vertex_vbo_id);

		glBufferDataARB(GL_ARRAY_BUFFER_ARB,
		  BUBBLE_FRAMES*SPHERE_DENSITY*SPHERE_DENSITY*sizeof(struct sphere_vertex),
		  0, GL_STATIC_DRAW_ARB);
	}

	for (k = 0; k < BUBBLE_FRAMES; k++) {
		p = sphere_verts[k];
		c = 2.*M_PI*k/BUBBLE_FRAMES;

		for (i = 0; i < SPHERE_DENSITY; i++) {
			float phi = -.5f*M_PI + M_PI*i/SPHERE_DENSITY;

			for (j = 0; j < SPHERE_DENSITY; j++) {
				const float EPSILON = 1e-3;
				float theta = 2.f*M_PI*j/SPHERE_DENSITY;
				vector3 p0, p1, p2, u, v, n;

				sphere_point(&p0, phi, theta, a, b, c);
				p->pos[0] = p0.x;
				p->pos[1] = p0.y;
				p->pos[2] = p0.z;

				sphere_point(&p1, phi + EPSILON, theta, a, b, c);
				sphere_point(&p2, phi, theta + EPSILON, a, b, c);

				vec3_sub(&u, &p1, &p0);
				vec3_sub(&v, &p2, &p0);
				vec3_cross_product(&n, &u, &v);
				vec3_normalize(&n);

				p->normal[0] = n.x;
				p->normal[1] = n.y;
				p->normal[2] = n.z;

				++p;
			}
		}

		assert(p == &sphere_verts[k][SPHERE_DENSITY*SPHERE_DENSITY]);

		if (settings.static_settings->use_vbo) {
			glBufferSubDataARB(GL_ARRAY_BUFFER_ARB,
			  k*SPHERE_DENSITY*SPHERE_DENSITY*sizeof(struct sphere_vertex),
			  SPHERE_DENSITY*SPHERE_DENSITY*sizeof(struct sphere_vertex),
			  sphere_verts[k]);
		}
	}

	if (settings.static_settings->use_vbo)
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	q = sphere_indices;

	for (i = 0; i < SPHERE_DENSITY; i++) {
		for (j = 0; j < SPHERE_DENSITY; j++) {
			*q++ = i*SPHERE_DENSITY + j;
			*q++ = ((i + 1)%SPHERE_DENSITY)*SPHERE_DENSITY + j;
			*q++ = ((i + 1)%SPHERE_DENSITY)*SPHERE_DENSITY + (j + 1)%SPHERE_DENSITY;
			*q++ = i*SPHERE_DENSITY + (j + 1)%SPHERE_DENSITY;
		}
	}

	if (settings.static_settings->use_vbo) {
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bubble_index_vbo_id);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(sphere_indices),
		  sphere_indices, GL_STATIC_DRAW_ARB);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}
}

static void
initialize_bubble(struct bubble *b)
{
	vector3 d;

	d.x = frand() - .5f;
	d.y = -.5f*frand();
	d.z = frand() - .5f;
	vec3_normalize(&d);

	b->pos.x = cell_radius*d.x;
	b->pos.y = cell_radius*d.y;
	b->pos.z = cell_radius*d.z;

	b->dy = .05f + .1*frand();

	b->frame = 0;
}

void
initialize_vbo(void)
{
	glGenBuffersARB(1, &bubble_vertex_vbo_id);
	glGenBuffersARB(1, &bubble_index_vbo_id);
}

void
tear_down_vbo(void)
{
	glDeleteBuffersARB(1, &bubble_vertex_vbo_id);
	glDeleteBuffersARB(1, &bubble_index_vbo_id);
}

void
initialize_bubbles_resources(void)
{
	if (settings.static_settings->use_vbo)
		initialize_vbo();

	gen_sphere(5*.04, 8*.23);
}

void
tear_down_bubbles_resources(void)
{
	if (settings.static_settings->use_vbo)
		tear_down_vbo();
}

void
initialize_bubbles(void)
{
	struct bubble *b;

	initialize_bubbles_resources();

	for (b = bubbles; b != &bubbles[MAX_BUBBLES]; b++)
		initialize_bubble(b);
}

void
tear_down_bubbles(void)
{
	tear_down_bubbles_resources();
}
