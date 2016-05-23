/*
 * HTC Vive Headset Mainboard
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_HEADSET_MAINBOARD_H__
#define __VIVE_HEADSET_MAINBOARD_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_HEADSET_MAINBOARD (ouvrt_vive_headset_mainboard_get_type())
#define OUVRT_VIVE_HEADSET_MAINBOARD(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
					   OUVRT_TYPE_VIVE_HEADSET_MAINBOARD, \
					   OuvrtViveHeadsetMainboard))
#define OUVRT_IS_VIVE_HEADSET_MAINBOARD(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
						 OUVRT_TYPE_VIVE_HEADSET_MAINBOARD))
#define OUVRT_VIVE_HEADSET_MAINBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
						   OUVRT_TYPE_VIVE_HEADSET_MAINBOARD, \
						   OuvrtViveHeadsetMainboardClass))
#define OUVRT_IS_VIVE_HEADSET_MAINBOARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						      OUVRT_TYPE_VIVE_HEADSET_MAINBOARD))
#define OUVRT_VIVE_HEADSET_MAINBOARD_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
						     OUVRT_TYPE_VIVE_HEADSET_MAINBOARD, \
						     OuvrtViveHeadsetMainboardClass))

typedef struct _OuvrtViveHeadsetMainboard		OuvrtViveHeadsetMainboard;
typedef struct _OuvrtViveHeadsetMainboardClass		OuvrtViveHeadsetMainboardClass;
typedef struct _OuvrtViveHeadsetMainboardPrivate	OuvrtViveHeadsetMainboardPrivate;

struct _OuvrtViveHeadsetMainboardPrivate;

struct _OuvrtViveHeadsetMainboard {
	OuvrtDevice dev;

	OuvrtViveHeadsetMainboardPrivate *priv;
};

struct _OuvrtViveHeadsetMainboardClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_vive_headset_mainboard_get_type(void);

OuvrtDevice *vive_headset_mainboard_new(const char *devnode);

#endif /* __VIVE_HEADSET_MAINBOARD_H__ */
