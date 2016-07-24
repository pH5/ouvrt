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
#include "vive-hid-reports.h"
#include "device.h"

enum pulse_mode {
	SYNC,
	SWEEP
};

struct lighthouse_pulse {
	uint32_t timestamp;
	uint16_t duration;
};

struct _OuvrtViveHeadsetLighthousePrivate {
	gboolean base_visible;

	enum pulse_mode mode;
	uint32_t seen_by;
	uint32_t last_timestamp;
	uint16_t duration;
	struct lighthouse_pulse last_sync;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveHeadsetLighthouse, ouvrt_vive_headset_lighthouse, \
			   OUVRT_TYPE_DEVICE)

struct lighthouse_sync_pulse {
	uint16_t duration;
	gboolean skip;
	gboolean rotor;
	gboolean data;
};

static struct lighthouse_sync_pulse pulse_table[8] = {
	{ 3000, 0, 0, 0 },
	{ 3500, 0, 1, 0 },
	{ 4000, 0, 0, 1 },
	{ 4500, 0, 1, 1 },
	{ 5000, 1, 0, 0 },
	{ 5500, 1, 1, 0 },
	{ 6000, 1, 0, 1 },
	{ 6500, 1, 1, 1 },
};

static void
vive_headset_lighthouse_handle_sync_pulse(OuvrtViveHeadsetLighthouse *self,
					  struct lighthouse_pulse *sync)
{
	OuvrtViveHeadsetLighthousePrivate *priv = self->priv;
	struct lighthouse_sync_pulse *pulse;
	int32_t dt;
	int i;

	if (!sync->duration)
		return;

	for (i = 0, pulse = pulse_table; i < 8; i++, pulse++) {
		if (sync->duration > (pulse->duration - 250) &&
		    sync->duration < (pulse->duration + 250)) {
			break;
		}
	}
	if (i == 8) {
		g_print("%s: Unknown pulse length: %d\n", self->dev.name,
			sync->duration);
		return;
	}

	dt = sync->timestamp - priv->last_timestamp;

	/* 48 MHz / 120 Hz = 400000 cycles per sync pulse */
	if (dt > (400000 - 4000) && dt < (400000 + 4000)) {
		/* Observing a single base station, channel A */
	} else if (dt > (380000 - 4000) && dt < (380000 + 4000)) {
		/* Observing two base stations, this is channel B */
	} else if (dt > (20000 - 4000) && dt < (20000 + 4000)) {
		/* Observing two base stations, this is channel C */
	} else {
		/* Irregular sync pulse */
		if (priv->last_timestamp)
			g_print("%s: Irregular sync pulse: %u -> %u (%+d)\n",
				self->dev.name, priv->last_timestamp,
				sync->timestamp, dt);
	}

	priv->last_timestamp = sync->timestamp;
}

static void
vive_headset_lighthouse_handle_pulse(OuvrtViveHeadsetLighthouse *self,
				     uint8_t id, uint16_t duration,
				     uint32_t timestamp)
{
	OuvrtViveHeadsetLighthousePrivate *priv = self->priv;
	int32_t dt;

	dt = timestamp - priv->last_sync.timestamp;

	if (priv->mode == SYNC && priv->seen_by && dt > priv->last_sync.duration) {
		vive_headset_lighthouse_handle_sync_pulse(self, &priv->last_sync);
		priv->seen_by = 0;
	}

	if (duration >= 2750) {
		if (dt > priv->last_sync.duration || priv->last_sync.duration == 0) {
			priv->seen_by = 1 << id;
			priv->last_sync.timestamp = timestamp;
			priv->last_sync.duration = duration;
		} else {
			priv->seen_by |= 1 << id;
			if (timestamp < priv->last_sync.timestamp) {
				priv->last_sync.duration += priv->last_sync.timestamp - timestamp;
				priv->last_sync.timestamp = timestamp;
			}
			if (duration > priv->last_sync.duration)
				priv->last_sync.duration = duration;
			priv->last_sync.duration = duration;
		}
		priv->mode = SYNC;
	} else {
		if (priv->mode == SYNC && dt <= priv->last_sync.duration)
			g_print("%s: error, expected sync\n", self->dev.name);
		priv->mode = SWEEP;
	}
}

/*
 * Decodes the periodic Lighthouse receiver message containing IR pulse
 * timing measurements.
 */
static void
vive_headset_lighthouse_decode_pulse_report1(OuvrtViveHeadsetLighthouse *self,
					     const void *buf)
{
	const struct vive_headset_lighthouse_pulse_report1 *report = buf;
	unsigned int i;

	/* The pulses may appear in arbitrary order */
	for (i = 0; i < 7; i++) {
		const struct vive_headset_lighthouse_pulse1 *pulse;
		uint16_t sensor_id;
		uint16_t duration;
		uint32_t timestamp;

		pulse = &report->pulse[i];

		sensor_id = __le16_to_cpu(pulse->id);
		if (sensor_id == 0xffff)
			continue;

		timestamp = __le32_to_cpu(pulse->timestamp);
		if (sensor_id == 0x00fe) {
			/* TODO: handle vsync timestamp */
			continue;
		}
		if (sensor_id == 0xfefe) {
			/* Unknown timestamp, ignore */
			continue;
		}

		if (sensor_id > 31) {
			g_print("%s: unhandled sensor id: %04x\n",
				self->dev.name, sensor_id);
			return;
		}

		duration = __le16_to_cpu(pulse->duration);

		vive_headset_lighthouse_handle_pulse(self, sensor_id, duration,
						     timestamp);
	}
}

static void
vive_headset_lighthouse_decode_pulse_report2(OuvrtViveHeadsetLighthouse *self,
					     const void *buf)
{
	const struct vive_headset_lighthouse_pulse_report2 *report = buf;
	unsigned int i;

	/* The pulses may appear in arbitrary order */
	for (i = 0; i < 9; i++) {
		const struct vive_headset_lighthouse_pulse2 *pulse;
		uint8_t sensor_id;
		uint16_t duration;
		uint32_t timestamp;

		pulse = &report->pulse[i];

		sensor_id = pulse->id;
		if (sensor_id == 0xff)
			continue;

		timestamp = __le32_to_cpu(pulse->timestamp);
		if (sensor_id == 0xfe) {
			/* TODO: handle vsync timestamp */
			continue;
		}

		if (sensor_id > 31) {
			g_print("%s: unhandled sensor id: %04x\n",
				self->dev.name, sensor_id);
			return;
		}

		duration = __le16_to_cpu(pulse->duration);

		vive_headset_lighthouse_handle_pulse(self, sensor_id, duration,
						     timestamp);
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
		if (ret == 58 &&
		    buf[0] == VIVE_HEADSET_LIGHTHOUSE_PULSE_REPORT1_ID) {
			vive_headset_lighthouse_decode_pulse_report1(self, buf);
		} else if (ret == 64 ||
			   buf[0] == VIVE_HEADSET_LIGHTHOUSE_PULSE_REPORT2_ID) {
			vive_headset_lighthouse_decode_pulse_report2(self, buf);
		} else {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
		}
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

	self->priv->mode = SWEEP;
	self->priv->seen_by = 0;
	self->priv->last_timestamp = 0;
	self->priv->last_sync.timestamp = 0;
	self->priv->last_sync.duration = 0;
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
