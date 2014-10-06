#ifndef VECTOR_H_
#define VECTOR_H_

#include <math.h>

typedef struct vector2 {
	float x, y;
} vector2;

typedef struct vector3 {
	float x, y, z;
} vector3;

/*
 *    V E C T O R 2
 */

static inline void
vec2_zero(vector2 *v)
{
	v->x = v->y = 0.f;
}

static inline void
vec2_set(vector2 *v, float x, float y)
{
	v->x = x;
	v->y = y;
}

static inline float
vec2_dot_product(const vector2 *u, const vector2 *v)
{
	return u->x*v->x + u->y*v->y;
}

static inline float
vec2_length_squared(const vector2 *v)
{
	return vec2_dot_product(v, v);
}

static inline float
vec2_length(const vector2 *v)
{
	return sqrtf(vec2_length_squared(v));
}

static inline void
vec2_add_to(vector2 *u, const vector2 *v)
{
	u->x += v->x;
	u->y += v->y;
}

static inline void
vec2_add(vector2 *v, const vector2 *a, const vector2 *b)
{
	v->x = a->x + b->x;
	v->y = a->y + b->y;
}

static inline void
vec2_sub_from(vector2 *u, const vector2 *v)
{
	u->x -= v->x;
	u->y -= v->y;
}

static inline void
vec2_sub(vector2 *v, const vector2 *a, const vector2 *b)
{
	v->x = a->x - b->x;
	v->y = a->y - b->y;
}

static inline void
vec2_mul_scalar(vector2 *v, float s)
{
	v->x *= s;
	v->y *= s;
}

static inline void
vec2_mul_scalar_copy(vector2 *v, const vector2 *u, float s)
{
	v->x = u->x*s;
	v->y = u->y*s;
}

static inline void
vec2_normalize(vector2 *v)
{
	float l = vec2_length(v);

	if (l != 0.f)
		vec2_mul_scalar(v, 1.f/l);
}

static inline void
vec2_clamp_length(vector2 *v, float ml)
{
	float l = vec2_length_squared(v);

	if (l > ml*ml)
		vec2_mul_scalar(v, ml/sqrtf(l));
}

static inline void
vec2_rotate(vector2 *v, float ang)
{
	const float s = sin(ang);
	const float c = cos(ang);
	float x = v->x*c + v->y*s;
	float y = v->x*-s + v->y*c;

	v->x = x;
	v->y = y;
}

static inline float
vec2_distance_squared(const vector2 *u, const vector2 *v)
{
	vector2 d = *v;
	vec2_sub_from(&d, u);
	return vec2_length_squared(&d);
}

static inline float
vec2_distance(const vector2 *u, const vector2 *v)
{
	return sqrtf(vec2_distance_squared(u, v));
}


/*
 *    V E C T O R 3
 */

static inline void
vec3_zero(vector3 *v)
{
	v->x = v->y = v->z = 0.f;
}

static inline void
vec3_set(vector3 *v, float x, float y, float z)
{
	v->x = x;
	v->y = y;
	v->z = z;
}

static inline float
vec3_dot_product(const vector3 *u, const vector3 *v)
{
	return u->x*v->x + u->y*v->y + u->z*v->z;
}

static inline float
vec3_length_squared(const vector3 *v)
{
	return vec3_dot_product(v, v);
}

static inline float
vec3_length(const vector3 *v)
{
	return sqrtf(vec3_length_squared(v));
}

static inline void
vec3_add_to(vector3 *u, const vector3 *v)
{
	u->x += v->x;
	u->y += v->y;
	u->z += v->z;
}

static inline void
vec3_add(vector3 *v, const vector3 *u, const vector3 *w)
{
	v->x = u->x + w->x;
	v->y = u->y + w->y;
	v->z = u->z + w->z;
}

static inline void
vec3_sub_from(vector3 *u, const vector3 *v)
{
	u->x -= v->x;
	u->y -= v->y;
	u->z -= v->z;
}

static inline void
vec3_sub(vector3 *v, const vector3 *u, const vector3 *w)
{
	v->x = u->x - w->x;
	v->y = u->y - w->y;
	v->z = u->z - w->z;
}

static inline void
vec3_mul_scalar(vector3 *v, float s)
{
	v->x *= s;
	v->y *= s;
	v->z *= s;
}

static inline void
vec3_mul_scalar_copy(vector3 *v, const vector3 *u, float s)
{
	v->x = u->x*s;
	v->y = u->y*s;
	v->z = u->z*s;
}

static inline void
vec3_normalize(vector3 *v)
{
	float l = vec3_length(v);

	if (l != 0.f)
		vec3_mul_scalar(v, 1.f/l);
}

static inline void
vec3_clamp_length(vector3 *v, float ml)
{
	float l = vec3_length_squared(v);

	if (l > ml*ml)
		vec3_mul_scalar(v, ml/sqrtf(l));
}

static inline void
vec3_cross_product(vector3 *v, const vector3 *u, const vector3 *w)
{
	v->x = u->y*w->z - u->z*w->y;
	v->y = u->z*w->x - u->x*w->z;
	v->z = u->x*w->y - u->y*w->x;
}

static inline float
vec3_distance_squared(const vector3 *u, const vector3 *v)
{
	vector3 d = *v;
	vec3_sub_from(&d, u);
	return vec3_length_squared(&d);
}

static inline float
vec3_distance(const vector3 *u, const vector3 *v)
{
	return sqrtf(vec3_distance_squared(u, v));
}

#endif /* VECTOR_H_ */
