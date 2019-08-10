/*
 * USB device class
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __USB_DEVICE_H__
#define __USB_DEVICE_H__

#include <glib.h>
#include <glib-object.h>
#include <libusb.h>
#include <stdint.h>

#include "device.h"

G_BEGIN_DECLS

#define OUVRT_TYPE_USB_DEVICE (ouvrt_usb_device_get_type())
G_DECLARE_DERIVABLE_TYPE(OuvrtUSBDevice, ouvrt_usb_device, OUVRT, USB_DEVICE, \
			 OuvrtDevice)

struct _OuvrtUSBDeviceClass {
	OuvrtDeviceClass parent_class;
};

libusb_device_handle *ouvrt_usb_device_get_handle(OuvrtUSBDevice *self);
void ouvrt_usb_device_set_vid_pid(OuvrtUSBDevice *self, uint16_t vid,
				  uint16_t pid);

G_END_DECLS

#endif /* __USB_DEVICE_H__ */
