/*
 * HTC Vive Controller Buttons
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_CONTROLLER_BUTTONS_H__
#define __VIVE_CONTROLLER_BUTTONS_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_CONTROLLER_BUTTONS	(ouvrt_vive_controller_buttons_get_type())
#define OUVRT_VIVE_CONTROLLER_BUTTONS(obj)	(G_TYPE_CHECK_INSTANCE_CAST((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_BUTTONS, \
						OuvrtViveControllerButtons))
#define OUVRT_IS_VIVE_CONTROLLER_BUTTONS(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_BUTTONS))
#define OUVRT_VIVE_CONTROLLER_BUTTONS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
						OUVRT_TYPE_VIVE_CONTROLLER_BUTTONS, \
						OuvrtViveControllerButtonsClass))
#define OUVRT_IS_VIVE_CONTROLLER_BUTTONS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						OUVRT_TYPE_VIVE_CONTROLLER_BUTTONS))
#define OUVRT_VIVE_CONTROLLER_BUTTONS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_BUTTONS, \
						OuvrtViveControllerButtonsClass))

typedef struct _OuvrtViveControllerButtons	OuvrtViveControllerButtons;
typedef struct _OuvrtViveControllerButtonsClass	OuvrtViveControllerButtonsClass;
typedef struct _OuvrtViveControllerButtonsPrivate OuvrtViveControllerButtonsPrivate;

struct _OuvrtViveControllerButtonsPrivate;

struct _OuvrtViveControllerButtons {
	OuvrtDevice dev;

	OuvrtViveControllerButtonsPrivate *priv;
};

struct _OuvrtViveControllerButtonsClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_vive_controller_buttons_get_type(void);

OuvrtDevice *vive_controller_buttons_new(const char *devnode);

#endif /* __VIVE_CONTROLLER_BUTTONS_H__ */
