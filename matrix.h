#ifndef MATRIX_H_
#define MATRIX_H_

#include "vector.h"

typedef struct matrix {
	float m11, m12, m13, m14;
	float m21, m22, m23, m24;
	float m31, m32, m33, m34;
} matrix;

void
mat_copy(matrix *r, const matrix *m);

void
mat_make_identity(matrix *m);

void
mat_make_rotation_around_x(matrix *m, const float ang);

void
mat_make_rotation_around_y(matrix *m, const float ang);

void
mat_make_rotation_around_z(matrix *m, const float ang);

void
mat_make_translation(matrix *m, const float x, const float y,
  const float z);

void
mat_make_translation_from_vec(matrix *m, const vector3 *pos);

void
mat_make_scale(matrix *m, const float s);

void
mat_make_scale_xyz(matrix *m, const float sx, const float sy,
  const float sz);

void
mat_mul(matrix *a, const matrix *b);

void
mat_mul_copy(matrix *r, const matrix *a, const matrix *b);

void
mat_invert(matrix *m);

void
mat_invert_copy(matrix *r, const matrix *m);

void
mat_transpose(matrix *m);

void
mat_transpose_copy(matrix *r, const matrix *m);

void
mat_get_col(matrix *m, vector3 *v, const int col);

void
mat_get_row(matrix *m, vector3 *v, const int col);

void
mat_transform(vector3 *v, const matrix *m);

void
mat_transform_copy(vector3 *r, const matrix *m, const vector3 *v);

void
mat_transform_batch(vector3 *r, const matrix *m, const vector3 *v, const int n);

void
mat_rotate(vector3 *v, const matrix *m);

void
mat_rotate_copy(vector3 *r, const matrix *m, const vector3 *v);

void
mat_rotate_batch(vector3 *r, const matrix *m, const vector3 *v, const int n);

void
mat_make_look_at(matrix *m, const vector3 *o, const vector3 *p);

void
mat_print(const matrix *m);

#endif /* MATRIX_H_ */
