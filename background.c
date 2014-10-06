#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <GL/glew.h>

#include "settings.h"
#include "image.h"
#include "gl_util.h"
#include "common.h"
#include "panic.h"
#include "mesh.h"
#include "arena.h"
#include "dna.h"
#include "bubbles.h"
#include "background.h"

#define SPHERE_RADIUS (.98*CELL_RADIUS)
#define SPHERE_DENSITY 30

static float rot_z;
static float sphere_rot_y;

static int skybox_texture_id;
static int halo_texture_id;
static int sphere_texture_id;

static struct sphere_vertex {
	GLfloat pos[3];
	GLfloat normal[3];
	GLfloat texuv[2];
} sphere_verts[(SPHERE_DENSITY + 1)*(SPHERE_DENSITY + 1)];

static GLushort tri_strip_indices[SPHERE_DENSITY][2*(SPHERE_DENSITY + 1)];

static GLuint sphere_vertex_vbo_id;
static GLuint sphere_index_vbo_id[SPHERE_DENSITY];

#define SKYBOX_DIST 2000.f

static const struct material sphere_material =
  { { 0, 0, 0 }, { .4f, .4f, .4f }, { 1.f, 1.f, 1.f }, 65 };

void
reset_background(void)
{
	rot_z = 0.f;
}

void
draw_skybox(void)
{
	static GLfloat vertices[][3] = {
		{ SKYBOX_DIST, SKYBOX_DIST, -SKYBOX_DIST },
		{ SKYBOX_DIST, SKYBOX_DIST, SKYBOX_DIST },
		{ SKYBOX_DIST, -SKYBOX_DIST, SKYBOX_DIST },
		{ SKYBOX_DIST, -SKYBOX_DIST, -SKYBOX_DIST },
		{ -SKYBOX_DIST, SKYBOX_DIST, -SKYBOX_DIST },
		{ -SKYBOX_DIST, SKYBOX_DIST, SKYBOX_DIST },
		{ -SKYBOX_DIST, -SKYBOX_DIST, SKYBOX_DIST },
		{ -SKYBOX_DIST, -SKYBOX_DIST, -SKYBOX_DIST },
	};
	static int skybox[][4] = {
		{ 3, 7, 6, 2 },
		{ 7, 4, 5, 6 },
		{ 4, 0, 1, 5 },
		{ 0, 3, 2, 1 },
		{ 5, 1, 2, 6 },
		{ 7, 3, 0, 4 },
	};
	int i;

	glShadeModel(GL_FLAT);
	glDisable(GL_LIGHTING);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glColor3f(1, 1, 1);
	glPushMatrix();
	glTranslatef(0, 0, 0);

	/* glRotatef(.15f*rot_z, 1.f, .8f, .2f); */
	glRotatef(.15f*rot_z, 1.f, 1.f, .8f);
	glRotatef(90.f, 1, 0, 0);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(skybox_texture_id));

	for (i = 0; i < 6; i++) {
		glBegin(GL_QUADS);

		glTexCoord2f(0.0f, 0.0f); 
		glVertex3fv(vertices[skybox[i][0]]);

		glTexCoord2f(1.0f, 0.0f);
		glVertex3fv(vertices[skybox[i][1]]);

		glTexCoord2f(1.0f, 1.0f);
		glVertex3fv(vertices[skybox[i][2]]);

		glTexCoord2f(0.0f, 1.0f);
		glVertex3fv(vertices[skybox[i][3]]);

		glEnd();
	}

	glPopMatrix();
}

static void
draw_arena(const struct arena *arena, float arena_alpha)
{
	float s = arena->center.x;
	float t = arena->center.y;

	glPushMatrix();
	glRotatef(t, 0, 1, 0);
	glRotatef(s, 1, 0, 0);
	glTranslatef(0, 0, CELL_RADIUS);

	draw_arena_outline(arena, arena_alpha, arena->border_type);

	glPopMatrix();
}

static void
draw_arenas(int level, float arena_alpha)
{
	const struct level *l = levels[level];
	struct wave **p;

	for (p = l->waves; p != &l->waves[l->num_waves]; p++)
		draw_arena((*p)->arena, arena_alpha);
}

static void
draw_halo(void)
{
	float mv[16], s;

	/* kill rotations */
	glPushMatrix();
	glGetFloatv(GL_MODELVIEW_MATRIX, mv);
	mv[0] = 1.f; mv[1] = 0.f; mv[2] = 0.f;
	mv[4] = 0.f; mv[5] = 1.f; mv[6] = 0.f;
	mv[8] = 0.f; mv[9] = 0.f; mv[10] = 1.f;
	glLoadMatrixf(mv);

	glDisable(GL_LIGHTING);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(halo_texture_id));

	glColor3f(1, 1, 1);

	s = 1.05*(1./.8f)*SPHERE_RADIUS;

	glBegin(GL_QUADS);

	glTexCoord2f(0, 1);
	glVertex3f(-s, -s, 0.f);

	glTexCoord2f(1, 1);
	glVertex3f(s, -s, 0.f);

	glTexCoord2f(1, 0);
	glVertex3f(s, s, 0.f);

	glTexCoord2f(0, 0);
	glVertex3f(-s, s, 0.f);

	glEnd();

	glDisable(GL_TEXTURE_2D);

	glPopMatrix();
}

static void gen_sphere(void);

static void
draw_sphere(int level, float arena_alpha)
{
	int i;
	struct sphere_vertex *base;

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	/* sphere */

	glShadeModel(GL_SMOOTH);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(sphere_texture_id));

	set_opengl_material(&sphere_material, 1.f);

	glPushMatrix();
	glRotatef(sphere_rot_y, 0, 1, 0);

	if (settings.static_settings->use_vbo) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, sphere_vertex_vbo_id);
		base = NULL;
	} else {
		base = sphere_verts;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(struct sphere_vertex),
	  &base->pos);

	glEnableClientState(GL_NORMAL_ARRAY);
	glNormalPointer(GL_FLOAT, sizeof(struct sphere_vertex),
	  &base->normal);

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(struct sphere_vertex),
	  &base->texuv);

	if (settings.static_settings->use_vbo) {
		for (i = 0; i < SPHERE_DENSITY; i++) {
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sphere_index_vbo_id[i]);
			glDrawElements(GL_TRIANGLE_STRIP, 2*(SPHERE_DENSITY + 1),
					GL_UNSIGNED_SHORT, 0);
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		}
	} else {
		for (i = 0; i < SPHERE_DENSITY; i++) {
			glDrawElements(GL_TRIANGLE_STRIP, 2*(SPHERE_DENSITY + 1),
					GL_UNSIGNED_SHORT, tri_strip_indices[i]);
		}
	}

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	if (settings.static_settings->use_vbo)
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

	glPopMatrix();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	draw_halo();

	if (arena_alpha > 0.f)
		draw_arenas(level, arena_alpha);

	glDisable(GL_BLEND);
}

void
draw_background(int level, float arena_alpha, int bubbles)
{
	draw_skybox();

	glShadeModel(GL_SMOOTH);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_CULL_FACE);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(0.f, 0.f, -CELL_RADIUS);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glPushMatrix();
	glRotatef(rot_z, .2f, 1.f, .8f);
	draw_dna();
	glPopMatrix();
	if (bubbles)
		draw_bubbles();
	draw_sphere(level, arena_alpha);

	glPopMatrix();
}

void
update_background(void)
{
	rot_z += .4f;
	sphere_rot_y -= .3f;
	update_dna();
	update_bubbles();
}

static void
gen_sphere(void)
{
	struct sphere_vertex *p;
	int i, j;

	p = sphere_verts;

	for (i = 0; i < SPHERE_DENSITY + 1; i++) {
		float phi = -.5f*M_PI + M_PI*i/SPHERE_DENSITY;
		float z = SPHERE_RADIUS*sin(phi);
		float mr = SPHERE_RADIUS*cos(phi);

		for (j = 0; j < SPHERE_DENSITY + 1; j++) {
			float theta = 2.f*M_PI*j/SPHERE_DENSITY;

			p->pos[1] = z;
			p->pos[0] = mr*cos(theta);
			p->pos[2]= mr*sin(theta);

			p->normal[0] = p->pos[0]/SPHERE_RADIUS;
			p->normal[1] = p->pos[1]/SPHERE_RADIUS;
			p->normal[2] = p->pos[2]/SPHERE_RADIUS;

			p->texuv[1] = (float)2.f*i/SPHERE_DENSITY;
			p->texuv[0] = (float)2.f*j/SPHERE_DENSITY;

			++p;
		}
	}

	assert(p == &sphere_verts[(SPHERE_DENSITY + 1)*(SPHERE_DENSITY + 1)]);

	for (i = 0; i < SPHERE_DENSITY; i++) {
		GLushort *p = tri_strip_indices[i];

		for (j = 0; j < SPHERE_DENSITY + 1; j++) {
			*p++ = i*(SPHERE_DENSITY + 1) + j;
			*p++ = (i + 1)*(SPHERE_DENSITY + 1) + j;
		}
	}

	if (settings.static_settings->use_vbo) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, sphere_vertex_vbo_id);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(sphere_verts),
		  sphere_verts, GL_STATIC_DRAW_ARB);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

		for (i = 0; i < SPHERE_DENSITY; i++) {
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sphere_index_vbo_id[i]);
			glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(tri_strip_indices[i]),
			  tri_strip_indices[i], GL_STATIC_DRAW_ARB);
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		}
	}
} 

static void
initialize_vbo(void)
{
	glGenBuffersARB(1, &sphere_vertex_vbo_id);
	glGenBuffersARB(SPHERE_DENSITY, sphere_index_vbo_id);
}

static void
tear_down_vbo(void)
{
	glDeleteBuffersARB(1, &sphere_vertex_vbo_id);
	glDeleteBuffersARB(SPHERE_DENSITY, sphere_index_vbo_id);
}

static void
initialize_sphere(void)
{
	if (settings.static_settings->use_vbo)
		initialize_vbo();

	gen_sphere();
}

static void
tear_down_sphere(void)
{
	if (settings.static_settings->use_vbo)
		tear_down_vbo();
}

void
initialize_textures(void)
{
	if (!skybox_texture_id)
		skybox_texture_id = png_to_texture("bg.png");

	if (!halo_texture_id)
	halo_texture_id = png_to_texture("halo.png");

	if (!sphere_texture_id)
		sphere_texture_id = png_to_texture("sphere-texture.png");
}

void
initialize_background(void)
{
	initialize_dna();
	initialize_sphere();
	initialize_textures();
}

void
tear_down_background(void)
{
	tear_down_dna();
	tear_down_sphere();
}

void
serialize_background(FILE *out)
{
	fwrite(&rot_z, sizeof rot_z, 1, out);
	fwrite(&sphere_rot_y, sizeof sphere_rot_y, 1, out);
}

int
unserialize_background(const char *data)
{
	memcpy(&rot_z, data, sizeof rot_z);
	data += sizeof rot_z;

	memcpy(&sphere_rot_y, data, sizeof sphere_rot_y);

	return sizeof rot_z + sizeof sphere_rot_y;
}
