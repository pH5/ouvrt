/*
 * Math helpers
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __MATH_H__
#define __MATH_H__

#include <math.h>
#include <stdint.h>

typedef struct {
	float x, y, z;
} vec3;

typedef struct {
	double x, y, z;
} dvec3;

typedef struct {
	double x, y, z, w;
} dquat;

typedef struct {
	double m[9];
} dmat3;

float f16_to_float(uint16_t f16);
void dquat_from_axis_angle(dquat *quat, const dvec3 *axis, double angle);

static inline double vec3_dot(const vec3 *a, const vec3 *b)
{
	return a->x * b->x + a->y * b->y + a->z * b->z;
}

static inline double vec3_norm(const vec3 *v)
{
	return sqrt(vec3_dot(v, v));
}

static inline void vec3_normalize(vec3 *v)
{
	const float inv_norm = 1.0f / vec3_norm(v);

	v->x *= inv_norm;
	v->y *= inv_norm;
	v->z *= inv_norm;
}

static inline void vec3_cross(vec3 *c, const vec3 *a, const vec3 *b)
{
	c->x = a->y * b->z - b->y * a->z;
	c->y = a->z * b->x - b->z * a->x;
	c->z = a->x * b->y - b->x * a->y;
}

#endif /* __MATH_H__ */
