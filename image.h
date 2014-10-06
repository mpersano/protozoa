#ifndef IMAGE_H_
#define IMAGE_H_

struct image {
	int width;
	int height;
	unsigned *bits;
};

struct image *
image_make_from_png(const char *png_filename);

void
image_free(struct image *img);

#endif /* IMAGE_H_ */
