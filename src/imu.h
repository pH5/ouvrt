#ifndef __IMU_H__
#define __IMU_H__

#include "math.h"

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

#endif /* __IMU_H__ */
