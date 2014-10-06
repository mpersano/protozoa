#ifndef FONT_RENDER_H_
#define FONT_RENDER_H_

#include <GL/gl.h>
#include "font.h"

enum {
	MAX_GLYPH_CODE = 127
};

struct font_render_info {
  	const struct glyph_info *glyph_table[MAX_GLYPH_CODE + 1];
	const struct font_info *font_info;
	struct image *image;
  	int texture_width;
	int texture_height;
	int char_height;
	GLuint texture_id;
	GLuint display_list_base;
};

struct font_render_info *
font_render_info_make(const struct font_info *info);

int
string_width_in_pixels(const struct font_render_info *fri, const char *fmt, ...);

int
char_width_in_pixels(const struct font_render_info *fri, int ch);

void
render_string(const struct font_render_info *fri, int x, int y, float scale,
  const char *fmt, ...);

#endif /* FONT_RENDER_H_ */
