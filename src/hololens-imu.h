/*
 * Microsoft HoloLens Sensors (Windows Mixed Reality) IMU
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __HOLOLENS_IMU_H__
#define __HOLOLENS_IMU_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_HOLOLENS_IMU (ouvrt_hololens_imu_get_type())
G_DECLARE_FINAL_TYPE(OuvrtHoloLensIMU, ouvrt_hololens_imu, \
		     OUVRT, HOLOLENS_IMU, OuvrtDevice)

OuvrtDevice *hololens_imu_new(const char *devnode);

#endif /* __HOLOLENS_IMU_H__ */
