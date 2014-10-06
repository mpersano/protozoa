#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GL/glew.h>
#include <assert.h>

#include "gl_util.h"
#include "arena.h"
#include "image.h"
#include "gl_util.h"
#include "particle.h"

enum {
	MAX_PARTICLES = 4096
};

static unsigned particle_bitmap[(MAX_PARTICLES + 31)/32];
static particle particles[MAX_PARTICLES];

typedef struct rgb {
	float r, g, b;
} rgb;

typedef struct palette {
	int num_entries;
	rgb *entries;
} palette;

static palette palettes[NUM_PALETTES];

static int debris_texture_id;

struct gl_vertex {
	GLfloat vertex[2];
	GLubyte color[3];
	GLshort texuv[2];
} /* __attribute__((packed)) */;

static struct gl_vertex gl_vertices[MAX_PARTICLES*4];
static struct gl_vertex *cur_vertex;

static GLuint
gen_debris_texture(int width, int height)
{
	struct image *image;
	unsigned *p;
	int i, j;

	image = malloc(sizeof *image);
	image->width = width;
	image->height = height;
	image->bits = malloc(width*height*sizeof *image->bits);

	p = image->bits;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			int v = 255 - sqrt((i - height/2)*(i - height/2) +
			  (j - width/2)*(j - width/2))*256*2/width;

			if (v < 0)
				v = 0;

			*p++ = 0xff000000 | v | (v << 8) | (v << 16);
		}
	}

	return image_to_texture(image);
}

#if 0
static GLuint
gen_shockwave_texture(int width, int height)
{
	struct image image;
	unsigned *p;
	GLuint texture_id;
	int i, j;

	image.width = width;
	image.height = height;
	image.bits = malloc(width*height*sizeof *image.bits);

	p = image.bits;

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			float d;
			
			int v;
			float s;
			d = sqrt((i - height/2)*(i - height/2) +
			  (j - width/2)*(j - width/2))/(width/2);
			s = d - .6f;
			if (s < -.2f)
				s = -.2f;
			if (s > .2f)
				s = .2f;
			s += .2f;
			s /= .4f;
			assert(s >= 0.f && s <= 1.f);
			s *= s;
			s *= s;
			s *= M_PI;
			v = 255*sin(s);

			if (v < 0)
				v = 0;

			*p++ = 0xff000000 | v | (v << 8) | (v << 16);
		}
	}

	texture_id = image_to_texture(&image);

	return texture_id;
}
#endif

inline int
particle_index(const particle *p)
{
	return p - &particles[0];
}

static inline int
particle_is_used(const particle *p)
{
	const int idx = particle_index(p);
	return particle_bitmap[idx/32] & (1u << (idx%32));
}

static inline void
particle_use(const particle *p)
{
	const int idx = particle_index(p);
	particle_bitmap[idx/32] |= (1u << (idx%32));
}

static inline void
particle_release(const particle *p)
{
	const int idx = particle_index(p);
	particle_bitmap[idx/32] &= ~(1u << (idx%32));
}

#define PARTICLE_SIZE .8f

static inline void
draw_debris_particle(particle *p)
{	
	GLubyte r, g, b;
	const palette *pal = &palettes[p->palette];
	float x0, y0, x1, y1, x2, y2, x3, y3;
	vector2 dir;
	int i;

	i = pal->num_entries*p->t/p->ttl;

	if (i >= pal->num_entries)
		i = pal->num_entries - 1;

	if (i < 0)
		i = 0;

	r = 255*pal->entries[i].r;
	g = 255*pal->entries[i].g;
	b = 255*pal->entries[i].b;

	/* glColor3f(r, g, b); */

	dir = p->dir;

	if (dir.x == 0.f && dir.y == 0.f)
		vec2_set(&dir, 1, 0);

	vec2_normalize(&dir);

	x0 = p->pos.x + dir.x*.5f*p->height - dir.y*.5f*p->width;
	y0 = p->pos.y + dir.y*.5f*p->height + dir.x*.5f*p->width;

	x1 = x0 - dir.x*p->height;
	y1 = y0 - dir.y*p->height;

	x2 = x1 + dir.y*p->width;
	y2 = y1 - dir.x*p->width;

	x3 = x2 + dir.x*p->height;
	y3 = y2 + dir.y*p->height;

	cur_vertex->color[0] = r;
	cur_vertex->color[1] = g;
	cur_vertex->color[2] = b;
	cur_vertex->texuv[0] = 0;
	cur_vertex->texuv[1] = 0;
	cur_vertex->vertex[0] = x0;
	cur_vertex->vertex[1] = y0;
	++cur_vertex;

	cur_vertex->color[0] = r;
	cur_vertex->color[1] = g;
	cur_vertex->color[2] = b;
	cur_vertex->texuv[0] = 0;
	cur_vertex->texuv[1] = 1;
	cur_vertex->vertex[0] = x1;
	cur_vertex->vertex[1] = y1;
	++cur_vertex;

	cur_vertex->color[0] = r;
	cur_vertex->color[1] = g;
	cur_vertex->color[2] = b;
	cur_vertex->texuv[0] = 1;
	cur_vertex->texuv[1] = 1;
	cur_vertex->vertex[0] = x2;
	cur_vertex->vertex[1] = y2;
	++cur_vertex;

	cur_vertex->color[0] = r;
	cur_vertex->color[1] = g;
	cur_vertex->color[2] = b;
	cur_vertex->texuv[0] = 1;
	cur_vertex->texuv[1] = 0;
	cur_vertex->vertex[0] = x3;
	cur_vertex->vertex[1] = y3;
	++cur_vertex;
}

static void
update_debris_particle(particle *p)
{
	vector2 normal;
	vector2 pos = p->pos;
	vector2 dir = p->dir;
	vec2_mul_scalar(&dir, p->speed);

	assert(particle_is_used(p));

	vec2_add_to(&pos, &dir);

	if (cur_arena && bounces_with_arena(&pos, .2f, &normal)) {
		float k = 2.f*(normal.x*p->dir.x + normal.y*p->dir.y);

		p->dir.x = -(k*normal.x - p->dir.x);
		p->dir.y = -(k*normal.y - p->dir.y);

		pos = p->pos;
		vec2_add_to(&pos, &p->dir);
	}

	p->pos = pos;

	p->speed *= p->friction;
	p->width *= p->friction;
	p->height *= p->friction;

	vec2_add_to(&p->dir, &p->acc);
}

static void
update_particle(particle *p)
{
	if (++p->t >= p->ttl) {
		particle_release(p);
	} else {
		update_debris_particle(p);
	}
}

#if 0
static void
for_each_particle(void (*fn)(particle *))
{
	int i;

	for (i = 0; i < sizeof particle_bitmap/sizeof *particle_bitmap; i++) {
		unsigned m = particle_bitmap[i];

		if (m) {
			particle *p = &particles[i*32];

			while (m) {
				if (m & 1)
					fn(p);
				m >>= 1;
				++p;
			}
		}
	}
}
#else
#define for_each_particle(particle_fn) \
{ \
	int i; \
	for (i = 0; i < sizeof particle_bitmap/sizeof *particle_bitmap; i++) { \
		unsigned m = particle_bitmap[i]; \
		if (m) { \
			particle *p = &particles[i*32]; \
			while (m) { \
				if (m & 1) \
					particle_fn(p); \
				m >>= 1; \
				++p; \
			} \
		} \
	} \
}
#endif

void
reset_particles(void)
{
	memset(particle_bitmap, 0, sizeof particle_bitmap);
}

void
draw_particles(void)
{
	int num_particles;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_LIGHTING);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(debris_texture_id));

	cur_vertex = gl_vertices;

	for_each_particle(draw_debris_particle);

	num_particles = cur_vertex - gl_vertices;

	if (num_particles > 0) {
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, sizeof(struct gl_vertex),
		  &gl_vertices[0].vertex);

		glEnableClientState(GL_COLOR_ARRAY);
		glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(struct gl_vertex),
		  &gl_vertices[0].color);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_SHORT, sizeof(struct gl_vertex),
		  &gl_vertices[0].texuv);

		glDrawArrays(GL_QUADS, 0, num_particles);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	glPopAttrib();
}

void
update_particles(void)
{
	for_each_particle(update_particle);
}

particle *
add_particle(void)
{
	int i;

	for (i = 0; i < sizeof particle_bitmap/sizeof *particle_bitmap; i++) {
		unsigned m = particle_bitmap[i];

		if (m != ~0u) {
			particle *p = &particles[i*32];

			while (m & 1) {
				m >>= 1;
				p++;
			}

			assert(!particle_is_used(p));
			particle_use(p);

			memset(p, 0, sizeof *p);

			return p;
		}
	}

	return NULL;
}

static void
init_palette(palette *pal,
	     int num_entries, float red, float green, float blue,
	     float kd, float ks, float pn)
{
	int i;

	pal->num_entries = num_entries;
	pal->entries = malloc(pal->num_entries*sizeof *pal->entries);

	for (i = 0; i < pal->num_entries; i++) {
		const float a = .5f*M_PI*i/pal->num_entries;
		const float ca = cos(a);

		const float ka = kd*ca;
		const float kb = ks*pow(ca, pn);

		const float r = red*ka + kb;
		const float g = green*ka + kb;
		const float b = blue*ka + kb;

		pal->entries[i].r = r > 1. ? 1. : r;
		pal->entries[i].g = g > 1. ? 1. : g;
		pal->entries[i].b = b > 1. ? 1. : b;
	}
}

static void
init_palettes(void)
{
	int i;
	const struct {
	     float red, green, blue;
	     float kd, ks, pn;
	} pal_info[NUM_PALETTES] = {
		{ .2, 1., 1., .9, .5, 4. },
		{ 1., .1, .1, .9, .5, 4. },
		{ .2, 1., .2, .9, .5, 4. },
		{ 1., 1., .2, .9, .5, 4. },
		{ 1., .6, .2, .9, .5, 4. },
		{ 1., 1., .6, .9, .5, 4. },
		{ 1., 0., .5, .9, .5, 4. },
	};

	for (i = 0; i < NUM_PALETTES; i++) {
		init_palette(&palettes[i], 256, 
		  pal_info[i].red, pal_info[i].green, pal_info[i].blue,
		  pal_info[i].kd, pal_info[i].ks, pal_info[i].pn);
	}
}

static void
init_texture(void)
{
	debris_texture_id = gen_debris_texture(8, 8);
}

void
initialize_particles(void)
{
	init_texture();
	init_palettes();
}

void
tear_down_particles(void)
{
}

static FILE *serialize_out;

static void
serialize_particle(particle *p)
{
#if 1
	fputc(p->palette, serialize_out);
	fputc(p->t, serialize_out);
	fputc(p->ttl, serialize_out);
	fwrite(&p->dir, sizeof p->dir, 1, serialize_out);
	fwrite(&p->pos, sizeof p->pos, 1, serialize_out);
	fwrite(&p->width, sizeof p->width, 1, serialize_out);
	fwrite(&p->height, sizeof p->height, 1, serialize_out);
#else
	fwrite(p, sizeof *p, 1, serialize_out);
#endif
}

void
serialize_particles(FILE *out)
{
	int i, count = 0;

	for (i = 0; i < sizeof(particle_bitmap)/sizeof(*particle_bitmap); i++) {
		unsigned m;

		for (m = particle_bitmap[i]; m; m >>= 1) {
			if (m & 1)
				++count;
		}
	}

	fwrite(&count, sizeof count, 1, out);

	serialize_out = out;
	for_each_particle(serialize_particle);
}

int
unserialize_particles(const char *data)
{
	int count;
	const unsigned char *q;

	q = (unsigned char *)data;

	memcpy(&count, q, sizeof count);
	q += sizeof count;

	memset(particle_bitmap, 0, sizeof particle_bitmap);

	if (count) {
		struct particle *p;

		for (p = particles; p != &particles[count]; p++) {
			particle_use(p);

#if 1
			p->palette = *q++;
			p->t = *q++;
			p->ttl = *q++;
			memcpy(&p->dir, q, sizeof p->dir); q += sizeof p->dir;
			memcpy(&p->pos, q, sizeof p->pos); q += sizeof p->pos;
			memcpy(&p->width, q, sizeof p->width); q += sizeof p->width;
			memcpy(&p->height, q, sizeof p->height); q += sizeof p->height;
#else
			memcpy(p, q, sizeof *p);
			q += sizeof *p;
#endif
		}
	}

	return q - (unsigned char *)data;
}
