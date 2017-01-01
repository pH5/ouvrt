/*
 * HTC Vive Controller (via USB)
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_CONTROLLER_USB_H__
#define __VIVE_CONTROLLER_USB_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_CONTROLLER_USB		(ouvrt_vive_controller_usb_get_type())
#define OUVRT_VIVE_CONTROLLER_USB(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_USB, \
						OuvrtViveControllerUSB))
#define OUVRT_IS_VIVE_CONTROLLER_USB(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_USB))
#define OUVRT_VIVE_CONTROLLER_USB_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
						OUVRT_TYPE_VIVE_CONTROLLER_USB, \
						OuvrtViveControllerUSBClass))
#define OUVRT_IS_VIVE_CONTROLLER_USB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
						OUVRT_TYPE_VIVE_CONTROLLER_USB))
#define OUVRT_VIVE_CONTROLLER_USB_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
						OUVRT_TYPE_VIVE_CONTROLLER_USB, \
						OuvrtViveControllerUSBClass))

typedef struct _OuvrtViveControllerUSB		OuvrtViveControllerUSB;
typedef struct _OuvrtViveControllerUSBClass	OuvrtViveControllerUSBClass;
typedef struct _OuvrtViveControllerUSBPrivate	OuvrtViveControllerUSBPrivate;

struct _OuvrtViveControllerUSBPrivate;

struct _OuvrtViveControllerUSB {
	OuvrtDevice dev;

	OuvrtViveControllerUSBPrivate *priv;
};

struct _OuvrtViveControllerUSBClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_vive_controller_usb_get_type(void);

OuvrtDevice *vive_controller_usb_new(const char *devnode);

#endif /* __VIVE_CONTROLLER_USB_H__ */
