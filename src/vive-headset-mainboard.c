/*
 * HTC Vive Headset Mainboard
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "vive-headset-mainboard.h"
#include "vive-hid-reports.h"
#include "device.h"
#include "hidraw.h"

struct _OuvrtViveHeadsetMainboardPrivate {
	uint16_t ipd;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveHeadsetMainboard, \
			   ouvrt_vive_headset_mainboard, OUVRT_TYPE_DEVICE)

#define VIVE_HEADSET_BUTTON_SYSTEM	1

#define VIVE_HEADSET_PROXIMITY_DEC	1
#define VIVE_HEADSET_PROXIMITY_INC	2

/*
 * Decodes the periodic message containing button state, lens separation
 * and proximity measurement.
 */
static void
vive_headset_mainboard_decode_message(OuvrtViveHeadsetMainboard *self,
				      const unsigned char *buf,
				      size_t len)
{
	struct vive_mainboard_status_report *report = (void *)buf;
	uint16_t ipd;

	if (len != sizeof(*report)) {
		return;
	}

	if (__le16_to_cpu(report->unknown) != 0x2cd0 || report->len != 60 ||
	    report->reserved1 || report->reserved2[0]) {
		g_print("Unexpected message content: %02x %02x %02x %02x %02x %02x\n",
			buf[1], buf[2], buf[3], buf[6], buf[7], buf[9]);
	}

	ipd = __le16_to_cpu(report->ipd);
	if (ipd != self->priv->ipd) {
		self->priv->ipd = ipd;
		g_print("IPD %4.1f mm\n", 1e-2 * ipd);
	}
}

static int vive_headset_poweron(OuvrtViveHeadsetMainboard *self)
{
	const struct vive_headset_power_report report = {
		.id = VIVE_HEADSET_POWER_REPORT_ID,
		.type = __cpu_to_le16(VIVE_HEADSET_POWER_REPORT_TYPE),
		.len = 56,
		.unknown1 = {
			0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01,
		},
		.unknown2 = 0x7a,
	};

	return hid_send_feature_report(self->dev.fd, &report, sizeof(report));
}

static int vive_headset_poweroff(OuvrtViveHeadsetMainboard *self)
{
	const struct vive_headset_power_report report = {
		.id = VIVE_HEADSET_POWER_REPORT_ID,
		.type = __cpu_to_le16(VIVE_HEADSET_POWER_REPORT_TYPE),
		.len = 56,
		.unknown1 = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
		},
		.unknown2 = 0x7c,
	};

	return hid_send_feature_report(self->dev.fd, &report, sizeof(report));
}

static int vive_headset_get_device_info(OuvrtViveHeadsetMainboard *self)
{
	struct vive_headset_mainboard_device_info_report report = {
		.id = VIVE_HEADSET_MAINBOARD_DEVICE_INFO_REPORT_ID,
	};
	uint16_t edid_vid;
	uint16_t type;
	int ret;

	ret = hid_get_feature_report(self->dev.fd, &report, sizeof(report));
	if (ret < 0)
		return ret;

	type = __le16_to_cpu(report.type);
	if (type != VIVE_HEADSET_MAINBOARD_DEVICE_INFO_REPORT_TYPE ||
	    report.len != 60) {
		g_print("unexpected device info!\n");
		return -1;
	}

	edid_vid = __be16_to_cpu(report.edid_vid);

	g_print("%s: EDID Manufacturer ID: %c%c%c, Product code: 0x%04x\n"
		"%s: Display firmware version: %u\n", self->dev.name,
		'@' + (edid_vid >> 10),
		'@' + ((edid_vid >> 5) & 0x1f),
		'@' + (edid_vid & 0x1f), __le16_to_cpu(report.edid_pid),
		self->dev.name,
		__le32_to_cpu(report.display_firmware_version));

	return 0;
}

/*
 * Opens the Mainboard device, powers it on, and reads device info.
 */
static int vive_headset_mainboard_start(OuvrtDevice *dev)
{
	OuvrtViveHeadsetMainboard *self = OUVRT_VIVE_HEADSET_MAINBOARD(dev);
	int fd = self->dev.fd;
	int ret;

	if (fd == -1) {
		fd = open(self->dev.devnode, O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("%s: Failed to open '%s': %d\n", dev->name,
				dev->devnode, errno);
			return -1;
		}
		dev->fd = fd;
	}

	ret = vive_headset_poweron(self);
	if (ret < 0)
		g_print("%s: Failed to power on\n", dev->name);

	ret = vive_headset_get_device_info(self);
	if (ret < 0)
		g_print("%s: Failed to get device info\n", dev->name);

	return ret;
}

/*
 * Handles Mainboard messages.
 */
static void vive_headset_mainboard_thread(OuvrtDevice *dev)
{
	OuvrtViveHeadsetMainboard *self = OUVRT_VIVE_HEADSET_MAINBOARD(dev);
	unsigned char buf[64];
	struct pollfd fds;
	int count = 0;
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
			if (count++ > 3)
				g_print("%s: Poll timeout: %d\n", dev->name, count);
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
		if (ret != 64 || buf[0] != 0x03) {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
			continue;
		}

		vive_headset_mainboard_decode_message(self, buf, 64);
	}
}

/*
 * Powers off the mainboard device.
 */
static void vive_headset_mainboard_stop(OuvrtDevice *dev)
{
	OuvrtViveHeadsetMainboard *self = OUVRT_VIVE_HEADSET_MAINBOARD(dev);
	int ret;

	ret = vive_headset_poweroff(self);
	if (ret < 0)
		g_print("%s: Failed to power off\n", dev->name);
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_headset_mainboard_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_headset_mainboard_parent_class)->finalize(object);
}

static void
ouvrt_vive_headset_mainboard_class_init(OuvrtViveHeadsetMainboardClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_headset_mainboard_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_headset_mainboard_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_headset_mainboard_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_headset_mainboard_stop;
}

static void ouvrt_vive_headset_mainboard_init(OuvrtViveHeadsetMainboard *self)
{
	self->dev.type = DEVICE_TYPE_HMD;
	self->priv = ouvrt_vive_headset_mainboard_get_instance_private(self);
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Headset Mainboard device.
 */
OuvrtDevice *vive_headset_mainboard_new(const char *devnode)
{
	OuvrtViveHeadsetMainboard *vive;

	vive = g_object_new(OUVRT_TYPE_VIVE_HEADSET_MAINBOARD, NULL);
	if (vive == NULL)
		return NULL;

	vive->dev.devnode = g_strdup(devnode);

	return &vive->dev;
}
