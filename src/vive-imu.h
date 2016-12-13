/*
 * HTC Vive configuration data readout
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_IMU_H__
#define __VIVE_IMU_H__

#include "math.h"

struct vive_imu {
	uint64_t time;
	uint8_t sequence;
	vec3 acc_bias;
	vec3 acc_scale;
	vec3 gyro_bias;
	vec3 gyro_scale;
};

void vive_imu_decode_message(struct vive_imu *imu, const void *buf, size_t len);

#endif /* __VIVE_IMU_H__ */
