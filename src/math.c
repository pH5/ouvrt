/*
 * Math helpers
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <math.h>

#include "math.h"

void dquat_from_axis_angle(dquat *q, dvec3 *axis, double angle)
{
	const double half_angle = angle * 0.5;
	const double sin_half_angle = sin(half_angle);

	q->w = cos(half_angle);
	q->x = sin_half_angle * axis->x;
	q->y = sin_half_angle * axis->y;
	q->z = sin_half_angle * axis->z;
}
