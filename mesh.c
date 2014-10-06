#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <GL/glew.h>
#ifdef USE_CG
#include <Cg/cg.h>
#include <Cg/cgGL.h>
#endif

#include "gl_util.h"
#include "vector.h"
#include "panic.h"
#include "list.h"
#include "hash_table.h"
#include "dict.h"
#include "mesh.h"

#ifdef USE_CG
extern CGcontext cg_context;
extern CGprofile cg_vert_profile;
extern CGprofile cg_frag_profile;

#if 0
static CGprogram vp->program;

static CGparameter cg_model_view_proj_param;
static CGparameter cg_model_view_it_param;

static CGparameter cg_ambient_color_param;
static CGparameter cg_diffuse_color_param;
static CGparameter cg_specular_color_param;
static CGparameter cg_shininess_param;

static CGparameter cg_mix_color_param;
static CGparameter cg_mix_factor_param;
#endif

struct vert_shader {
	CGprogram program;

	CGparameter ambient_color_param;
	CGparameter diffuse_color_param;
	CGparameter specular_color_param;
	CGparameter shininess_param;

	CGparameter mix_color_param;
	CGparameter mix_factor_param;

	CGparameter model_view_proj_param;
	CGparameter model_view_it_param;
};

struct frag_shader {
	CGprogram program;

	CGparameter mix_color_param;
	CGparameter mix_factor_param;
};

static struct vert_shader no_tex_vert_shader;
static struct vert_shader tex_vert_shader;

static struct frag_shader no_tex_frag_shader;
static struct frag_shader tex_frag_shader;

static void
load_vert_shader(struct vert_shader *vp, const char *source, const char *entry_point)
{
	vp->program = cgCreateProgramFromFile(cg_context,
	  CG_SOURCE, source, cg_vert_profile, entry_point, NULL);

	cgGLLoadProgram(vp->program);

	vp->model_view_proj_param =
	  cgGetNamedParameter(vp->program, "model_view_proj");

	vp->model_view_it_param =
	  cgGetNamedParameter(vp->program, "model_view_it");

	vp->ambient_color_param =
	  cgGetNamedParameter(vp->program, "ambient_color");

	vp->diffuse_color_param =
	  cgGetNamedParameter(vp->program, "diffuse_color");

	vp->specular_color_param =
	  cgGetNamedParameter(vp->program, "specular_color");

	vp->shininess_param =
	  cgGetNamedParameter(vp->program, "shininess");

	vp->mix_color_param =
	  cgGetNamedParameter(vp->program, "mix_color");

	vp->mix_factor_param =
	  cgGetNamedParameter(vp->program, "mix_factor");
}

static void
load_frag_shader(struct frag_shader *fp, const char *source, const char *entry_point)
{
	fp->program = cgCreateProgramFromFile(cg_context,
	  CG_SOURCE, source, cg_frag_profile, entry_point, NULL);

	cgGLLoadProgram(fp->program);

	fp->mix_color_param =
	  cgGetNamedParameter(fp->program, "mix_color");

	fp->mix_factor_param =
	  cgGetNamedParameter(fp->program, "mix_factor");
}

static void
tear_down_vert_shader(struct vert_shader *vp)
{
	cgDestroyProgram(vp->program);
}

static void
tear_down_frag_shader(struct frag_shader *vp)
{
	cgDestroyProgram(vp->program);
}

static void
enable_vert_shader(const struct vert_shader *vp, const struct material *material, const struct rgb *mix_color, float mix_factor)
{
	float mix_color_rgba[] = { mix_color->r, mix_color->g,
	  mix_color->b, 1 };

	cgGLBindProgram(vp->program);

	cgGLSetStateMatrixParameter(vp->model_view_proj_param,
	  CG_GL_MODELVIEW_PROJECTION_MATRIX,
	  CG_GL_MATRIX_IDENTITY);

	cgGLSetStateMatrixParameter(vp->model_view_it_param,
	  CG_GL_MODELVIEW_MATRIX,
	  CG_GL_MATRIX_INVERSE_TRANSPOSE);

	cgGLSetParameter3fv(vp->mix_color_param, mix_color_rgba);
	cgGLSetParameter1f(vp->mix_factor_param, mix_factor);

	if (material) {
		cgGLSetParameter3fv(vp->ambient_color_param,
		  (float *)&material->ambient);

		cgGLSetParameter3fv(vp->diffuse_color_param,
		  (float *)&material->diffuse);

		cgGLSetParameter3fv(vp->specular_color_param,
		  (float *)&material->specular);

		cgGLSetParameter3fv(vp->shininess_param,
		  (float *)&material->shine);
	}

	cgGLEnableProfile(cg_vert_profile);
}

static void
enable_frag_shader(const struct frag_shader *fp, const struct rgb *mix_color, float mix_factor)
{
	float mix_color_rgba[] = { mix_color->r, mix_color->g, mix_color->b, 1 };
	cgGLBindProgram(fp->program);
	cgGLSetParameter3fv(fp->mix_color_param, mix_color_rgba);
	cgGLSetParameter1f(fp->mix_factor_param, mix_factor);
	cgGLEnableProfile(cg_frag_profile);
}
#endif

struct vertex {
	struct vector3 pos;
	struct vector3 normal;
	struct vector2 texuv;
};

struct gl_vertex {
	GLfloat pos[3];
	GLfloat normal[3];
	GLfloat texuv[2];
};

struct obj_polygon {
	int num_vertices;
	struct vtx_index {
		int coord;
		int normal;
		int texuv;
	} vtx_index[MAX_POLY_VTX];	/* vertex index */
	vector3 normal;
};

struct polygon {
	int num_vertices;
	int vtx_index[MAX_POLY_VTX];
	vector3 normal;
};

/* a group of polygons that share the same material */
struct mesh_chunk {
	int num_vertices;
	int num_polygons;
	struct material *material;
	struct vertex *vertices;
	struct polygon *polygons;

	/* rendering */
	struct gl_vertex *gl_vertex_array;
	int gl_index_array_size;
	GLushort *gl_index_array;
	int texture_id;
};

struct mesh {
	int num_chunks;
	struct mesh_chunk *chunks;
};

static void
mesh_chunk_initialize_poly_normals(struct mesh_chunk *chunk)
{
	struct polygon *p, *end;
	struct vertex *vtx;
	vector3 u, v;

	vtx = chunk->vertices;
	end = &chunk->polygons[chunk->num_polygons];

	for (p = chunk->polygons; p != end; p++) {
		const int *index = p->vtx_index;

		assert(p->num_vertices >= 3);

		assert(index[0] >= 0 && index[0] < chunk->num_vertices);
		assert(index[1] >= 0 && index[1] < chunk->num_vertices);
		assert(index[2] >= 0 && index[2] < chunk->num_vertices);

		vec3_sub(&u, &vtx[index[1]].pos, &vtx[index[0]].pos);
		vec3_sub(&v, &vtx[index[2]].pos, &vtx[index[0]].pos);

		vec3_cross_product(&p->normal, &u, &v);
		vec3_normalize(&p->normal);
	}
}

static void
mesh_chunk_initialize_vertex_normals(struct mesh_chunk *chunk)
{
	struct polygon *q, *poly_end;
	struct vertex *v;

	poly_end = &chunk->polygons[chunk->num_polygons];

	for (q = chunk->polygons; q != poly_end; q++) {
		const vector3 *normal = &q->normal;
		int i;

		for (i = 0; i < q->num_vertices; i++) {
			int idx = q->vtx_index[i];
			assert(idx >= 0 && idx < chunk->num_vertices);
			vec3_add_to(&chunk->vertices[idx].normal,
			  normal);
		}
	}

	for (v = chunk->vertices; v != &chunk->vertices[chunk->num_vertices]; v++)
		vec3_normalize(&v->normal);
}

static void
mesh_initialize_poly_normals(struct mesh *mesh)
{
	struct mesh_chunk *p, *end;

	end = &mesh->chunks[mesh->num_chunks];

	for (p = mesh->chunks; p != end; p++)
		mesh_chunk_initialize_poly_normals(p);
}

static void
mesh_initialize_vertex_normals(struct mesh *mesh)
{
	struct mesh_chunk *p, *end;

	end = &mesh->chunks[mesh->num_chunks];

	for (p = mesh->chunks; p != end; p++)
		mesh_chunk_initialize_vertex_normals(p);
}

void
mesh_initialize_normals(struct mesh *mesh)
{
	mesh_initialize_poly_normals(mesh);
	mesh_initialize_vertex_normals(mesh);
}

static void
chomp(char *str)
{
	char *p = &str[strlen(str) - 1];

	while (*p == '\n' || *p == '\r')
		*p-- = 0;
}

static void
load_mtl(const char *file_name, struct dict *mat_dict)
{
	FILE *in;
	static char line[256];
	struct material *cur_material;
	
	if ((in = fopen(file_name, "r")) == NULL)
		panic("load_mtl: failed to open `%s': %s\n", file_name,
		  strerror(errno));

	cur_material = NULL;

	while (fgets(line, sizeof line, in) != NULL) {
		chomp(line);

		if (!strncmp(line, "newmtl ", 7)) {
			cur_material = calloc(1, sizeof *cur_material);
			dict_put(mat_dict, line + 7, cur_material);
		} else if (!strncmp(line, "Ns ", 3)) {
			/* specular highlight */
			sscanf(line + 3, "%f", &cur_material->shine);
		} else if (!strncmp(line, "Kd ", 3)) {
			/* diffuse */
			sscanf(line + 3, "%f %f %f", &cur_material->diffuse.r,
			  &cur_material->diffuse.g, &cur_material->diffuse.b);
		} else if (!strncmp(line, "Ka ", 3)) {
			/* ambient */
			sscanf(line + 3, "%f %f %f", &cur_material->ambient.r,
			  &cur_material->ambient.g, &cur_material->ambient.b);
		} else if (!strncmp(line, "Ks ", 3)) {
			/* specular */
			sscanf(line + 3, "%f %f %f", &cur_material->specular.r,
			  &cur_material->specular.g, &cur_material->specular.b);
		} else if (!strncmp(line, "map_Kd ", 7)) {
			/* Kd map */
			cur_material->texture_source = strdup(line + 7);
		}
	}

	fclose(in);
}

static int
mesh_chunk_count_triangles(const struct mesh_chunk *chunk)
{
	const struct polygon *p;
	int count;

	count = 0;

	for (p = chunk->polygons; p != &chunk->polygons[chunk->num_polygons]; p++)
		count += p->num_vertices - 2;

	return count;
}

static unsigned
ptr_hash(const void *p)
{
	return (unsigned)p; /* tee-hee. */
}

static int
ptr_equals(const void *p, const void *q)
{
	return p == q;
}

struct chunk_mapper_context {
	int cur_chunk;
	struct mesh *mesh;
	struct list *vertex_list;
	struct list *texuv_list;
	struct list *normal_list;
};

struct chunk_vertex {
	int index;
	vector3 *pos;
	vector3 *normal;
	vector2 *texuv;
};

static void
vertex_mapper(unsigned key, struct chunk_vertex *vt, struct vertex *chunk_vertices)
{
	chunk_vertices[vt->index].pos = *vt->pos;

	if (vt->normal)
		chunk_vertices[vt->index].normal = *vt->normal;
	else
		vec3_zero(&chunk_vertices[vt->index].normal);

	if (vt->texuv)
		chunk_vertices[vt->index].texuv = *vt->texuv;
	else
		vec2_zero(&chunk_vertices[vt->index].texuv);
}

static void
chunk_mapper(struct material *material, struct list *polygon_list, struct chunk_mapper_context *ctx)
{
	struct list_node *ln;
	struct mesh_chunk *chunk = &ctx->mesh->chunks[ctx->cur_chunk++];
	struct polygon *q;
	struct hash_table *vertex_hash_table;

	vertex_hash_table = hash_table_make(311, ptr_hash, ptr_equals);

	chunk->material = material;
	chunk->num_polygons = polygon_list->length;
	chunk->polygons = malloc(chunk->num_polygons*sizeof *chunk->polygons);

	chunk->num_vertices = 0;

	chunk->gl_index_array_size = 0;
	chunk->gl_index_array = NULL;
	chunk->gl_vertex_array = NULL;
	chunk->texture_id = 0;

	q = chunk->polygons;

	for (ln = polygon_list->first; ln; ln = ln->next) {
		struct obj_polygon *p = (struct obj_polygon *)ln->data;
		int i;

		q->num_vertices = p->num_vertices;
		vec3_zero(&q->normal);

		assert(q->num_vertices >= 3);

		for (i = 0; i < p->num_vertices; i++) {
			const struct vtx_index *vi = &p->vtx_index[i];
			struct chunk_vertex *v;
			unsigned key;

			key = vi->coord;
			if (vi->texuv != -1)
				key |= (vi->texuv << 10);
			if (vi->normal != -1)
				key |= (vi->normal << 20);

			v = hash_table_get(vertex_hash_table, (void *)key);

			if (v == NULL) {
				v = malloc(sizeof *v);

				v->index = chunk->num_vertices++;
				v->pos = (vector3 *)list_element_at(ctx->vertex_list, vi->coord);

				if (vi->normal == -1)
					v->normal = NULL;
				else
					v->normal = (vector3 *)list_element_at(ctx->normal_list, vi->normal);

				if (vi->texuv == -1)
					v->texuv = NULL;
				else
					v->texuv = (vector2 *)list_element_at(ctx->texuv_list, vi->texuv);

				hash_table_put(vertex_hash_table, (void *)key, v);

			}

			q->vtx_index[i] = v->index;
		}

		++q;
	}

	chunk->vertices = malloc(chunk->num_vertices*sizeof *chunk->vertices);
	hash_table_map(vertex_hash_table,
	  (void(*)(const void *, void *, void *))vertex_mapper, chunk->vertices);
	hash_table_free(vertex_hash_table, NULL, free);

#ifdef DEBUG
	printf("chunk %d: %d polygons, %d vertices\n", ctx->cur_chunk - 1, chunk->num_polygons, chunk->num_vertices);
#endif
}

static void
mesh_initialize_chunks(struct mesh *mesh, struct hash_table *chunk_hash_table, struct list *vertex_list, struct list *texuv_list, struct list *normal_list)
{
	struct chunk_mapper_context ctx;

	mesh->num_chunks = hash_table_num_entries(chunk_hash_table);
	mesh->chunks = malloc(mesh->num_chunks*sizeof *mesh->chunks);

#ifdef DEBUG
	printf("%d chunks\n", mesh->num_chunks);
#endif

	ctx.cur_chunk = 0;
	ctx.mesh = mesh;
	ctx.vertex_list = vertex_list;
	ctx.texuv_list = texuv_list;
	ctx.normal_list = normal_list;

	hash_table_map(chunk_hash_table,
	  (void(*)(const void *, void *, void *))chunk_mapper, &ctx);
}

static void
polygon_list_free(struct list *polygon_list)
{
	list_free(polygon_list, free);
}

struct mesh *
mesh_load_obj(const char *file_name)
{
	FILE *in;
	static char line[256];
	struct list *vertex_list;
	struct list *normal_list;
	struct list *texuv_list;
	/* materials loaded from .mtl file */
	struct dict *material_dict;
	/* key: ptr to material; value: list of polygons */
	struct hash_table *chunk_hash_table;
	struct material *cur_material;
	struct mesh *mesh;

	if ((in = fopen(file_name, "r")) == NULL)
		panic("mesh_load_obj: failed to open `%s': %s",
		  file_name, strerror(errno));

	vertex_list = list_make();
	normal_list = list_make();
	texuv_list = list_make();
	material_dict = dict_make();
	chunk_hash_table = hash_table_make(31, ptr_hash, ptr_equals);

	cur_material = NULL;

	/* read stuff from file. */

	while (fgets(line, sizeof line, in) != NULL) {
		chomp(line);

		if (!strncmp("mtllib ", line, 7)) {
			/* material library */
			load_mtl(line + 7, material_dict);
		} else if (!strncmp("usemtl ", line, 7)) {
			/* use material */
			cur_material = dict_get(material_dict, line + 7);
		} else if (!strncmp("v ", line, 2)) {
			/* vertex */
			vector3 *p = malloc(sizeof *p);
			sscanf(line + 2, "%f %f %f", &p->x, &p->y, &p->z);
			list_append(vertex_list, p);
		} else if (!strncmp("vn ", line, 3)) {
			/* normal */
			vector3 *p = malloc(sizeof *p);
			sscanf(line + 2, "%f %f %f", &p->x, &p->y, &p->z);
			list_append(normal_list, p);
		} else if (!strncmp("vt ", line, 3)) {
			/* texture uv */
			vector2 *p = malloc(sizeof *p);
			sscanf(line + 2, "%f %f", &p->x, &p->y);
			list_append(texuv_list, p);
		} else if (!strncmp("f ", line, 2)) {
			/* face */
			struct obj_polygon *poly;
			struct list *poly_list;
			char *q;

			poly = calloc(1, sizeof *poly);

			for (q = strtok(line + 2, " "); q; q = strtok(NULL, " ")) {
				int coord_index, texuv_index, normal_index;
				char *pe, *nptr;
				struct vtx_index *v;

				coord_index = strtol(q, &pe, 10);
				if (pe == q || *pe != '/')
					panic("corrupt obj file");

				nptr = pe + 1;
				texuv_index = strtol(nptr, &pe, 10);
				if (*pe != '/')
					panic("corrupt obj file");

				nptr = pe + 1;
				normal_index = strtol(nptr, &pe, 10);

				v = &poly->vtx_index[poly->num_vertices++];
				v->coord = coord_index - 1;
				v->texuv = texuv_index - 1;
				v->normal = normal_index - 1;
			}

			assert(poly->num_vertices >= 3);

			poly_list = hash_table_get(chunk_hash_table,
			  cur_material);

			if (poly_list == NULL) {
				poly_list = list_make();
				hash_table_put(chunk_hash_table, cur_material,
				  poly_list);
			}

			list_append(poly_list, poly);
		}
	}

	fclose(in);

	/* assemble mesh */
	mesh = calloc(1, sizeof *mesh);

	/* mesh_initialize_vertices(mesh, vertex_list); */
	mesh_initialize_chunks(mesh, chunk_hash_table, vertex_list, texuv_list, normal_list);

	/* free temporary data */ 
	list_free(vertex_list, free);
	list_free(normal_list, free);
	list_free(texuv_list, free);
	hash_table_free(chunk_hash_table, NULL,
	  (void(*)(void *))polygon_list_free);
	dict_free(material_dict, NULL);

	return mesh;
}

static void
mesh_chunk_render(const struct mesh_chunk *chunk, const struct rgb *mix_color, float mix_factor)
{
#ifdef USE_CG
	struct vert_shader *vert_shader;
	struct frag_shader *frag_shader;

	if (chunk->texture_id) {
		vert_shader = &tex_vert_shader;
		frag_shader = &tex_frag_shader;
	} else {
		vert_shader = &no_tex_vert_shader;
		frag_shader = &no_tex_frag_shader;
	}

	enable_vert_shader(vert_shader, chunk->material, mix_color, mix_factor);
	enable_frag_shader(frag_shader, mix_color, mix_factor);
#else
	set_opengl_material(chunk->material, 1);
#endif

	if (chunk->texture_id) {
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, gl_texture_id(chunk->texture_id));
	} else {
		glDisable(GL_TEXTURE_2D);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(struct gl_vertex),
	  &chunk->gl_vertex_array[0].pos);

	glEnableClientState(GL_NORMAL_ARRAY);
	glNormalPointer(GL_FLOAT, sizeof(struct gl_vertex),
	  &chunk->gl_vertex_array[0].normal);

	if (chunk->texture_id) {
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, sizeof(struct gl_vertex),
		  &chunk->gl_vertex_array[0].texuv);
	}

	glDrawElements(GL_TRIANGLES, chunk->gl_index_array_size,
	  GL_UNSIGNED_SHORT, chunk->gl_index_array);
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

#ifdef USE_CG
	cgGLDisableProfile(cg_vert_profile);
	cgGLDisableProfile(cg_frag_profile);
#endif
}

void
mesh_render_modulate_color(const struct mesh *mesh, const struct rgb *mix_color,
  float mix_factor)
{
	const struct mesh_chunk *p, *end;

	end = &mesh->chunks[mesh->num_chunks];
	for (p = mesh->chunks; p != end; p++)
		mesh_chunk_render(p, mix_color, mix_factor);
}

void
mesh_render(const struct mesh *mesh)
{
	const static struct rgb dummy = { 0, 0, 0 };

	mesh_render_modulate_color(mesh, &dummy, 1);
}

void
initialize_mesh(void)
{
#ifdef USE_CG
	load_vert_shader(&no_tex_vert_shader, "vert_shader.cg", "vert_shader_main");
	load_vert_shader(&tex_vert_shader, "tex_vert_shader.cg", "tex_vert_shader_main");

	load_frag_shader(&no_tex_frag_shader, "frag_shader.cg", "frag_shader_main");
	load_frag_shader(&tex_frag_shader, "tex_frag_shader.cg", "tex_frag_shader_main");
#endif
}

void
tear_down_mesh(void)
{
#ifdef USE_CG
	tear_down_vert_shader(&tex_vert_shader);
	tear_down_vert_shader(&no_tex_vert_shader);

	tear_down_frag_shader(&tex_frag_shader);
	tear_down_frag_shader(&no_tex_frag_shader);
#endif
}

static void
chunk_transform(struct mesh_chunk *chunk, const struct matrix *t)
{
	struct polygon *p, *end;

	end = &chunk->polygons[chunk->num_polygons];
	for (p = chunk->polygons; p != end; p++) {
		mat_rotate(&p->normal, t);
		vec3_normalize(&p->normal);
	}
}

static void
mesh_transform_chunks(struct mesh *mesh, const struct matrix *t)
{
	struct mesh_chunk *p;

	for (p = mesh->chunks; p != &mesh->chunks[mesh->num_chunks]; p++)
		chunk_transform(p, t);
}

static void
mesh_chunk_initialize_gl_vertex_array(struct mesh_chunk *chunk)
{
	int i;

	assert(chunk->gl_vertex_array == NULL);

	chunk->gl_vertex_array =
	  malloc(chunk->num_vertices*sizeof *chunk->gl_vertex_array);

	for (i = 0; i < chunk->num_vertices; i++) {
		chunk->gl_vertex_array[i].pos[0] = chunk->vertices[i].pos.x;
		chunk->gl_vertex_array[i].pos[1] = chunk->vertices[i].pos.y;
		chunk->gl_vertex_array[i].pos[2] = chunk->vertices[i].pos.z;

		chunk->gl_vertex_array[i].normal[0] = chunk->vertices[i].normal.x;
		chunk->gl_vertex_array[i].normal[1] = chunk->vertices[i].normal.y;
		chunk->gl_vertex_array[i].normal[2] = chunk->vertices[i].normal.z;

		chunk->gl_vertex_array[i].texuv[0] = chunk->vertices[i].texuv.x;
		chunk->gl_vertex_array[i].texuv[1] = 1 - chunk->vertices[i].texuv.y;

#if 0
		printf("[%f %f %f] - [%f %f %f] - [%f %f]\n",
		  chunk->gl_vertex_array[i].pos[0],
		  chunk->gl_vertex_array[i].pos[1],
		  chunk->gl_vertex_array[i].pos[2],

		  chunk->gl_vertex_array[i].normal[0],
		  chunk->gl_vertex_array[i].normal[1],
		  chunk->gl_vertex_array[i].normal[2],

		  chunk->gl_vertex_array[i].texuv[0],
		  chunk->gl_vertex_array[i].texuv[1]);
#endif
	}
}

static void
mesh_chunk_initialize_gl_index_array(struct mesh_chunk *chunk)
{
	int i;
	struct polygon *q;
	GLushort *p;

	assert(chunk->gl_index_array == NULL);

	chunk->gl_index_array_size = 3*mesh_chunk_count_triangles(chunk);
	p = chunk->gl_index_array =
	  malloc(chunk->gl_index_array_size*sizeof *chunk->gl_index_array);

	for (q = chunk->polygons;
	  q != &chunk->polygons[chunk->num_polygons]; q++) {
		for (i = 1; i < q->num_vertices - 1; i++) {
			*p++ = q->vtx_index[0];
			*p++ = q->vtx_index[i];
			*p++ = q->vtx_index[i + 1];
		}
	}

	assert(p == &chunk->gl_index_array[chunk->gl_index_array_size]);
}

static void
mesh_chunk_initialize_texture(struct mesh_chunk *chunk)
{
	assert(chunk->texture_id == 0);

	if (chunk->material && chunk->material->texture_source)
		chunk->texture_id = png_to_texture(chunk->material->texture_source);
}

static void
mesh_initialize_gl_resources(struct mesh *mesh)
{
	struct mesh_chunk *p;

	for (p = mesh->chunks; p != &mesh->chunks[mesh->num_chunks]; p++) {
		mesh_chunk_initialize_gl_vertex_array(p);
		mesh_chunk_initialize_gl_index_array(p);
		mesh_chunk_initialize_texture(p);
	}
}

static void
mesh_chunk_transform_vertices(struct mesh_chunk *chunk, const struct matrix *t)
{
	struct vertex *p, *end;

	end = &chunk->vertices[chunk->num_vertices];
	for (p = chunk->vertices; p != end; p++) {
		mat_transform(&p->pos, t);
		mat_rotate(&p->normal, t);
		vec3_normalize(&p->normal);
	}
}

static void
mesh_transform_vertices(struct mesh *mesh, const struct matrix *t)
{
	struct mesh_chunk *p;

	for (p = mesh->chunks; p != &mesh->chunks[mesh->num_chunks]; p++)
		mesh_chunk_transform_vertices(p, t);
}

static void
mesh_chunk_setup_normals(struct mesh_chunk *chunk)
{
	assert(chunk->num_vertices > 0);

	if (vec3_length(&chunk->vertices[0].normal) == 0) {
		mesh_chunk_initialize_poly_normals(chunk);
		mesh_chunk_initialize_vertex_normals(chunk);
	}
}

static void
mesh_setup_normals(struct mesh *mesh)
{
	struct mesh_chunk *p;

	for (p = mesh->chunks; p != &mesh->chunks[mesh->num_chunks]; p++)
		mesh_chunk_setup_normals(p);
}

void
mesh_setup(struct mesh *mesh)
{
	mesh_setup_normals(mesh);
	mesh_initialize_gl_resources(mesh);
}

void
mesh_transform(struct mesh *mesh, const struct matrix *t)
{
	mesh_transform_vertices(mesh, t);
	mesh_transform_chunks(mesh, t);
}

static void
material_release(struct material *material)
{
	if (material->texture_source)
		free(material->texture_source);
	free(material);
}

static void
mesh_chunk_release(struct mesh_chunk *chunk)
{
	if (chunk->material)
		material_release(chunk->material);
	free(chunk->polygons);
	if (chunk->gl_index_array)
		free(chunk->gl_index_array);
	if (chunk->gl_vertex_array)
		free(chunk->gl_vertex_array);
	free(chunk->vertices);
}

void
mesh_release(struct mesh *mesh)
{
	struct mesh_chunk *p;
	for (p = mesh->chunks; p != &mesh->chunks[mesh->num_chunks]; p++)
		mesh_chunk_release(p);
	free(mesh->chunks);
}
