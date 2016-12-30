/*
 * HTC Vive Controller Buttons Receiver
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "vive-controller-buttons.h"
#include "vive-hid-reports.h"
#include "device.h"
#include "hidraw.h"
#include "math.h"

struct _OuvrtViveControllerButtonsPrivate {
	const gchar *serial;
	uint32_t buttons;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveControllerButtons, \
			   ouvrt_vive_controller_buttons, OUVRT_TYPE_DEVICE)

void vive_controller_decode_button_message(OuvrtViveControllerButtons *self,
					   const unsigned char *buf, size_t len)
{
	struct vive_controller_button_report *report = (void *)buf;
	uint32_t buttons = report->buttons;

	(void)len;

	if (buttons != self->priv->buttons)
		self->priv->buttons = buttons;
}

/*
 * Opens the Vive Controller Buttons HID device.
 */
static int vive_controller_buttons_start(OuvrtDevice *dev)
{
	int fd = dev->fd;

	free(dev->name);
	dev->name = g_strdup_printf("Vive Controller %s Buttons", dev->serial);

	if (fd == -1) {
		fd = open(dev->devnode, O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("%s: Failed to open '%s': %d\n", dev->name,
				dev->devnode, errno);
			return -1;
		}
		dev->fd = fd;
	}

	return 0;
}

/*
 * Handles button messages.
 */
static void vive_controller_buttons_thread(OuvrtDevice *dev)
{
	OuvrtViveControllerButtons *self = OUVRT_VIVE_CONTROLLER_BUTTONS(dev);
	unsigned char buf[64];
	struct pollfd fds;
	int ret;

	while (dev->active) {
		fds.fd = dev->fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 2000);
		if (ret == -1) {
			g_print("%s: Poll failure: %d\n", dev->name, errno);
			continue;
		}

		if (ret == 0) {
			g_print("%s: Poll timeout\n", dev->name);
			continue;
		}

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			dev->active = FALSE;
			break;
		}

		if (!(fds.revents & POLLIN)) {
			g_print("%s: Unhandled poll event: 0x%x\n", dev->name,
				fds.revents);
			continue;
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("%s: Read error: %d\n", dev->name, errno);
			continue;
		}
		if (ret == 64 && buf[0] == 1) {
			vive_controller_decode_button_message(self, buf, ret);
		} else {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
		}
	}
}

/*
 * Nothing to do here.
 */
static void vive_controller_buttons_stop(OuvrtDevice *dev)
{
	(void)dev;
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_controller_buttons_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_controller_buttons_parent_class)->finalize(object);
}

static void ouvrt_vive_controller_buttons_class_init(OuvrtViveControllerButtonsClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_controller_buttons_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_controller_buttons_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_controller_buttons_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_controller_buttons_stop;
}

static void ouvrt_vive_controller_buttons_init(OuvrtViveControllerButtons *self)
{
	self->dev.type = DEVICE_TYPE_CONTROLLER;
	self->priv = ouvrt_vive_controller_buttons_get_instance_private(self);
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Controller Buttons device.
 */
OuvrtDevice *vive_controller_buttons_new(const char *devnode)
{
	OuvrtViveControllerButtons *vive;

	vive = g_object_new(OUVRT_TYPE_VIVE_CONTROLLER_BUTTONS, NULL);
	if (vive == NULL)
		return NULL;

	vive->dev.devnode = g_strdup(devnode);

	return &vive->dev;
}
