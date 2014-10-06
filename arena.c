/*
 * "Borders? I have never seen one. But I have heard they exist in the minds
 *  of most people."
 *	- Thor Heyerdahl.
 */
#include <math.h>
#include <GL/gl.h>

#include "gl_util.h"
#include "arena.h"

#define ARENA_RADIUS 7.f

arena *cur_arena;

struct {
	int border_texture_id;
	float factor;
} arena_borders[NUM_ARENA_BORDERS] = {
	{ 0, .04f },
	{ 0, .04f },
	{ 0, .12f },
	{ 0, .24f },
};

int
bounces_with_arena(const vector2 *position, float radius, vector2 *arena_normal)
{
	const vector2 *verts = cur_arena->verts;
	const int nverts = cur_arena->nverts;
	int i;

	for (i = 0; i < nverts; i++) {
		vector2 n, d, o;

		n.x = verts[(i + 1)%nverts].y - verts[i].y; 
		n.y = -(verts[(i + 1)%nverts].x - verts[i].x);
		vec2_normalize(&n);

		o.x = verts[i].x - n.x*radius;
		o.y = verts[i].y - n.y*radius;

		d.x = position->x - o.x;
		d.y = position->y - o.y;

		if (d.x*n.x + d.y*n.y > 0.f) {
			*arena_normal = n;
			return 1;
		}
	}

	return 0;
}

void
reset_arena(void)
{
}

void
draw_arena_outline(const struct arena *arena, float alpha,
  enum arena_border border_type)
{
	int i;
	float A, B;

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glShadeModel(GL_FLAT);
	glDisable(GL_LIGHTING);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(arena_borders[border_type].border_texture_id));
	glDisable(GL_CULL_FACE);

	A = 1.f + arena_borders[border_type].factor;
	B = 1.f - arena_borders[border_type].factor;

	glColor4f(1, 1, 1, alpha);

	glBegin(GL_QUADS);

	for (i = 0; i < arena->nverts; i++) {
		const vector2 *u = &arena->verts[i];
		const vector2 *v = &arena->verts[(i + 1)%arena->nverts];

		glTexCoord2f(0, 0);
		glVertex2f(A*u->x, A*u->y);

		glTexCoord2f(0, 1);
		glVertex2f(A*v->x, A*v->y);

		glTexCoord2f(1, 1);
		glVertex2f(B*v->x, B*v->y);

		glTexCoord2f(1, 0);
		glVertex2f(B*u->x, B*u->y);
	}

	glEnd();

	glPopAttrib();
}

void
draw_filled_arena(const struct arena *arena)
{
	int i;

	glBegin(GL_TRIANGLE_FAN);

	for (i = 0; i < arena->nverts; i++)
		glVertex2f(arena->verts[i].x, arena->verts[i].y);

	glEnd();
}

void
initialize_arena(void)
{
	static const char *border_texture[NUM_ARENA_BORDERS] = {
		"arena-border-blue.png",
		"arena-border-red.png",
		"arena-border-1.png",
		"arena-border-0.png",
	};
	int i;

	for (i = 0; i < NUM_ARENA_BORDERS; i++) {
		arena_borders[i].border_texture_id =
		  png_to_texture(border_texture[i]);
	}
}
