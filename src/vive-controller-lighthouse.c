/*
 * HTC Vive Controller Lighthouse Receiver
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "vive-controller-lighthouse.h"
#include "vive-hid-reports.h"
#include "lighthouse.h"
#include "device.h"
#include "math.h"

struct _OuvrtViveControllerLighthousePrivate {
	const gchar *serial;
	struct lighthouse_watchman watchman;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveControllerLighthouse, \
			   ouvrt_vive_controller_lighthouse, OUVRT_TYPE_DEVICE)

/*
 * Decodes the periodic Lighthouse receiver message containing IR pulse
 * timing measurements.
 */
static void
vive_controller_lighthouse_decode_pulse_report(OuvrtViveControllerLighthouse *self,
					     const void *buf)
{
	const struct vive_controller_lighthouse_pulse_report *report = buf;
	unsigned int i;

	/* The pulses may appear in arbitrary order */
	for (i = 0; i < 7; i++) {
		const struct vive_controller_lighthouse_pulse *pulse;
		uint16_t sensor_id;
		uint16_t duration;
		uint32_t timestamp;

		pulse = &report->pulse[i];

		sensor_id = __le16_to_cpu(pulse->id);
		if (sensor_id == 0xffff)
			continue;

		if (sensor_id > 31) {
			g_print("%s: unhandled sensor id: %04x\n",
				self->dev.name, sensor_id);
			for (i = 0; i < sizeof(*report); i++)
				g_print("%02x ", ((unsigned char *)buf)[i]);
			g_print("\n");
			return;
		}

		timestamp = __le32_to_cpu(pulse->timestamp);
		duration = __le16_to_cpu(pulse->duration);

		lighthouse_watchman_handle_pulse(&self->priv->watchman,
						 sensor_id, duration,
						 timestamp);
	}
}

/*
 * Opens the Vive Controller Lighthouse Receiver HID device.
 */
static int vive_controller_lighthouse_start(OuvrtDevice *dev)
{
	OuvrtViveControllerLighthouse *self = OUVRT_VIVE_CONTROLLER_LIGHTHOUSE(dev);
	int fd = dev->fd;

	if (fd == -1) {
		fd = open(dev->devnode, O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("%s: Failed to open '%s': %d\n", dev->name,
				dev->devnode, errno);
			return -1;
		}
		dev->fd = fd;
	}

	self->priv->watchman.name = dev->name;

	return 0;
}

/*
 * Handles Lighthouse Receiver messages.
 */
static void vive_controller_lighthouse_thread(OuvrtDevice *dev)
{
	OuvrtViveControllerLighthouse *self = OUVRT_VIVE_CONTROLLER_LIGHTHOUSE(dev);
	unsigned char buf[64];
	struct pollfd fds;
	int ret;

	while (dev->active) {
		fds.fd = dev->fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		if (ret == -1) {
			g_print("%s: Poll failure: %d\n", dev->name, errno);
			continue;
		}

		if (ret == 0) {
			/* No Lighthouse base station visible */
			if (self->priv->watchman.base_visible) {
				g_print("%s: Lost base station visibility\n",
					dev->name);
				self->priv->watchman.base_visible = FALSE;
			}
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

		if (!self->priv->watchman.base_visible) {
			g_print("%s: Spotted a base station\n", dev->name);
			self->priv->watchman.base_visible = TRUE;
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("%s: Read error: %d\n", dev->name, errno);
			continue;
		}
		if (ret == 58 &&
		    buf[0] == VIVE_CONTROLLER_LIGHTHOUSE_PULSE_REPORT_ID) {
			vive_controller_lighthouse_decode_pulse_report(self, buf);
		} else {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
		}
	}
}

/*
 * Nothing to do here.
 */
static void vive_controller_lighthouse_stop(OuvrtDevice *dev)
{
	(void)dev;
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_controller_lighthouse_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_controller_lighthouse_parent_class)->finalize(object);
}

static void ouvrt_vive_controller_lighthouse_class_init(OuvrtViveControllerLighthouseClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_controller_lighthouse_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_controller_lighthouse_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_controller_lighthouse_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_controller_lighthouse_stop;
}

static void ouvrt_vive_controller_lighthouse_init(OuvrtViveControllerLighthouse *self)
{
	self->dev.type = DEVICE_TYPE_CONTROLLER;
	self->priv = ouvrt_vive_controller_lighthouse_get_instance_private(self);

	lighthouse_watchman_init(&self->priv->watchman);
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Controller Lighthouse Receiver device.
 */
OuvrtDevice *vive_controller_lighthouse_new(const char *devnode)
{
	OuvrtViveControllerLighthouse *vive;

	vive = g_object_new(OUVRT_TYPE_VIVE_CONTROLLER_LIGHTHOUSE, NULL);
	if (vive == NULL)
		return NULL;

	vive->dev.devnode = g_strdup(devnode);

	return &vive->dev;
}
