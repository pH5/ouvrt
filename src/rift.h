/*
 * Oculus Rift HMDs
 * Copyright 2015-2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __RIFT_H__
#define __RIFT_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"
#include "leds.h"
#include "math.h"
#include "tracker.h"

#define OUVRT_TYPE_RIFT			(ouvrt_rift_get_type())
#define OUVRT_RIFT(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_RIFT, OuvrtRift))
#define OUVRT_IS_RIFT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_RIFT))
#define OUVRT_RIFT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), \
					OUVRT_TYPE_RIFT, \
					OuvrtRiftClass))
#define OUVRT_IS_RIFT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_RIFT))
#define OUVRT_RIFT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_RIFT, \
					 OuvrtRiftClass))

#define MAX_POSITIONS	(MAX_LEDS + 1)

struct imu {
	vec3 position;
};

typedef struct _OuvrtRift		OuvrtRift;
typedef struct _OuvrtRiftClass		OuvrtRiftClass;
typedef struct _OuvrtRiftPrivate	OuvrtRiftPrivate;

struct _OuvrtRiftPrivate;

struct _OuvrtRift {
	OuvrtDevice dev;
	OuvrtTracker *tracker;

	struct leds leds;
	struct imu imu;

	OuvrtRiftPrivate *priv;
};

struct _OuvrtRiftClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_rift_get_type(void);

OuvrtDevice *rift_dk2_new(const char *devnode);

void ouvrt_rift_set_flicker(OuvrtRift *camera, gboolean flicker);

#endif /* __RIFT_H__ */
