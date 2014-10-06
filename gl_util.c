#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "image.h"
#include "panic.h"
#include "mesh.h" /* struct material */
#include "gl_util.h"

struct texture_info {
	struct image *source;
	GLuint gl_texture_id;
};

static int texture_pool_size = 0;
static int num_textures;

static struct texture_info *texture_pool;

void
mat_to_opengl(float *r, const matrix *m)
{
	r[0] = m->m11;  r[4] = m->m12;  r[8] = m->m13; r[12] = m->m14;
	r[1] = m->m21;  r[5] = m->m22;  r[9] = m->m23; r[13] = m->m24;
	r[2] = m->m31;  r[6] = m->m32; r[10] = m->m33; r[14] = m->m34;
	r[3] = 0.f;     r[7] = 0.f;    r[11] = 0.f;    r[15] = 1.f;
}

void
set_opengl_matrix(const matrix *m)
{
	static float fm[16];
	mat_to_opengl(fm, m);
        glMultMatrixf(fm);
}

static GLuint
image_to_opengl_texture(const struct image *image)
{
	GLuint texture_id;

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glEnable(GL_TEXTURE_2D);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glGenTextures(1, &texture_id);

	if (glGetError() != GL_NO_ERROR)
		panic("glGenTextures failed");

	glBindTexture(GL_TEXTURE_2D, texture_id);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->width, image->height,
	  0, GL_RGBA, GL_UNSIGNED_BYTE, image->bits);

	glPopAttrib();

	return texture_id;
}

int
image_to_texture(struct image *image)
{
	struct texture_info *p;

	if (texture_pool_size == 0) {
		texture_pool = malloc((texture_pool_size = 10)*sizeof *texture_pool);
	} else if (num_textures == texture_pool_size) {
		texture_pool = realloc(texture_pool, (texture_pool_size *= 2)*sizeof *texture_pool);
	}

	p = &texture_pool[num_textures++];

	p->source = image;
	p->gl_texture_id = image_to_opengl_texture(p->source);

	return num_textures;
}

int
png_to_texture(const char *source)
{
	return image_to_texture(image_make_from_png(source));
}

GLuint
gl_texture_id(int texture_id)
{
	return texture_pool[texture_id - 1].gl_texture_id;
}

void
delete_textures(void)
{
	struct texture_info *p;

	for (p = texture_pool; p != &texture_pool[num_textures]; p++) {
		glDeleteTextures(1, &p->gl_texture_id);
		p->gl_texture_id = 0;
	}
}

void
reload_textures(void)
{
	struct texture_info *p;

	for (p = texture_pool; p != &texture_pool[num_textures]; p++) {
		assert(p->gl_texture_id == 0);
		assert(p->source != NULL);
		p->gl_texture_id = image_to_opengl_texture(p->source);
	}
}

void
destroy_texture_pool(void)
{
	struct texture_info *p;

	delete_textures();

	for (p = texture_pool; p != &texture_pool[num_textures]; p++)
		image_free(p->source);

	free(texture_pool);

	texture_pool = NULL;
	num_textures = 0;
	texture_pool_size = 0;
}

void
set_opengl_material(const struct material *mat, float alpha)
{
	GLfloat diffuse[4];
	GLfloat specular[4];

	memcpy(diffuse, &mat->diffuse, sizeof mat->diffuse);
	memcpy(specular, &mat->specular, sizeof mat->specular);

	diffuse[3] = specular[3] = alpha;

	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat->shine);
}
