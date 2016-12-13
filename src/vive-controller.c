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
#include <json-glib/json-glib.h>

#include "vive-controller.h"
#include "vive-config.h"
#include "vive-hid-reports.h"
#include "device.h"
#include "hidraw.h"
#include "math.h"

struct _OuvrtViveControllerPrivate {
	JsonNode *config;
	const gchar *serial;
	gboolean connected;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveController, ouvrt_vive_controller, \
			   OUVRT_TYPE_DEVICE)

/*
 * Retrieves the controller firmware version
 */
static int vive_controller_get_firmware_version(OuvrtViveController *self)
{
	struct vive_firmware_version_report report = {
		.id = VIVE_FIRMWARE_VERSION_REPORT_ID,
	};
	uint32_t firmware_version;
	int ret;

	ret = hid_get_feature_report_timeout(self->dev.fd, &report,
					     sizeof(report), 100);
	if (ret < 0) {
		if (errno != EPIPE) {
			g_print("%s: Read error 0x05: %d\n", self->dev.name,
				errno);
		}
		return ret;
	}

	firmware_version = __le32_to_cpu(report.firmware_version);

	g_print("%s: Controller firmware version %u %s@%s FPGA %u.%u\n",
		self->dev.name, firmware_version, report.string1,
		report.string2, report.fpga_version_major,
		report.fpga_version_minor);
	g_print("%s: Hardware revision: %d rev %d.%d.%d\n",
		self->dev.name, report.hardware_revision,
		report.hardware_version_major, report.hardware_version_minor,
		report.hardware_version_micro);

	return 0;
}

/*
 * Downloads the configuration data stored in the controller
 */
static int vive_controller_get_config(OuvrtViveController *self)
{
	char *config_json;
	JsonObject *object;

	config_json = ouvrt_vive_get_config(&self->dev);
	if (!config_json)
		return -1;

	self->priv->config = json_from_string(config_json, NULL);
	g_free(config_json);
	if (!self->priv->config) {
		g_print("Vive Wireless Receiver %s: Parsing JSON configuration data failed\n",
			self->dev.serial);
		return -1;
	}

	object = json_node_get_object(self->priv->config);

	self->priv->serial = json_object_get_string_member(object,
							   "device_serial_number");

	return 0;
}

static int vive_controller_poweroff(OuvrtViveController *self)
{
	const struct vive_controller_poweroff_report report = {
		.id = VIVE_CONTROLLER_COMMAND_REPORT_ID,
		.command = VIVE_CONTROLLER_POWEROFF_COMMAND,
		.len = 4,
		.magic = { 'o', 'f', 'f', '!' },
	};

	return hid_send_feature_report(self->dev.fd, &report, sizeof(report));
}

static void
vive_controller_decode_squeeze_message(OuvrtViveController *self,
				       const struct vive_controller_message *msg)
{
	/* Time in 48 MHz ticks, missing the lower 16 bits */
	uint32_t time = (msg->timestamp_hi << 24) |
			(msg->timestamp_lo << 16);

	(void)self;
	(void)time;
}

static void
vive_controller_decode_button_message(OuvrtViveController *self,
				      const struct vive_controller_message *msg)
{
	uint8_t buttons = msg->button.buttons;

	(void)self;
	(void)buttons;
}

static void
vive_controller_decode_touch_move_message(OuvrtViveController *self,
					  const struct vive_controller_message *msg)
{
	int16_t pos[2] = {
		__le16_to_cpu(msg->touchpad_move.pos[0]),
		__le16_to_cpu(msg->touchpad_move.pos[1]),
	};

	(void)self;
	(void)pos;
}

static void
vive_controller_decode_touch_updown_message(OuvrtViveController *self,
					    const struct vive_controller_message *msg)
{
	int16_t pos[2] = {
		__le16_to_cpu(msg->touchpad_updown.pos[0]),
		__le16_to_cpu(msg->touchpad_updown.pos[1]),
	};

	(void)self;
	(void)pos;
}

static void
vive_controller_decode_imu_message(OuvrtViveController *self,
				   const struct vive_controller_message *msg)
{
	/* Time in 48 MHz ticks, but we are missing the low byte */
	uint32_t time = (msg->timestamp_hi << 24) | (msg->timestamp_lo << 16) |
			(msg->imu.timestamp_3 << 8);
	int16_t acc[3] = {
		__le16_to_cpu(msg->imu.accel[0]),
		__le16_to_cpu(msg->imu.accel[1]),
		__le16_to_cpu(msg->imu.accel[2]),
	};
	int16_t gyro[3] = {
		__le16_to_cpu(msg->imu.gyro[0]),
		__le16_to_cpu(msg->imu.gyro[1]),
		__le16_to_cpu(msg->imu.gyro[2]),
	};

	(void)self;
	(void)time;
	(void)acc;
	(void)gyro;
}

static void
vive_controller_decode_ping_message(OuvrtViveController *self,
				    const struct vive_controller_message *msg)
{
	uint8_t charge_percent = msg->ping.charge & 0x7f;
	gboolean charging = msg->ping.charge & 0x80;
	int16_t acc[3] = {
		__le16_to_cpu(msg->ping.accel[0]),
		__le16_to_cpu(msg->ping.accel[1]),
		__le16_to_cpu(msg->ping.accel[2]),
	};
	int16_t gyro[3] = {
		__le16_to_cpu(msg->ping.gyro[0]),
		__le16_to_cpu(msg->ping.gyro[1]),
		__le16_to_cpu(msg->ping.gyro[2]),
	};

	(void)self;
	(void)charge_percent;
	(void)charging;
	(void)acc;
	(void)gyro;
}

/*
 * Decodes multiplexed Wireless Receiver messages.
 */
static void
vive_controller_decode_message(OuvrtViveController *self,
			       struct vive_controller_message *message)
{
	uint16_t type = (message->type_hi << 8) | message->type_lo;

	switch (type) {
	case 0x03f4: /* analog trigger */
		vive_controller_decode_squeeze_message(self, message);
		break;
	case 0x03f1: /* button */
	case 0x04f5: /* trigger switch */
		vive_controller_decode_button_message(self, message);
		break;
	case 0x06f2: /* touchpad movement */
		vive_controller_decode_touch_move_message(self, message);
		break;
	case 0x07f3: /* touchpad touchdown/liftoff */
		vive_controller_decode_touch_updown_message(self, message);
		break;
	case 0x0fe8: /* IMU */
		vive_controller_decode_imu_message(self, message);
		break;
	case 0x11e1: /* Ping */
		vive_controller_decode_ping_message(self, message);
		break;
	}
}

/*
 * Opens the Wireless Receiver HID device descriptor.
 */
static int vive_controller_start(OuvrtDevice *dev)
{
	OuvrtViveController *self = OUVRT_VIVE_CONTROLLER(dev);
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

	self->priv->watchman.name = self->dev.name;

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
		ret = vive_controller_get_config(self);
		if (!ret) {
			g_print("Vive Wireless Receiver %s: Controller %s connected\n",
				dev->serial, self->priv->serial);
			self->priv->connected = TRUE;
		}
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

			ret = vive_controller_get_config(self);
			if (ret < 0)
				continue;

			g_print("Vive Wireless Receiver %s: Controller %s connected\n",
				dev->serial, self->priv->serial);
			self->priv->connected = TRUE;
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("Vive Controller %s: Read error: %d\n",
				self->priv->serial, errno);
			continue;
		}
		if (ret == 30 && buf[0] == VIVE_CONTROLLER_REPORT1_ID) {
			struct vive_controller_report1 *report = (void *)buf;

			vive_controller_decode_message(self, &report->message);
		} else if (ret == 59 && buf[0] == VIVE_CONTROLLER_REPORT2_ID) {
			struct vive_controller_report2 *report = (void *)buf;

			vive_controller_decode_message(self,
						       &report->message[0]);
			vive_controller_decode_message(self,
						       &report->message[1]);
		} else if (ret == 2 &&
			   buf[0] == VIVE_CONTROLLER_DISCONNECT_REPORT_ID &&
			   buf[1] == 0x01) {
			g_print("Vive Wireless Receiver %s: Controller %s disconnected\n",
				dev->serial, self->priv->serial);
			self->priv->connected = FALSE;
		} else {
			g_print("Vive Controller %s: Error, invalid %d-byte report 0x%02x\n",
				self->priv->serial, ret, buf[0]);
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

	self->priv->config = NULL;
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
