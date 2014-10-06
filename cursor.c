#include <stdio.h>
#include <assert.h>
#include <SDL.h>

#include "image.h"
#include "cursor.h"

SDL_Cursor *
cursor_create_from_png(const char *file_name)
{
	int w, h;
	Uint8 *data, *mask;
	struct image *img;
	unsigned *p;
	SDL_Cursor *r;
	int i, j;
	
	img = image_make_from_png(file_name);

	w = 8*((img->width + 7)/8);
	h = img->height;

	assert(w >= img->width);

	data = calloc(1, h*w/8);
	mask = calloc(1, h*w/8);
	
	p = img->bits;

	for (i = 0; i < img->height; i++) {
		for (j = 0; j < img->width; j++) {
			const unsigned char *q = (const unsigned char *)p;

			if (q[3] < 128) {
				/* transparent */
			} else if (q[3] < 210) {
				/* black */
				data[i*(w/8) + (j/8)] |= 1u << (7 - (j%8));
				mask[i*(w/8) + (j/8)] |= 1u << (7 - (j%8));
			} else {
				/* white */
				mask[i*(w/8) + (j/8)] |= 1u << (7 - (j%8));
			}

			++p;
		}
	}

	assert(p == &img->bits[img->width*img->height]);

	r = SDL_CreateCursor(data, mask, w, h, 0, 0);

	free(mask);
	free(data);

	return r;
}
