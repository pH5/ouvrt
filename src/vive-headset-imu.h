/*
 * HTC Vive Headset IMU
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_HEADSET_IMU_H__
#define __VIVE_HEADSET_IMU_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_HEADSET_IMU	(ouvrt_vive_headset_imu_get_type())
#define OUVRT_VIVE_HEADSET_IMU(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_VIVE_HEADSET_IMU, \
					 OuvrtViveHeadsetIMU))
#define OUVRT_IS_VIVE_HEADSET_IMU(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_VIVE_HEADSET_IMU))
#define OUVRT_VIVE_HEADSET_IMU_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
					     OUVRT_TYPE_VIVE_HEADSET_IMU, \
					     OuvrtViveHeadsetIMUClass))
#define OUVRT_IS_VIVE_HEADSET_IMU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
					        OUVRT_TYPE_VIVE_HEADSET_IMU))
#define OUVRT_VIVE_HEADSET_IMU_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
					       OUVRT_TYPE_VIVE_HEADSET_IMU, \
					       OuvrtViveHeadsetIMUClass))

typedef struct _OuvrtViveHeadsetIMU		OuvrtViveHeadsetIMU;
typedef struct _OuvrtViveHeadsetIMUClass	OuvrtViveHeadsetIMUClass;
typedef struct _OuvrtViveHeadsetIMUPrivate	OuvrtViveHeadsetIMUPrivate;

struct _OuvrtViveHeadsetIMUPrivate;

struct _OuvrtViveHeadsetIMU {
	OuvrtDevice dev;

	OuvrtViveHeadsetIMUPrivate *priv;
};

struct _OuvrtViveHeadsetIMUClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_vive_headset_imu_get_type(void);

OuvrtDevice *vive_headset_imu_new(const char *devnode);

#endif /* __VIVE_HEADSET_IMU_H__ */
