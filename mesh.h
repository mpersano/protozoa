#ifndef MESH_H_
#define MESH_H_

#include <GL/gl.h>
#include "vector.h"

#define MAX_POLY_VTX 20

struct rgb {
	float r, g, b;
};

struct material {
	struct rgb ambient;
	struct rgb diffuse;
	struct rgb specular;
	float shine;
	char *texture_source;
};

struct mesh;

struct mesh *
mesh_load_obj(const char *file_name);

void
mesh_initialize_normals(struct mesh *mesh);

void
mesh_setup(struct mesh *mesh);

void
mesh_release(struct mesh *mesh);

void
mesh_render(const struct mesh *mesh);

void
mesh_render_modulate_color(const struct mesh *mesh, const struct rgb *color,
  float factor);

void
initialize_mesh(void);

void
tear_down_mesh(void);

struct matrix;

void
mesh_transform(struct mesh *mesh, const struct matrix *t);

#endif /* MESH_H_ */
