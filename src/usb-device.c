/*
 * USB device class
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <errno.h>
#include <libusb.h>
#include <stdbool.h>

#include "usb-device.h"

typedef struct {
	uint16_t vid;
	uint16_t pid;
	libusb_context *context;
	libusb_device_handle *devh;
	int completed;
} OuvrtUSBDevicePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(OuvrtUSBDevice, ouvrt_usb_device, \
				    OUVRT_TYPE_DEVICE)

libusb_device_handle *ouvrt_usb_device_get_handle(OuvrtUSBDevice *self)
{
	OuvrtUSBDevicePrivate *priv = ouvrt_usb_device_get_instance_private(self);

	return priv->devh;
}

/*
 * Sets the vendor id and product id to match in open.
 */
void ouvrt_usb_device_set_vid_pid(OuvrtUSBDevice *self, uint16_t vid,
				  uint16_t pid)
{
	OuvrtUSBDevicePrivate *priv = ouvrt_usb_device_get_instance_private(self);

	priv->vid = vid;
	priv->pid = pid;
}

/*
 * Opens the USB device.
 */
static int ouvrt_usb_device_open(OuvrtDevice *dev)
{
	OuvrtUSBDevice *self = OUVRT_USB_DEVICE(dev);
	OuvrtUSBDevicePrivate *priv = ouvrt_usb_device_get_instance_private(self);
	struct libusb_device_descriptor desc;
	libusb_device **devices;
	uint8_t bus, address;
	gchar *endp;
	ssize_t num;
	int ret;
	int i;

	if (!g_str_has_prefix(dev->devnode, "/dev/bus/usb/"))
		return -ENODEV;

	bus = g_ascii_strtoull(dev->devnode + 13, &endp, 10);
	if (*endp != '/')
		return -ENODEV;

	address = g_ascii_strtoull(endp + 1, NULL, 10);

	libusb_init(&priv->context);

	num = libusb_get_device_list(priv->context, &devices);
	if (num < 0)
		return num;
	for (i = 0; i < num; i++) {
		ret = libusb_get_device_descriptor(devices[i], &desc);
		if (ret < 0)
			return ret;

		if (desc.idVendor == priv->vid && desc.idProduct == priv->pid &&
		    bus == libusb_get_bus_number(devices[i]) &&
		    address == libusb_get_device_address(devices[i]))
			break;
	}
	if (i == num) {
		libusb_free_device_list(devices, 1);
		return -ENODEV;
	}

	int speed = libusb_get_device_speed(devices[i]);
	switch (speed) {
	case LIBUSB_SPEED_HIGH:
		g_print("%s: USB2\n", dev->name);
		break;
	case LIBUSB_SPEED_SUPER:
		g_print("%s: USB3\n", dev->name);
		break;
	}

	ret = libusb_open(devices[i], &priv->devh);
	libusb_free_device_list(devices, 1);
	if (ret < 0) {
		if (ret == LIBUSB_ERROR_ACCESS) {
			g_print("%s: failed to open: %d (access denied) \n",
				dev->name, ret);
		} else {
			g_print("%s: failed to open: %d\n", dev->name, ret);
		}
		return ret;
	}

	return 0;
}

/*
 * Handles USB transfers.
 */
static void ouvrt_usb_device_thread(OuvrtDevice *dev)
{
	OuvrtUSBDevice *self = OUVRT_USB_DEVICE(dev);
	OuvrtUSBDevicePrivate *priv = ouvrt_usb_device_get_instance_private(self);
	struct timeval tv = {
		.tv_sec = 1,
	};
	int ret;

	while (dev->active) {
		ret = libusb_handle_events_timeout_completed(priv->context, &tv,
							     &priv->completed);
		if (ret != 0) {
			g_print("libusb_handle_events failed with: %d\n", ret);
			break;
		}
		if (priv->completed)
			dev->active = false;
	}
}

/*
 * Closes the USB device.
 */
static void ouvrt_usb_device_close(OuvrtDevice *dev)
{
	OuvrtUSBDevice *self = OUVRT_USB_DEVICE(dev);
	OuvrtUSBDevicePrivate *priv = ouvrt_usb_device_get_instance_private(self);

	libusb_close(priv->devh);
	priv->devh = NULL;
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_usb_device_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_usb_device_parent_class)->finalize(object);
}

static void ouvrt_usb_device_class_init(OuvrtUSBDeviceClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_usb_device_finalize;
	OUVRT_DEVICE_CLASS(klass)->open = ouvrt_usb_device_open;
	OUVRT_DEVICE_CLASS(klass)->thread = ouvrt_usb_device_thread;
	OUVRT_DEVICE_CLASS(klass)->close = ouvrt_usb_device_close;
}

static void ouvrt_usb_device_init(G_GNUC_UNUSED OuvrtUSBDevice *self)
{
}
