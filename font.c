#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include <GL/gl.h>

#include "panic.h"
#include "image.h"
#include "font.h"
#include "font_render.h"
#include "dict.h"

struct font_render_info *font_small;
struct font_render_info *font_medium;
struct font_render_info *font_big;

enum {
	NUM_FONTS = 3
};

static struct {
	const char *file;
	const char *name;
	struct font_render_info **render_info_ptr;
} fonts[NUM_FONTS] = {
	{ "birdman-small.fontdef", "birdman-small", &font_small },
	{ "birdman-medium.fontdef", "birdman-medium", &font_medium },
	{ "birdman-big.fontdef", "birdman-big", &font_big },
};

void
initialize_font(void)
{
	struct dict *font_dict;
	struct font_info *fi;
	int i;

	font_dict = dict_make();

	for (i = 0; i < NUM_FONTS; i++) {
		fontdef_parse_file(font_dict, fonts[i].file);

		if ((fi = dict_get(font_dict, fonts[i].name)) == NULL)
			panic("font %s found in fontdef file %s?",
			  fonts[i].name, fonts[i].file);

		*fonts[i].render_info_ptr = font_render_info_make(fi);
	}

	dict_free(font_dict, NULL);
}

void
font_initialize_resources(void)
{
	int i;

	for (i = 0; i < NUM_FONTS; i++)
		font_render_info_initialize_resources(*fonts[i].render_info_ptr);
}

void
font_destroy_resources(void)
{
	int i;

	for (i = 0; i < NUM_FONTS; i++)
		font_render_info_destroy_resources(*fonts[i].render_info_ptr);
}
