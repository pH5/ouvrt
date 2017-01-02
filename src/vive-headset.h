/*
 * HTC Vive Headset
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_HEADSET_H__
#define __VIVE_HEADSET_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_HEADSET		(ouvrt_vive_headset_get_type())
#define OUVRT_VIVE_HEADSET(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_VIVE_HEADSET, \
					 OuvrtViveHeadset))
#define OUVRT_IS_VIVE_HEADSET(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_VIVE_HEADSET))
#define OUVRT_VIVE_HEADSET_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
					 OUVRT_TYPE_VIVE_HEADSET, \
					 OuvrtViveHeadsetClass))
#define OUVRT_IS_VIVE_HEADSET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_VIVE_HEADSET))
#define OUVRT_VIVE_HEADSET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_VIVE_HEADSET, \
					 OuvrtViveHeadsetClass))

typedef struct _OuvrtViveHeadset		OuvrtViveHeadset;
typedef struct _OuvrtViveHeadsetClass		OuvrtViveHeadsetClass;
typedef struct _OuvrtViveHeadsetPrivate		OuvrtViveHeadsetPrivate;

struct _OuvrtViveHeadsetPrivate;

struct _OuvrtViveHeadset {
	OuvrtDevice dev;

	OuvrtViveHeadsetPrivate *priv;
};

struct _OuvrtViveHeadsetClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_vive_headset_get_type(void);

OuvrtDevice *vive_headset_new(const char *devnode);

#endif /* __VIVE_HEADSET_H__ */
