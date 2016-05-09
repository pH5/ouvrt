/*
 * HTC Vive Headset Lighthouse Receiver
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_HEADSET_LIGHTHOUSE_H__
#define __VIVE_HEADSET_LIGHTHOUSE_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_HEADSET_LIGHTHOUSE	(ouvrt_vive_headset_lighthouse_get_type())
#define OUVRT_VIVE_HEADSET_LIGHTHOUSE(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_VIVE_HEADSET_LIGHTHOUSE, \
					 OuvrtViveHeadsetLighthouse))
#define OUVRT_IS_VIVE_HEADSET_LIGHTHOUSE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_VIVE_HEADSET_LIGHTHOUSE))
#define OUVRT_VIVE_HEADSET_LIGHTHOUSE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
					     OUVRT_TYPE_VIVE_HEADSET_LIGHTHOUSE, \
					     OuvrtViveHeadsetLighthouseClass))
#define OUVRT_IS_VIVE_HEADSET_LIGHTHOUSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
					        OUVRT_TYPE_VIVE_HEADSET_LIGHTHOUSE))
#define OUVRT_VIVE_HEADSET_LIGHTHOUSE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
					       OUVRT_TYPE_VIVE_HEADSET_LIGHTHOUSE, \
					       OuvrtViveHeadsetLighthouseClass))

typedef struct _OuvrtViveHeadsetLighthouse		OuvrtViveHeadsetLighthouse;
typedef struct _OuvrtViveHeadsetLighthouseClass	OuvrtViveHeadsetLighthouseClass;
typedef struct _OuvrtViveHeadsetLighthousePrivate	OuvrtViveHeadsetLighthousePrivate;

struct _OuvrtViveHeadsetLighthousePrivate;

struct _OuvrtViveHeadsetLighthouse {
	OuvrtDevice dev;

	OuvrtViveHeadsetLighthousePrivate *priv;
};

struct _OuvrtViveHeadsetLighthouseClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_vive_headset_lighthouse_get_type(void);

OuvrtDevice *vive_headset_lighthouse_new(const char *devnode);

#endif /* __VIVE_HEADSET_LIGHTHOUSE_H__ */
