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
#include <zlib.h>

#include "vive-headset-lighthouse.h"
#include "vive-hid-reports.h"
#include "device.h"
#include "math.h"

enum pulse_mode {
	SYNC,
	SWEEP
};

struct lighthouse_rotor_calibration {
	float tilt;
	float phase;
	float curve;
	float gibphase;
	float gibmag;
};

struct lighthouse_base_calibration {
	struct lighthouse_rotor_calibration rotor[2];
};

struct lighthouse_base {
	int data_sync;
	int data_word;
	int data_bit;
	uint8_t ootx[40];

	int firmware_version;
	uint32_t serial;
	struct lighthouse_base_calibration calibration;
	vec3 gravity;
	char channel;
	int model_id;
	int reset_count;
};

struct lighthouse_pulse {
	uint32_t timestamp;
	uint16_t duration;
};

struct lighthouse_sensor {
	struct lighthouse_pulse sync;
	struct lighthouse_pulse sweep;
};

struct _OuvrtViveHeadsetLighthousePrivate {
	gboolean base_visible;
	struct lighthouse_base base[2];

	enum pulse_mode mode;
	uint32_t seen_by;
	uint32_t last_timestamp;
	uint16_t duration;
	struct lighthouse_sensor sensor[32];
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

struct lighthouse_ootx_report {
	__le16 version;
	__le32 serial;
	__le16 phase[2];
	__le16 tilt[2];
	__u8 reset_count;
	__u8 model_id;
	__le16 curve[2];
	__s8 gravity[3];
	__le16 gibphase[2];
	__le16 gibmag[2];
} __attribute__((packed));

static inline float __le16_to_float(__le16 le16)
{
	return f16_to_float(__le16_to_cpu(le16));
}

static void lighthouse_base_handle_ootx_frame(struct lighthouse_base *base)
{
	struct lighthouse_ootx_report *report = (void *)(base->ootx + 2);
	uint16_t len = __le16_to_cpup((__le16 *)base->ootx);
	uint32_t crc = crc32(0L, Z_NULL, 0);
	gboolean serial_changed = FALSE;
	uint32_t ootx_crc;
	uint16_t version;
	int ootx_version;
	vec3 gravity;
	int i;

	if (len != 33) {
		g_print("Lighthouse Base %X: unexpected OOTX payload length: %d\n",
			base->serial, len);
		return;
	}

	ootx_crc = __le32_to_cpup((__le32 *)(base->ootx + 36)); /* (len+3)/4*4 */
	crc = crc32(crc, base->ootx + 2, 33);
	if (ootx_crc != crc) {
		g_print("Lighthouse Base %X: CRC error: %08x != %08x\n",
			base->serial, crc, ootx_crc);
		return;
	}

	version = __le16_to_cpu(report->version);
	ootx_version = version & 0x3f;
	if (ootx_version != 6) {
		g_print("Lighthouse Base %X: unexpected OOTX frame version: %d\n",
			base->serial, ootx_version);
		return;
	}

	base->firmware_version = version >> 6;

	if (base->serial != __le32_to_cpu(report->serial)) {
		base->serial = __le32_to_cpu(report->serial);
		serial_changed = TRUE;
	}

	for (i = 0; i < 2; i++) {
		struct lighthouse_rotor_calibration *rotor;

		rotor = &base->calibration.rotor[i];
		rotor->tilt = __le16_to_float(report->tilt[i]);
		rotor->phase = __le16_to_float(report->phase[i]);
		rotor->curve = __le16_to_float(report->curve[i]);
		rotor->gibphase = __le16_to_float(report->gibphase[i]);
		rotor->gibmag = __le16_to_float(report->gibmag[i]);
	}

	base->model_id = report->model_id;

	if (serial_changed) {
		g_print("Lighthouse Base %X: firmware version: %d, model id: %d, channel: %c\n",
			base->serial, base->firmware_version, base->model_id,
			base->channel);

		for (i = 0; i < 2; i++) {
			struct lighthouse_rotor_calibration *rotor;

			rotor = &base->calibration.rotor[i];

			g_print("Lighthouse Base %X: rotor %d: [ %12.9f %12.9f %12.9f %12.9f %12.9f ]\n",
				base->serial, i, rotor->tilt, rotor->phase,
				rotor->curve, rotor->gibphase, rotor->gibmag);
		}
	}

	gravity.x = report->gravity[0];
	gravity.y = report->gravity[1];
	gravity.z = report->gravity[2];
	vec3_normalize(&gravity);
	if (gravity.x != base->gravity.x ||
	    gravity.y != base->gravity.y ||
	    gravity.z != base->gravity.z) {
		base->gravity = gravity;
		g_print("Lighthouse Base %X: gravity: [ %9.6f %9.6f %9.6f ]\n",
			base->serial, gravity.x, gravity.y, gravity.z);
	}

	if (base->reset_count != report->reset_count) {
		base->reset_count = report->reset_count;
		g_print("Lighthouse Base %X: reset count: %d\n", base->serial,
			base->reset_count);
	}
}

static void lighthouse_base_reset(struct lighthouse_base *base)
{
	base->data_sync = 0;
	base->data_word = -1;
	base->data_bit = 0;
	memset(base->ootx, 0, sizeof(base->ootx));
}

static void
lighthouse_base_handle_ootx_data_word(OuvrtViveHeadsetLighthouse *self,
				      struct lighthouse_base *base)
{
	uint16_t len = __le16_to_cpup((__le16 *)base->ootx);

	/* After 4 OOTX words we have received the base station serial number */
	if (base->data_word == 4) {
		struct lighthouse_ootx_report *report = (void *)(base->ootx + 2);
		uint16_t ootx_version = __le16_to_cpu(report->version) & 0x3f;
		uint32_t serial = __le32_to_cpu(report->serial);

		if (len != 33) {
			g_print("%s: unexpected OOTX frame length %d\n",
				self->dev.name, len);
			return;
		}

		if (ootx_version == 6 && serial != base->serial) {
			g_print("%s: spotted Lighthouse Base %X\n",
				self->dev.name, serial);
		}
	}
	if (len == 33 && base->data_word == 20) { /* (len + 3)/4 * 2 + 2 */
		lighthouse_base_handle_ootx_frame(base);
	}
}

static void
lighthouse_base_handle_sync_pulse(OuvrtViveHeadsetLighthouse *self,
				  struct lighthouse_sync_pulse *pulse,
				  char channel)
{
	struct lighthouse_base *base = &self->priv->base[channel == 'C'];

	base->channel = channel;

	if (base->data_word >= (int)sizeof(base->ootx) / 2)
		base->data_word = -1;
	if (base->data_word >= 0) {
		if (base->data_bit == 16) {
			/* Sync bit */
			base->data_bit = 0;
			if (pulse->data) {
				base->data_word++;
				lighthouse_base_handle_ootx_data_word(self,
								      base);
			} else {
				g_print("%s: Missed a sync bit, restarting\n",
					self->dev.name);
				/* Missing sync bit, restart */
				base->data_word = -1;
			}
		} else if (base->data_bit < 8) {
			base->ootx[2 * base->data_word] |=
					pulse->data << (7 - base->data_bit);
			base->data_bit++;
		} else if (base->data_bit < 16) {
			base->ootx[2 * base->data_word + 1] |=
					pulse->data << (15 - base->data_bit);
			base->data_bit++;
		}
	}

	/* Preamble detection */
	if (base->data_sync > 16 && pulse->data == 1) {
		memset(base->ootx, 0, sizeof(base->ootx));
		base->data_word = 0;
		base->data_bit = 0;
	}
	if (pulse->data)
		base->data_sync = 0;
	else
		base->data_sync++;
}

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
		lighthouse_base_handle_sync_pulse(self, pulse, 'A');
	} else if (dt > (380000 - 4000) && dt < (380000 + 4000)) {
		/* Observing two base stations, this is channel B */
		lighthouse_base_handle_sync_pulse(self, pulse, 'B');
	} else if (dt > (20000 - 4000) && dt < (20000 + 4000)) {
		/* Observing two base stations, this is channel C */
		lighthouse_base_handle_sync_pulse(self, pulse, 'C');
	} else {
		/* Irregular sync pulse */
		if (priv->last_timestamp)
			g_print("%s: Irregular sync pulse: %u -> %u (%+d)\n",
				self->dev.name, priv->last_timestamp,
				sync->timestamp, dt);
		lighthouse_base_reset(&priv->base[0]);
		lighthouse_base_reset(&priv->base[1]);
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
