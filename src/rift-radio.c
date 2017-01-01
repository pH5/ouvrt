/*
 * Oculus Rift CV1 Radio
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "rift-hid-reports.h"
#include "rift-radio.h"
#include "device.h"
#include "hidraw.h"

struct _OuvrtRiftRadioPrivate {
	uint16_t remote_buttons;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtRiftRadio, ouvrt_rift_radio, OUVRT_TYPE_DEVICE)

static void rift_dump_message(const unsigned char *buf, size_t len)
{
	unsigned int i;

	for (i = 0; i < len; i++)
		g_print(" %02x", buf[i]);
	g_print("\n");
}

static void rift_decode_remote_message(OuvrtRiftRadio *rift,
				       const struct rift_radio_message *message)
{
	const struct rift_remote_message *remote = &message->remote;
	int16_t buttons = __le16_to_cpu(remote->buttons);

	if (rift->priv->remote_buttons != buttons)
		rift->priv->remote_buttons = buttons;
}

static void rift_decode_touch_message(OuvrtRiftRadio *rift,
				      const struct rift_radio_message *message)
{
	const struct rift_touch_message *touch = &message->touch;
	int16_t accel[3] = {
		__le16_to_cpu(touch->accel[0]),
		__le16_to_cpu(touch->accel[1]),
		__le16_to_cpu(touch->accel[2]),
	};
	int16_t gyro[3] = {
		__le16_to_cpu(touch->gyro[0]),
		__le16_to_cpu(touch->gyro[1]),
		__le16_to_cpu(touch->gyro[2]),
	};
	uint16_t trigger = touch->trigger_grip_stick[0] |
			   ((touch->trigger_grip_stick[1] & 0x03) << 8);
	uint16_t grip = ((touch->trigger_grip_stick[1] & 0xfc) >> 2) |
			((touch->trigger_grip_stick[2] & 0x0f) << 6);
	uint16_t stick[2] = {
		((touch->trigger_grip_stick[2] & 0xf0) >> 4) |
		((touch->trigger_grip_stick[3] & 0x3f) << 4),
		((touch->trigger_grip_stick[3] & 0xc0) >> 6) |
		((touch->trigger_grip_stick[4] & 0xff) << 2),
	};

	(void)rift;
	(void)accel;
	(void)gyro;
	(void)trigger;
	(void)grip;
	(void)stick;
}

static void rift_decode_radio_message(OuvrtRiftRadio *rift,
				      const unsigned char *buf,
				      size_t len)
{
	const struct rift_radio_message *message = (const void *)buf;

	if (message->device_type == RIFT_REMOTE) {
		rift_decode_remote_message(rift, message);
	} else if (message->device_type == RIFT_TOUCH_CONTROLLER_LEFT ||
		 message->device_type == RIFT_TOUCH_CONTROLLER_RIGHT) {
		rift_decode_touch_message(rift, message);
	} else {
		g_print("%s: unknown device %02x:", rift->dev.name,
			message->device_type);
		rift_dump_message(buf, len);
	}
}

static void rift_check_unknown_message(OuvrtRiftRadio *rift,
				      const unsigned char *buf,
				      size_t len)
{
	unsigned int i;

	for (i = 1; i < len && !buf[i]; i++);
	if (i != len) {
		g_print("%s: unknown message:", rift->dev.name);
		rift_dump_message(buf, len);
		return;
	}
}

static int rift_radio_start(OuvrtDevice *dev)
{
	OuvrtRiftRadio *rift = OUVRT_RIFT_RADIO(dev);
	int fd = rift->dev.fd;

	if (fd == -1) {
		fd = open(rift->dev.devnode, O_RDWR);
		if (fd == -1) {
			g_print("Rift: Failed to open '%s': %d\n",
				rift->dev.devnode, errno);
			return -1;
		}
		rift->dev.fd = fd;
	}

	return 0;
}

static void rift_radio_thread(OuvrtDevice *dev)
{
	OuvrtRiftRadio *rift = OUVRT_RIFT_RADIO(dev);
	unsigned char buf[64];
	struct timespec ts;
	struct pollfd fds;
	int ret;

	while (dev->active) {
		fds.fd = dev->fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		clock_gettime(CLOCK_MONOTONIC, &ts);
		if (ret == -1 || ret == 0)
			continue;

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL))
			break;

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("%s: Read error: %d\n", dev->name, errno);
			continue;
		}
		if (ret < 64) {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
			continue;
		}

		if (buf[0] == RIFT_RADIO_MESSAGE_ID)
			rift_decode_radio_message(rift, buf, sizeof(buf));
		if (buf[0] == RIFT_RADIO_UNKNOWN_MESSAGE_ID)
			rift_check_unknown_message(rift, buf, sizeof(buf));
	}
}

/*
 * Nothing to do here.
 */
static void rift_radio_stop(OuvrtDevice *dev)
{
	(void)dev;
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_rift_radio_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_rift_radio_parent_class)->finalize(object);
}

static void ouvrt_rift_radio_class_init(OuvrtRiftRadioClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_rift_radio_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = rift_radio_start;
	OUVRT_DEVICE_CLASS(klass)->thread = rift_radio_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = rift_radio_stop;
}

static void ouvrt_rift_radio_init(OuvrtRiftRadio *self)
{
	self->priv = ouvrt_rift_radio_get_instance_private(self);
}

/*
 * Allocates and initializes the device structure and opens the HID device
 * file descriptor.
 *
 * Returns the newly allocated Rift Radio device.
 */
OuvrtDevice *rift_cv1_radio_new(const char *devnode G_GNUC_UNUSED)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_RIFT_RADIO, NULL));
}
