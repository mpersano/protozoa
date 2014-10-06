#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <GL/gl.h>

#include "common.h"
#include "gl_util.h"
#include "explosion.h"

enum {
	MAX_EXPLOSIONS = 128,
	NUM_EXPLOSION_FRAMES = 16
};

struct explosion {
	int used;
	float radius;
	float angle;
	vector2 pos;
	int ttl;
	int t;
	int type;
};

struct explosion_data {
	int num_frames;
	float radius_factor;
	int texture_id;
	int blend_type;
	const char *texture_file_name;
} explosion_data[] = {
	{ 16, 1.08, 0, 0, "explosion.png" },	/* EFF_EXPLOSION */
	{ 16, .9, 0, 0, "flare.png" },		/* EFF_FLARE */
	{ 16, 1.1137, 0, 1, "ring.png" },		/* EFF_RING */
	{ 16, 1.08, 0, 0, "blue-explosion.png" },	/* EFF_BLUE_EXPLOSION */
	{ 16, 1.1137, 0, 1, "blue-ring.png" },		/* EFF_BLUE_RING */
};

static struct explosion explosions[MAX_EXPLOSIONS];

void
reset_explosions(void)
{
	struct explosion *p;

	for (p = explosions; p != &explosions[MAX_EXPLOSIONS]; p++)
		p->used = 0;
}

static void
draw_explosion(const struct explosion *p)
{
	float u, du;
	int cur_frame;

	glPushMatrix();
	glTranslatef(p->pos.x, p->pos.y, 0.f);
	glRotatef(p->angle, 0, 0, 1.f);

	cur_frame = p->t*explosion_data[p->type].num_frames/p->ttl;

	du = 1.f/explosion_data[p->type].num_frames;
	u = cur_frame*du;

	glColor3f(1, 1, 1);

	glBegin(GL_QUADS);

	glTexCoord2f(u, 1);
	glVertex3f(-p->radius, -p->radius, 0.f);

	glTexCoord2f(u + du, 1);
	glVertex3f(p->radius, -p->radius, 0.f);

	glTexCoord2f(u + du, 0);
	glVertex3f(p->radius, p->radius, 0.f);

	glTexCoord2f(u, 0);
	glVertex3f(-p->radius, p->radius, 0.f);

	glEnd();

	glPopMatrix();
}

void
draw_explosions(void)
{
	int i;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);

	glEnable(GL_TEXTURE_2D);

	glEnable(GL_BLEND);

	for (i = 0; i < NUM_EXPLOSION_TYPES; i++) {
		const struct explosion *p;

		glBindTexture(GL_TEXTURE_2D, gl_texture_id(explosion_data[i].texture_id));

		switch (explosion_data[i].blend_type) {
			case 0:
				glBlendFunc(GL_SRC_ALPHA,
				  GL_ONE_MINUS_SRC_ALPHA);
				break;

			case 1:
				glBlendFunc(GL_ONE, GL_ONE);
				break;

			default:
				assert(0);
		}

		for (p = explosions; p != &explosions[MAX_EXPLOSIONS]; p++) {
			if (p->used && p->type == i)
				draw_explosion(p);
		}
	}

	glPopAttrib();
}

static void
update_explosion(struct explosion *p)
{
	p->radius *= explosion_data[p->type].radius_factor;
	p->angle += 7.5f;
}

void
update_explosions(void)
{
	struct explosion *p;

	for (p = explosions; p != &explosions[MAX_EXPLOSIONS]; p++) {
		if (p->used) {
			if (++p->t >= p->ttl)
				p->used = 0;
			else
				update_explosion(p);
		}
	}
}

void
add_explosion(const vector2 *pos, float radius, int type, int ttl)
{
	struct explosion *p = NULL;
	int i;

	for (i = 0; i < MAX_EXPLOSIONS; i++) {
		if (!explosions[i].used) {
			p = &explosions[i];
			break;
		}
	}

	if (p) {
		p->used = 1;
		p->pos = *pos;
		p->radius = radius;
		p->angle = frand()*360.f;
		p->ttl = ttl;
		p->t = 0;
		p->type = type;
	}
}

void
initialize_explosions(void)
{
	int i;

	for (i = 0; i < NUM_EXPLOSION_TYPES; i++) {
		explosion_data[i].texture_id =
		  png_to_texture(explosion_data[i].texture_file_name);
	}
}

void
serialize_explosions(FILE *out)
{
	struct explosion *p;
	int count = 0;

	for (p = explosions; p != &explosions[MAX_EXPLOSIONS]; p++) {
		if (p->used)
			++count;
	}

	fwrite(&count, sizeof count, 1, out);

	if (count) {
		for (p = explosions; p != &explosions[MAX_EXPLOSIONS]; p++) {
			if (p->used) {
				fwrite(p, sizeof *p, 1, out);
			}
		}
	}
}

int
unserialize_explosions(const char *data)
{
	int count;
	struct explosion *p;

	memcpy(&count, data, sizeof count);
	data += sizeof count;

	assert(count < MAX_EXPLOSIONS);

	if (count)
		memcpy(explosions, data, count*sizeof *explosions);

	for (p = &explosions[count]; p != &explosions[MAX_EXPLOSIONS]; p++) {
		p->used = 0;
	}

	return sizeof count + count*sizeof *explosions;
}
