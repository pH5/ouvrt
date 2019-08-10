/*
 * HTC Vive Controller (via USB)
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __VIVE_CONTROLLER_USB_H__
#define __VIVE_CONTROLLER_USB_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_CONTROLLER_USB (ouvrt_vive_controller_usb_get_type())
G_DECLARE_FINAL_TYPE(OuvrtViveControllerUSB, ouvrt_vive_controller_usb, OUVRT, \
		     VIVE_CONTROLLER_USB, OuvrtDevice)

OuvrtDevice *vive_controller_usb_new(const char *devnode);

#endif /* __VIVE_CONTROLLER_USB_H__ */
