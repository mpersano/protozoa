#ifndef FONT_H_
#define FONT_H_

struct glyph_info {
	int code;
	int width;
	int height;
	int left;
	int top;
	int advance_x;
	int advance_y;
	int texture_x;
	int texture_y;
	int transposed;
};

struct font_info {
	const char *texture_filename;
	int size;
	int num_glyphs;
	struct glyph_info *glyphs;
};

struct dict;

void
fontdef_parse_file(struct dict *font_dict, const char *filename);

void
initialize_font(void);

void
font_initialize_resources(void);

void
font_destroy_resources(void);

extern struct font_render_info *font_small;
extern struct font_render_info *font_medium;
extern struct font_render_info *font_big;

#endif /* FONT_H_ */
