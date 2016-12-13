/*
 * HTC Vive Controller IMU
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_CONTROLLER_IMU_H__
#define __VIVE_CONTROLLER_IMU_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_CONTROLLER_IMU		(ouvrt_vive_controller_imu_get_type())
#define OUVRT_VIVE_CONTROLLER_IMU(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_IMU, \
						OuvrtViveControllerIMU))
#define OUVRT_IS_VIVE_CONTROLLER_IMU(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_IMU))
#define OUVRT_VIVE_CONTROLLER_IMU_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
						OUVRT_TYPE_VIVE_CONTROLLER_IMU, \
						OuvrtViveControllerIMUClass))
#define OUVRT_IS_VIVE_CONTROLLER_IMU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						OUVRT_TYPE_VIVE_CONTROLLER_IMU))
#define OUVRT_VIVE_CONTROLLER_IMU_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_IMU, \
						OuvrtViveControllerIMUClass))

typedef struct _OuvrtViveControllerIMU		OuvrtViveControllerIMU;
typedef struct _OuvrtViveControllerIMUClass	OuvrtViveControllerIMUClass;
typedef struct _OuvrtViveControllerIMUPrivate	OuvrtViveControllerIMUPrivate;

struct _OuvrtViveControllerIMUPrivate;

struct _OuvrtViveControllerIMU {
	OuvrtDevice dev;

	OuvrtViveControllerIMUPrivate *priv;
};

struct _OuvrtViveControllerIMUClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_vive_controller_imu_get_type(void);

OuvrtDevice *vive_controller_imu_new(const char *devnode);

#endif /* __VIVE_CONTROLLER_IMU_H__ */
