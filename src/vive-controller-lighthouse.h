/*
 * HTC Vive Controller Lighthouse
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_CONTROLLER_LIGHTHOUSE_H__
#define __VIVE_CONTROLLER_LIGHTHOUSE_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_CONTROLLER_LIGHTHOUSE	(ouvrt_vive_controller_lighthouse_get_type())
#define OUVRT_VIVE_CONTROLLER_LIGHTHOUSE(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_LIGHTHOUSE, \
						OuvrtViveControllerLighthouse))
#define OUVRT_IS_VIVE_CONTROLLER_LIGHTHOUSE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_LIGHTHOUSE))
#define OUVRT_VIVE_CONTROLLER_LIGHTHOUSE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
						OUVRT_TYPE_VIVE_CONTROLLER_LIGHTHOUSE, \
						OuvrtViveControllerLighthouseClass))
#define OUVRT_IS_VIVE_CONTROLLER_LIGHTHOUSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						OUVRT_TYPE_VIVE_CONTROLLER_LIGHTHOUSE))
#define OUVRT_VIVE_CONTROLLER_LIGHTHOUSE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_LIGHTHOUSE, \
						OuvrtViveControllerLighthouseClass))

typedef struct _OuvrtViveControllerLighthouse		OuvrtViveControllerLighthouse;
typedef struct _OuvrtViveControllerLighthouseClass	OuvrtViveControllerLighthouseClass;
typedef struct _OuvrtViveControllerLighthousePrivate	OuvrtViveControllerLighthousePrivate;

struct _OuvrtViveControllerLighthousePrivate;

struct _OuvrtViveControllerLighthouse {
	OuvrtDevice dev;

	OuvrtViveControllerLighthousePrivate *priv;
};

struct _OuvrtViveControllerLighthouseClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_vive_controller_lighthouse_get_type(void);

OuvrtDevice *vive_controller_lighthouse_new(const char *devnode);

#endif /* __VIVE_CONTROLLER_LIGHTHOUSE_H__ */
