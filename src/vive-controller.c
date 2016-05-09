/*
 * HTC Vive Controller
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

#include "vive-controller.h"
#include "device.h"
#include "hidraw.h"
#include "math.h"

struct _OuvrtViveControllerPrivate {
	gboolean connected;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveController, ouvrt_vive_controller, \
			   OUVRT_TYPE_DEVICE)

/*
 * Retrieves the controller firmware version
 */
static int vive_controller_get_firmware_version(OuvrtViveController *self)
{
	uint32_t firmware_version;
	unsigned char buf[64];
	int ret;

	buf[0] = 0x05;
	ret = hid_get_feature_report_timeout(self->dev.fd, buf, sizeof(buf), 100);
	if (ret < 0) {
		if (errno != EPIPE) {
			g_print("%s: Read error 0x05: %d\n", self->dev.name,
				errno);
		}
		return ret;
	}

	firmware_version = __le32_to_cpup((__le32 *)(buf + 1));

	g_print("%s: Controller firmware version %u %s@%s FPGA %u.%u\n",
		self->dev.name, firmware_version, buf + 9, buf + 25, buf[50],
		buf[49]);
	g_print("%s: Hardware revision: %d rev %d.%d.%d\n",
		self->dev.name, buf[44], buf[43], buf[42], buf[41]);

	return 0;
}

/*
 * Causes the Vive Controller do disconnect and power off.
 */
static int vive_controller_poweroff(OuvrtViveController *self)
{
	unsigned char buf[7] = {
		0xff, 0x9f, 0x04, 'o', 'f', 'f', '!',
	};

	return hid_send_feature_report(self->dev.fd, buf, sizeof(buf));
}

static void vive_controller_decode_squeeze_message(OuvrtViveController *self,
						   const unsigned char *buf)
{
	uint32_t time;
	uint8_t squeeze;

	(void)self;

	/* Time in 48 MHz ticks, missing the lower 16 bits */
	time = (buf[1] << 24) | (buf[3] << 16);
	squeeze = buf[5];
	/* buf[5-9] unknown */

	(void)time;
	(void)squeeze;
}

#define BUTTON_TRIGGER	0x01
#define BUTTON_TOUCH	0x02
#define BUTTON_THUMB	0x04
#define BUTTON_SYSTEM	0x08
#define BUTTON_GRIP	0x10
#define BUTTON_MENU	0x20

static void vive_controller_decode_button_message(OuvrtViveController *self,
						  const unsigned char *buf)
{
	uint8_t buttons;

	(void)self;

	buttons = buf[5];
	/* buf[5-9] unknown */

	(void) buttons;
}

static void vive_controller_decode_touch_move_message(OuvrtViveController *self,
						      const unsigned char *buf)
{
	int16_t pos[2];

	(void)self;

	pos[0] = __le16_to_cpup((__le16 *)(buf + 5));
	pos[1] = __le16_to_cpup((__le16 *)(buf + 7));
	/* buf[9-12] unknown */

	(void)pos;
}

static void
vive_controller_decode_touch_updown_message(OuvrtViveController *self,
					    const unsigned char *buf)
{
	uint8_t buttons;
	int16_t pos[2];

	(void)self;

	buttons = buf[5];
	pos[0] = __le16_to_cpup((__le16 *)(buf + 6));
	pos[1] = __le16_to_cpup((__le16 *)(buf + 8));
	/* buf[10-13] unknown */

	(void)buttons;
	(void)pos;
}

static void vive_controller_decode_imu_message(OuvrtViveController *self,
					       const unsigned char *buf)
{
	uint32_t time;
	int16_t acc[3];
	int16_t gyro[3];

	(void)self;

	/* Time in 48 MHz ticks, but we are missing the low byte */
	time = (buf[1] << 24) | (buf[3] << 16) | (buf[5] << 8);
	acc[0] = __le16_to_cpup((__le16 *)(buf + 6));
	acc[1] = __le16_to_cpup((__le16 *)(buf + 8));
	acc[2] = __le16_to_cpup((__le16 *)(buf + 10));
	gyro[0] = __le16_to_cpup((__le16 *)(buf + 12));
	gyro[1] = __le16_to_cpup((__le16 *)(buf + 14));
	gyro[2] = __le16_to_cpup((__le16 *)(buf + 16));
	/* buf[18-21] unknown */

	(void)time;
	(void)acc;
	(void)gyro;
}

static void vive_controller_decode_ping_message(OuvrtViveController *self,
						const unsigned char *buf)
{
	uint8_t charge;
	int16_t acc[3];
	int16_t gyro[3];

	(void)self;

	charge = buf[5];
	/* buf[6-7] unknown */
	acc[0] = __le16_to_cpup((__le16 *)(buf + 8));
	acc[1] = __le16_to_cpup((__le16 *)(buf + 10));
	acc[2] = __le16_to_cpup((__le16 *)(buf + 12));
	gyro[0] = __le16_to_cpup((__le16 *)(buf + 14));
	gyro[1] = __le16_to_cpup((__le16 *)(buf + 16));
	gyro[2] = __le16_to_cpup((__le16 *)(buf + 18));
	/* buf[20-24] unknown */

	(void)charge;
	(void)acc;
	(void)gyro;
}

/*
 * Decodes the periodic sensor message containing IMU sample(s) and
 * frame timing data.
 */
static void vive_controller_decode_message(OuvrtViveController *self,
					   const unsigned char *buf,
					   size_t len)
{
	uint32_t time;
	uint16_t type;

	(void)self;
	(void)len;

	time = (buf[1] << 16) | (buf[3] << 8);
	type = (buf[2] << 8) | buf[4];

	(void)time;

	switch (type) {
	case 0x03f4: /* analog trigger */
		vive_controller_decode_squeeze_message(self, buf);
		break;
	case 0x03f1: /* button */
	case 0x04f5: /* trigger switch */
		vive_controller_decode_button_message(self, buf);
		break;
	case 0x06f2: /* touchpad movement */
		vive_controller_decode_touch_move_message(self, buf);
		break;
	case 0x07f3: /* touchpad touchdown/liftoff */
		vive_controller_decode_touch_updown_message(self, buf);
		break;
	case 0x0fe8: /* IMU */
		vive_controller_decode_imu_message(self, buf);
		break;
	case 0x11e1: /* Ping */
		vive_controller_decode_ping_message(self, buf);
		break;
	}
}

/*
 * Opens the Wireless Receiver HID device descriptor.
 */
static int vive_controller_start(OuvrtDevice *dev)
{
	int fd = dev->fd;

	if (fd == -1) {
		fd = open(dev->devnode, O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("Vive Wireless Receiver %s: Failed to open '%s': %d\n",
				dev->serial, dev->devnode, errno);
			return -1;
		}
		dev->fd = fd;
	}

	return 0;
}

/*
 * Keeps the controller active.
 */
static void vive_controller_thread(OuvrtDevice *dev)
{
	OuvrtViveController *self = OUVRT_VIVE_CONTROLLER(dev);
	unsigned char buf[64];
	struct pollfd fds;
	int ret;

	ret = vive_controller_get_firmware_version(self);
	if (ret < 0 && errno == EPIPE) {
		g_print("Vive Wireless Receiver %s: No connected controller found\n",
			dev->serial);
	}
	if (!ret) {
		g_print("Vive Wireless Receiver %s: Controller connected\n",
			dev->serial);
		self->priv->connected = TRUE;
	}

	while (dev->active) {
		fds.fd = dev->fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		if (ret == -1) {
			g_print("Vive Wireless Receiver %s: Poll failure: %d\n",
				dev->serial, errno);
			continue;
		}

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL))
			break;

		if (!(fds.revents & POLLIN)) {
			if (self->priv->connected) {
				g_print("Vive Wireless Receiver %s: Poll timeout\n",
					dev->serial);
				continue;
			}
		}

		if (!self->priv->connected) {
			ret = vive_controller_get_firmware_version(self);
			if (ret < 0)
				continue;

			g_print("Vive Wireless Receiver %s: Controller connected\n",
				dev->serial);
			self->priv->connected = TRUE;
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("Vive Wireless Receiver %s: Read error: %d\n",
				dev->serial, errno);
			continue;
		}
		if (ret == 30 && buf[0] == 0x23) {
			vive_controller_decode_message(self, buf, 30);
		} else if (ret == 59 && buf[0] == 0x24) {
			vive_controller_decode_message(self, buf, 30);
			vive_controller_decode_message(self, buf + 29, 30);
		} else if (ret == 2 && buf[0] == 0x26 && buf[1] == 0x01) {
			g_print("Vive Wireless Receiver %s: Controller disconnected\n",
				dev->serial);
			self->priv->connected = FALSE;
		} else {
			g_print("Vive Wireless Receiver %s: Error, invalid %d-byte report 0x%02x\n",
				dev->serial, ret, buf[0]);
		}
	}
}

/*
 * Powers off the controller.
 */
static void vive_controller_stop(OuvrtDevice *dev)
{
	OuvrtViveController *self = OUVRT_VIVE_CONTROLLER(dev);

	vive_controller_poweroff(self);
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_controller_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_controller_parent_class)->finalize(object);
}

static void ouvrt_vive_controller_class_init(OuvrtViveControllerClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_controller_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_controller_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_controller_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_controller_stop;
}

static void ouvrt_vive_controller_init(OuvrtViveController *self)
{
	self->dev.type = DEVICE_TYPE_CONTROLLER;
	self->priv = ouvrt_vive_controller_get_instance_private(self);

	self->priv->connected = FALSE;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Controller device.
 */
OuvrtDevice *vive_controller_new(const char *devnode)
{
	OuvrtViveController *vive;

	vive = g_object_new(OUVRT_TYPE_VIVE_CONTROLLER, NULL);
	if (vive == NULL)
		return NULL;

	vive->dev.devnode = g_strdup(devnode);

	return &vive->dev;
}
