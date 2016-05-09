/*
 * HTC Vive Controller
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_CONTROLLER_H__
#define __VIVE_CONTROLLER_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_CONTROLLER	(ouvrt_vive_controller_get_type())
#define OUVRT_VIVE_CONTROLLER(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_VIVE_CONTROLLER, \
					 OuvrtViveController))
#define OUVRT_IS_VIVE_CONTROLLER(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_VIVE_CONTROLLER))
#define OUVRT_VIVE_CONTROLLER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
					    OUVRT_TYPE_VIVE_CONTROLLER, \
					    OuvrtViveControllerClass))
#define OUVRT_IS_VIVE_CONTROLLER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
					       OUVRT_TYPE_VIVE_CONTROLLER))
#define OUVRT_VIVE_CONTROLLER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
					      OUVRT_TYPE_VIVE_CONTROLLER, \
					      OuvrtViveControllerClass))

typedef struct _OuvrtViveController	OuvrtViveController;
typedef struct _OuvrtViveControllerClass	OuvrtViveControllerClass;
typedef struct _OuvrtViveControllerPrivate	OuvrtViveControllerPrivate;

struct _OuvrtViveControllerPrivate;

struct _OuvrtViveController {
	OuvrtDevice dev;

	OuvrtViveControllerPrivate *priv;
};

struct _OuvrtViveControllerClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_vive_controller_get_type(void);

OuvrtDevice *vive_controller_new(const char *devnode);

#endif /* __VIVE_CONTROLLER_H__ */
