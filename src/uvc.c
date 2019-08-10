/*
 * UVC Controls
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <glib.h>
#include <libusb.h>
#include <stdint.h>

#define SET_CUR			0x01
#define GET_CUR			0x81
#define GET_LEN			0x85
#define TIMEOUT			1000

#define VS_PROBE_CONTROL	1
#define VS_COMMIT_CONTROL	2

int uvc_set_cur(libusb_device_handle *dev, uint8_t interface, uint8_t entity,
		uint8_t selector, void *data, uint16_t wLength)
{
	uint8_t bmRequestType = LIBUSB_ENDPOINT_OUT |
				LIBUSB_REQUEST_TYPE_CLASS |
				LIBUSB_RECIPIENT_INTERFACE;
	uint8_t bRequest = SET_CUR;
	uint16_t wValue = selector << 8;
	uint16_t wIndex = entity << 8 | interface;
	int ret;

	ret = libusb_control_transfer(dev, bmRequestType, bRequest, wValue,
				      wIndex, data, wLength, TIMEOUT);
	if (ret < 0) {
		g_print("UVC: Failed to transfer SET CUR %u %u %u: %d (%s)\n",
			interface, entity, selector, ret, libusb_strerror(ret));
	}
	return ret;
}

int uvc_get_cur(libusb_device_handle *dev, uint8_t interface, uint8_t entity,
		uint8_t selector, void *data, uint16_t wLength)
{
	uint8_t bmRequestType = LIBUSB_ENDPOINT_IN |
				LIBUSB_REQUEST_TYPE_CLASS |
				LIBUSB_RECIPIENT_INTERFACE;
	uint8_t bRequest = GET_CUR;
	uint16_t wValue = selector << 8;
	uint16_t wIndex = entity << 8 | interface;
	int ret;

	ret = libusb_control_transfer(dev, bmRequestType, bRequest, wValue,
				      wIndex, data, wLength, TIMEOUT);
	if (ret < 0) {
		g_print("UVC: Failed to transfer GET CUR %u %u %u: %d (%s)\n",
			interface, entity, selector, ret, libusb_strerror(ret));
	}
	return ret;
}

int uvc_get_len(libusb_device_handle *dev, uint8_t interface, uint8_t entity,
		uint8_t selector, uint16_t *wLength)
{
	uint8_t bmRequestType = LIBUSB_ENDPOINT_IN |
				LIBUSB_REQUEST_TYPE_CLASS |
				LIBUSB_RECIPIENT_INTERFACE;
	uint8_t bRequest = GET_LEN;
	uint16_t wValue = selector << 8;
	uint16_t wIndex = entity << 8 | interface;
	int ret;

	ret = libusb_control_transfer(dev, bmRequestType, bRequest, wValue,
				      wIndex, (void *)wLength, sizeof(*wLength),
				      TIMEOUT);
	if (ret < 0) {
		g_print("UVC: Failed to transfer GET LEN %u %u %u: %d (%s)\n",
			interface, entity, selector, ret, libusb_strerror(ret));
	}
	return ret;
}
