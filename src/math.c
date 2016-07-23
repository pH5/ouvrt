/*
 * Math helpers
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#include <math.h>

#include "math.h"

float f16_to_float(uint16_t f16)
{
	unsigned int sign = f16 >> 15;
	unsigned int exponent = (f16 >> 10) & 0x1f;
	unsigned int mantissa = f16 & 0x3ff;
	union {
		float f32;
		uint32_t u32;
	} u;

	if (exponent == 0) {
		if (!mantissa) {
			/* zero */
			u.u32 = sign << 31;
		} else {
			/* subnormal */
			exponent = 127 - 14;
			mantissa <<= 23 - 10;
			/*
			 * convert to normal representation:
			 * shift up mantissa and drop MSB
			 */
			while (!(mantissa & (1 << 23))) {
				mantissa <<= 1;
				exponent--;
			}
			mantissa &= 0x7fffffu;
			u.u32 = (sign << 31) | (exponent << 23) | mantissa;
		}
	} else if (exponent < 31) {
		/* normal */
		exponent += 127 - 15;
		mantissa <<= 23 - 10;
		u.u32 = (sign << 31) | (exponent << 23) | mantissa;
	} else if (mantissa == 0) {
		/* infinite */
		u.u32 = (sign << 31) | (255 << 23);
	} else {
		/* NaN */
		u.u32 = 0x7fffffffu;
	}
	return u.f32;
}

void dquat_from_axis_angle(dquat *q, dvec3 *axis, double angle)
{
	const double half_angle = angle * 0.5;
	const double sin_half_angle = sin(half_angle);

	q->w = cos(half_angle);
	q->x = sin_half_angle * axis->x;
	q->y = sin_half_angle * axis->y;
	q->z = sin_half_angle * axis->z;
}

void vec3_normalize(vec3 *v)
{
	float scale = 1.0f / sqrt(v->x * v->x + v->y * v->y + v->z * v->z);

	v->x *= scale;
	v->y *= scale;
	v->z *= scale;
}
