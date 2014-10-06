#ifndef GL_UTIL_H_
#define GL_UTIL_H_

#include "matrix.h"
#include <GL/gl.h>

struct image;
struct material;

void
mat_to_opengl(float *r, const matrix *m);

void
set_opengl_matrix(const matrix *m);

int
image_to_texture(struct image *image);

int
png_to_texture(const char *file_name);

GLuint
gl_texture_id(int texture_id);

void
delete_textures(void);

void
reload_textures(void);

void
destroy_texture_pool(void);

void
set_opengl_material(const struct material *mat, float alpha);

#endif /* GL_UTIL_H_ */
