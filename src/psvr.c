/*
 * Sony PlayStation VR Headset
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <unistd.h>

#include "psvr.h"
#include "psvr-hid-reports.h"
#include "device.h"
#include "hidraw.h"

struct _OuvrtPSVRPrivate {
	bool power;
	bool vrmode;
	uint8_t state;
	uint8_t last_seq;
	uint32_t last_timestamp;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtPSVR, ouvrt_psvr, OUVRT_TYPE_DEVICE)

void psvr_set_processing_box_power(int fd, bool power)
{
	struct psvr_processing_box_power_report report = {
		.id = PSVR_PROCESSING_BOX_POWER_REPORT_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 4,
		.payload = __cpu_to_le32(power ? PSVR_PROCESSING_BOX_POWER_ON :
						 PSVR_PROCESSING_BOX_POWER_OFF),
	};

	write(fd, &report, sizeof(report));
}

void psvr_set_headset_power(int fd, bool power)
{
	struct psvr_headset_power_report report = {
		.id = PSVR_HEADSET_POWER_REPORT_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 4,
		.payload = __cpu_to_le32(power ? PSVR_HEADSET_POWER_ON :
						 PSVR_HEADSET_POWER_OFF),
	};

	write(fd, &report, sizeof(report));
}

/*
 * Switches into VR mode enables the tracking LEDs.
 */
static void psvr_enable_vr_tracking(int fd)
{
	struct psvr_enable_vr_tracking_report report = {
		.id = PSVR_ENABLE_VR_TRACKING_REPORT_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 8,
		.payload = {
			__cpu_to_le32(PSVR_ENABLE_VR_TRACKING_DATA_1),
			__cpu_to_le32(PSVR_ENABLE_VR_TRACKING_DATA_2),
		},
	};

	write(fd, &report, sizeof(report));
	g_print("PSVR: Sent enable VR tracking report\n");
}

void psvr_set_mode(int fd, int mode)
{
	struct psvr_set_mode_report report = {
		.id = PSVR_SET_MODE_REPORT_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 4,
		.payload = __cpu_to_le32(mode ? PSVR_MODE_VR :
						PSVR_MODE_CINEMATIC),
	};

	write(fd, &report, sizeof(report));
}

void psvr_decode_sensor_message(OuvrtPSVR *self, const unsigned char *buf,
				size_t len G_GNUC_UNUSED)
{
	const struct psvr_sensor_message *message = (void *)buf;
	uint16_t volume = __le16_to_cpu(message->volume);
	uint32_t timestamp = __le32_to_cpu(message->sample[0].timestamp);
	int16_t gyro[3] = {
		__le16_to_cpu(message->sample[0].gyro[0]),
		__le16_to_cpu(message->sample[0].gyro[1]),
		__le16_to_cpu(message->sample[0].gyro[2])
	};
	int16_t accel[3] = {
		__le16_to_cpu(message->sample[0].accel[0]),
		__le16_to_cpu(message->sample[0].accel[1]),
		__le16_to_cpu(message->sample[0].accel[2])
	};
	uint16_t button_raw = __be16_to_cpu(message->button_raw);
	uint16_t proximity = __le16_to_cpu(message->proximity);

	if (message->state != self->priv->state) {
		self->priv->state = message->state;

		if (self->priv->state == PSVR_STATE_RUNNING &&
		    !self->priv->vrmode) {
			g_print("PSVR: Switch to VR mode\n");
			psvr_set_mode(self->dev.fds[1], PSVR_MODE_VR);
			psvr_enable_vr_tracking(self->dev.fds[1]);

			self->priv->vrmode = true;
		}
		if (self->priv->state != PSVR_STATE_RUNNING &&
		    self->priv->vrmode)
			self->priv->vrmode = false;
	}

	self->priv->last_timestamp = timestamp;
	self->priv->last_seq = message->sequence;

	(void)volume;
	(void)timestamp;
	(void)gyro;
	(void)accel;
	(void)button_raw;
	(void)proximity;
}

/*
 * Enables the headset.
 */
static int psvr_start(OuvrtDevice *dev)
{
	psvr_set_processing_box_power(dev->fds[1], true);
	psvr_set_headset_power(dev->fds[1], true);
	g_print("PSVR: Sent power on message\n");

	return 0;
}

/*
 * Handles sensor messages.
 */
static void psvr_thread(OuvrtDevice *dev)
{
	OuvrtPSVR *psvr = OUVRT_PSVR(dev);
	unsigned char buf[64];
	struct pollfd fds;
	int ret;

	while (dev->active) {
		fds.fd = dev->fds[0];
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		if (ret == -1) {
			g_print("PSVR: Poll failure\n");
			continue;
		}
		if (ret == 0) {
			if (psvr->priv->power) {
				if (psvr->priv->state == PSVR_STATE_POWER_OFF) {
					/*
					 * A poll timeout after powering off is
					 * expected.
					 */
					g_print("PSVR: Powered off\n");
				} else {
					g_print("PSVR: Poll timeout\n");
				}
				psvr->priv->power = false;
			}
			continue;
		}

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL))
			break;

		if (fds.revents & POLLIN) {
			ret = read(dev->fds[0], buf, sizeof(buf));
			if (ret == -1) {
				g_print("PSVR: Read error: %d\n", errno);
				continue;
			}
			if (ret != 64) {
				g_print("PSVR: Error, invalid %d-byte report\n",
					ret);
				continue;
			}

			if (!psvr->priv->power) {
				g_print("PSVR: Powered on\n");
				psvr->priv->power = true;
			}

			psvr_decode_sensor_message(psvr, buf, sizeof(buf));
		}
	}
}

/*
 * Powers off the headset and processing box.
 */
static void psvr_stop(OuvrtDevice *dev)
{
	psvr_set_headset_power(dev->fds[1], false);
	psvr_set_processing_box_power(dev->fds[1], false);
	g_print("PSVR: Sent power off message\n");
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_psvr_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_psvr_parent_class)->finalize(object);
}

static void ouvrt_psvr_class_init(OuvrtPSVRClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_psvr_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = psvr_start;
	OUVRT_DEVICE_CLASS(klass)->thread = psvr_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = psvr_stop;
}

static void ouvrt_psvr_init(OuvrtPSVR *self)
{
	self->dev.type = DEVICE_TYPE_HMD;
	self->priv = ouvrt_psvr_get_instance_private(self);
	self->priv->power = false;
	self->priv->vrmode = false;
	self->priv->state = PSVR_STATE_POWER_OFF;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated PlayStation VR device.
 */
OuvrtDevice *psvr_new(const char *devnode G_GNUC_UNUSED)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_PSVR, NULL));
}
