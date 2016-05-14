/*
 * HTC Vive Headset Lighthouse Receiver
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <math.h>

#include "vive-headset-lighthouse.h"
#include "device.h"

struct _OuvrtViveHeadsetLighthousePrivate {
	gboolean base_visible;
	uint32_t last_timestamp;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveHeadsetLighthouse, ouvrt_vive_headset_lighthouse, \
			   OUVRT_TYPE_DEVICE)

/*
 * Decodes the periodic Lighthouse receiver message containing IR pulse
 * timing measurements.
 */
static void
vive_headset_lighthouse_decode_pulse_message(OuvrtViveHeadsetLighthouse *self,
					     const unsigned char *buf,
					     size_t len)
{
	int i;

	(void)len;

	for (i = 0; i < 7; i++) {
		const unsigned char *pulse = buf + 1 + i * 8;
		uint16_t sensor_id;
		uint16_t pulse_duration;
		uint32_t pulse_timestamp;

		sensor_id = __le16_to_cpup((__le16 *)pulse);
		if (sensor_id == 0xffff)
			continue;

		pulse_duration = __le16_to_cpup((__le16 *)(pulse + 2));
		pulse_timestamp = __le32_to_cpup((__le32 *)(pulse + 4));

		self->priv->last_timestamp = pulse_timestamp;

		(void)pulse_duration;
	}
}

/*
 * Opens the Lighthouse Receiver HID device.
 */
static int vive_headset_lighthouse_start(OuvrtDevice *dev)
{
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

	return 0;
}

/*
 * Handles Lighthouse Receiver messages.
 */
static void vive_headset_lighthouse_thread(OuvrtDevice *dev)
{
	OuvrtViveHeadsetLighthouse *self = OUVRT_VIVE_HEADSET_LIGHTHOUSE(dev);
	unsigned char buf[64];
	struct pollfd fds;
	int count;
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

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL))
			break;

		if (!(fds.revents & POLLIN)) {
			/* No Lighthouse base station visible */
			if (self->priv->base_visible) {
				g_print("%s: Lost base station visibility\n",
					dev->name);
				self->priv->base_visible = FALSE;
			}
			continue;
		}

		if (!self->priv->base_visible) {
			g_print("%s: Spotted a base station\n", dev->name);
			self->priv->base_visible = TRUE;
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("%s: Read error: %d\n", dev->name, errno);
			continue;
		}
		if (ret != 58) {
			g_print("%s: Error, invalid %d-byte report\n",
				dev->name, ret);
			continue;
		}

		vive_headset_lighthouse_decode_pulse_message(self, buf, 58);
		count++;
	}
}

/*
 * Nothing to do here.
 */
static void vive_headset_lighthouse_stop(OuvrtDevice *dev)
{
	(void)dev;
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_headset_lighthouse_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_headset_lighthouse_parent_class)->finalize(object);
}

static void ouvrt_vive_headset_lighthouse_class_init(OuvrtViveHeadsetLighthouseClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_headset_lighthouse_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_headset_lighthouse_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_headset_lighthouse_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_headset_lighthouse_stop;
}

static void ouvrt_vive_headset_lighthouse_init(OuvrtViveHeadsetLighthouse *self)
{
	self->dev.type = DEVICE_TYPE_HMD;
	self->priv = ouvrt_vive_headset_lighthouse_get_instance_private(self);
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Headset Lighthouse Receiver device.
 */
OuvrtDevice *vive_headset_lighthouse_new(const char *devnode)
{
	OuvrtViveHeadsetLighthouse *vive;

	vive = g_object_new(OUVRT_TYPE_VIVE_HEADSET_LIGHTHOUSE, NULL);
	if (vive == NULL)
		return NULL;

	vive->dev.devnode = g_strdup(devnode);

	return &vive->dev;
}
