/*
 * IMU pose update
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <math.h>

#include "imu.h"
#include "maths.h"

enum pose_mode {
	ACCEL_ONLY,
	GYRO_ONLY,
};

enum pose_mode mode = GYRO_ONLY;

/*
 * Find the quaternion that rotates the local up vector back to where the
 * accelerometer points.
 */
void dquat_from_accel(dquat *q, vec3 *accel)
{
	vec3 a = *accel;
	vec3 up = { 0.0, 1.0, 0.0 };

	vec3_normalize(&a);
	dquat_from_axes(q, &a, &up);
}

/*
 * Updates the rotational part of the pose, given a time interval and angular
 * velocity measurement.
 */
void pose_update(double dt, struct dpose *pose, struct imu_sample *sample)
{
	dquat q, dq;

	switch (mode) {
	case ACCEL_ONLY:
		dquat_from_accel(&q, &sample->acceleration);
		break;
	case GYRO_ONLY:
		dquat_from_gyro(&dq, &sample->angular_velocity, dt);
		dquat_mult(&q, &pose->rotation, &dq);
		dquat_normalize(&q);
		break;
	}

	pose->rotation = q;
}
