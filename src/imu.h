/*
 * IMU report data structures
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __IMU_H__
#define __IMU_H__

#include <stdint.h>

#include "maths.h"

#define STANDARD_GRAVITY 9.80665 /* m/s² */

/*
 * Raw IMU sample - a single measurement of acceleration, angular
 * velocity, and sample time. Units are hardware dependent and may
 * or may not be calibrated.
 */
struct raw_imu_sample {
	uint64_t time;
	int32_t acc[3];
	int32_t gyro[3];
};

/*
 * Raw IMU sample - a single measurement of acceleration (in m/s²),
 * angular velocity (in rad/s), magnetic field, and temperature,
 * and sample time.
 */
struct imu_sample {
	vec3 acceleration;
	vec3 angular_velocity;
	vec3 magnetic_field;
	float temperature;
	double time;
};

/*
 * Pose - a transform consisting of rotation and translation.
 */
struct dpose {
	dquat rotation;
	dvec3 translation;
};

/*
 * IMU state - a raw IMU sample and derived pose, as well as its first
 * and second derivatives, linear and angular velocity and acceleration.
 */
struct imu_state {
	struct imu_sample sample;
	struct dpose pose;
	vec3 angular_velocity;
	vec3 linear_velocity;
	vec3 angular_acceleration;
	vec3 linear_acceleration;
};

void pose_update(double dt, struct dpose *pose, struct imu_sample *sample);

#endif /* __IMU_H__ */
