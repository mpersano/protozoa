#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <assert.h>

#include <GL/gl.h>
#include <png.h>

#include "font.h"
#include "panic.h"
#include "dict.h"
#include "image.h"
#include "gl_util.h"
#include "font_render.h"

void
font_render_info_initialize_resources(struct font_render_info *fri)
{
	const struct image *img;
	const struct glyph_info *g;
	const struct font_info *info;
	float ds, dt;

	assert(fri->display_list_base == 0);

	info = fri->font_info;
	img = fri->image;

	ds = 1.f/img->width;
	dt = 1.f/img->height;

	fri->display_list_base = glGenLists(MAX_GLYPH_CODE + 1);

	for (g = info->glyphs; g != &info->glyphs[info->num_glyphs]; g++) {
		float s0, s1, t0, t1;

		assert(g->code >= 0 && g->code <= MAX_GLYPH_CODE);

		fri->glyph_table[g->code] = g;

		glNewList(fri->display_list_base + g->code, GL_COMPILE);

		glBindTexture(GL_TEXTURE_2D, gl_texture_id(fri->texture_id));

		glBegin(GL_QUADS);

		s0 = ds*g->texture_x;
		s1 = ds*(g->texture_x + (g->transposed ? g->height : g->width)); 
		t0 = dt*g->texture_y;
		t1 = dt*(g->texture_y + (g->transposed ? g->width : g->height));

		glTexCoord2f(s0, t0);
		glVertex2f(g->left, -g->top);

		if (g->transposed)
			glTexCoord2f(s0, t1);
		else
			glTexCoord2f(s1, t0);
		glVertex2f(g->left + g->width, -g->top);

		glTexCoord2f(s1, t1);
		glVertex2f(g->left + g->width, g->height - g->top);

		if (g->transposed)
			glTexCoord2f(s1, t0);
		else
			glTexCoord2f(s0, t1);
		glVertex2f(g->left, g->height - g->top);

		glEnd();

		glTranslatef(g->advance_x, 0, 0);

		glEndList();
	}
}

void
font_render_info_destroy_resources(struct font_render_info *fri)
{
	assert(fri->display_list_base != 0);

	glDeleteLists(fri->display_list_base, MAX_GLYPH_CODE + 1);
	fri->display_list_base = 0;
}

struct font_render_info *
font_render_info_make(const struct font_info *info)
{
	struct font_render_info *fri;
	
	glDisable(GL_TEXTURE_2D);

	fri = calloc(1, sizeof *fri);

	fri->font_info = info;
	fri->char_height = info->size;
	fri->image = image_make_from_png(info->texture_filename);
	fri->texture_id = image_to_texture(fri->image);

	font_render_info_initialize_resources(fri);

	return fri;
}

void
render_string(const struct font_render_info *fri, int x, int y, float scale,
  const char *fmt, ...)
{
	static char str[80];
	va_list ap;
	const char *p;

	va_start(ap, fmt);
	vsnprintf(str, sizeof str, fmt, ap);
	va_end(ap);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_LIGHTING);
	glShadeModel(GL_FLAT);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindTexture(GL_TEXTURE_2D, gl_texture_id(fri->texture_id));

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glTranslatef(x, y, 0.f);
	glScalef(scale, scale, 0.f);

	for (p = str; *p; p++)
		glCallList(fri->display_list_base + *p);

	glPopMatrix();
	glPopAttrib();
}

int
char_width_in_pixels(const struct font_render_info *fri, int ch)
{
	return fri->glyph_table[ch]->width;
}

int
string_width_in_pixels(const struct font_render_info *fri, const char *fmt, ...)
{
	static char str[80];
	va_list ap;
	int width;
	const struct glyph_info *g;
	const char *p;

	va_start(ap, fmt);
	vsnprintf(str, sizeof str, fmt, ap);
	va_end(ap);

	width = 0;
	g = NULL;

	for (p = str; *p != '\0'; ++p) {
		g = fri->glyph_table[(int)*p];
		width += g != NULL ? g->advance_x : fri->char_height;
	}

	return width;
}
