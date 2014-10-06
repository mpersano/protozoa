#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <GL/gl.h>

#include "ship.h"
#include "arena.h"
#include "gl_util.h"
#include "bomb.h"

enum {
	MAX_BOMBS = 16
};

struct bomb {
	int used;
	vector2 pos;
	vector2 dir;
};

static struct bomb bombs[MAX_BOMBS];
const float bomb_speed;
static int bomb_texture_id;

static const float BOMB_SIZE = .25f;
static const float BOMB_SPEED = .15f;

void
reset_bombs(void)
{
	struct bomb *p;

	for (p = bombs; p != &bombs[MAX_BOMBS]; p++)
		p->used = 0;
}

static void
draw_bomb(const struct bomb *b)
{
	glTexCoord2f(0, 1);
	glVertex2f(b->pos.x - BOMB_SIZE, b->pos.y - BOMB_SIZE);

	glTexCoord2f(1, 1);
	glVertex2f(b->pos.x + BOMB_SIZE, b->pos.y - BOMB_SIZE);

	glTexCoord2f(1, 0);
	glVertex2f(b->pos.x + BOMB_SIZE, b->pos.y + BOMB_SIZE);

	glTexCoord2f(0, 0);
	glVertex2f(b->pos.x - BOMB_SIZE, b->pos.y + BOMB_SIZE);
}

void
draw_bombs(void)
{
	struct bomb *p;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_FLAT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(bomb_texture_id));
	glColor4f(1, 1, 1, 1);

	glBegin(GL_QUADS);

	for (p = bombs; p != &bombs[MAX_BOMBS]; p++) {
		if (p->used)
			draw_bomb(p);
	}

	glEnd(); 
	glPopAttrib();
}

static void
update_bomb(struct bomb *b)
{
	vector2 pos, normal;

	vec2_add(&pos, &b->pos, &b->dir);
	
	if (bounces_with_arena(&pos, .15f, &normal)) {
		b->used = 0;
	} else {
		b->pos = pos;
		hit_ship(&b->pos, .15f);
	}
}

void
update_bombs(void)
{
	struct bomb *p;

	for (p = bombs; p != &bombs[MAX_BOMBS]; p++) {
		if (p->used)
			update_bomb(p);
	}
}

void
add_bomb(const vector2 *origin)
{
	struct bomb *p;

	for (p = bombs; p != &bombs[MAX_BOMBS]; p++) {
		if (!p->used)
			break;
	}

	if (p != &bombs[MAX_BOMBS] && !p->used) {
		p->used = 1;
		p->pos = *origin;
		vec2_sub(&p->dir, &ship.pos, &p->pos);
		vec2_mul_scalar(&p->dir, BOMB_SPEED/vec2_length(&p->dir));
	}
}

void
initialize_bombs(void)
{
	bomb_texture_id = png_to_texture("bomb.png");
}
