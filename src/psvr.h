/*
 * Sony PlayStation VR Headset
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __PSVR_H__
#define __PSVR_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"
#include "leds.h"
#include "math.h"
#include "tracker.h"

#define OUVRT_TYPE_PSVR			(ouvrt_psvr_get_type())
#define OUVRT_PSVR(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_PSVR, OuvrtPSVR))
#define OUVRT_IS_PSVR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_PSVR))
#define OUVRT_PSVR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), \
					OUVRT_TYPE_PSVR, \
					OuvrtPSVRClass))
#define OUVRT_IS_PSVR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_PSVR))
#define OUVRT_PSVR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_PSVR, \
					 OuvrtPSVRClass))

typedef struct _OuvrtPSVR		OuvrtPSVR;
typedef struct _OuvrtPSVRClass		OuvrtPSVRClass;
typedef struct _OuvrtPSVRPrivate	OuvrtPSVRPrivate;

struct _OuvrtPSVRPrivate;

struct _OuvrtPSVR {
	OuvrtDevice dev;

	OuvrtPSVRPrivate *priv;
};

struct _OuvrtPSVRClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_psvr_get_type(void);

OuvrtDevice *psvr_new(const char *devnode);

#endif /* __PSVR_H__ */
