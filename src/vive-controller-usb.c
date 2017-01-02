/*
 * HTC Vive Controller (via USB)
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <json-glib/json-glib.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "vive-controller-usb.h"
#include "vive-config.h"
#include "vive-firmware.h"
#include "vive-hid-reports.h"
#include "vive-imu.h"
#include "device.h"
#include "hidraw.h"
#include "imu.h"
#include "json.h"
#include "lighthouse.h"
#include "math.h"
#include "tracking-model.h"
#include "usb-ids.h"

struct _OuvrtViveControllerUSBPrivate {
	JsonNode *config;
	struct vive_imu imu;
	struct tracking_model model;
	struct lighthouse_watchman watchman;
	uint32_t buttons;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveControllerUSB, ouvrt_vive_controller_usb, \
			   OUVRT_TYPE_DEVICE)

/*
 * Downloads the configuration data stored in the controller
 */
static int vive_controller_usb_get_config(OuvrtViveControllerUSB *self)
{
	char *config_json;
	JsonObject *object;
	struct vive_imu *imu = &self->priv->imu;
	const char *device_class;
	gint64 device_pid, device_vid;
	const char *serial;

	config_json = ouvrt_vive_get_config(&self->dev);
	if (!config_json)
		return -1;

	self->priv->config = json_from_string(config_json, NULL);
	g_free(config_json);
	if (!self->priv->config) {
		g_print("%s: Parsing JSON configuration data failed\n",
			self->dev.name);
		return -1;
	}

	object = json_node_get_object(self->priv->config);

	json_object_get_vec3_member(object, "acc_bias", &imu->acc_bias);
	json_object_get_vec3_member(object, "acc_scale", &imu->acc_scale);

	device_class = json_object_get_string_member(object, "device_class");
	if (strcmp(device_class, "controller") != 0) {
		g_print("%s: Unknown device class \"%s\"\n", self->dev.name,
			device_class);
	}

	device_pid = json_object_get_int_member(object, "device_pid");
	if (device_pid != PID_VIVE_CONTROLLER_USB) {
		g_print("%s: Unknown device PID: 0x%04lx\n", self->dev.name,
			device_pid);
	}

	serial = json_object_get_string_member(object, "device_serial_number");
	if (strcmp(serial, self->dev.serial) != 0)
		g_print("%s: Configuration serial number differs: %s\n",
			self->dev.name, serial);

	device_vid = json_object_get_int_member(object, "device_vid");
	if (device_vid != VID_VALVE) {
		g_print("%s: Unknown device VID: 0x%04lx\n", self->dev.name,
			device_vid);
	}

	json_object_get_vec3_member(object, "gyro_bias", &imu->gyro_bias);
	json_object_get_vec3_member(object, "gyro_scale", &imu->gyro_scale);

	json_object_get_lighthouse_config_member(object, "lighthouse_config",
						 &self->priv->model);
	if (!self->priv->model.num_points) {
		g_print("%s: Failed to parse Lighthouse configuration\n",
			self->dev.name);
	}

	return 0;
}

/*
 * Decodes the periodic Lighthouse receiver message containing IR pulse
 * timing measurements.
 */
static void vive_controller_decode_pulse_report(OuvrtViveControllerUSB *self,
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

void vive_controller_decode_button_message(OuvrtViveControllerUSB *self,
					   const unsigned char *buf, size_t len)
{
	struct vive_controller_button_report *report = (void *)buf;
	uint32_t buttons = report->buttons;

	(void)len;

	if (buttons != self->priv->buttons)
		self->priv->buttons = buttons;
}

/*
 * Opens the Vive Controller USB HID device.
 */
static int vive_controller_usb_start(OuvrtDevice *dev)
{
	OuvrtViveControllerUSB *self = OUVRT_VIVE_CONTROLLER_USB(dev);
	int fd;
	int ret;

	g_free(dev->name);
	dev->name = g_strdup_printf("Vive Controller %s USB", dev->serial);

	if (dev->fds[0] == -1) {
		fd = open(dev->devnodes[0], O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("%s: Failed to open '%s': %d\n", dev->name,
				dev->devnode, errno);
			return -1;
		}
		dev->fds[0] = fd;
	}
	if (dev->fds[1] == -1) {
		fd = open(dev->devnodes[1], O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("%s: Failed to open '%s': %d\n", dev->name,
				dev->devnode, errno);
			return -1;
		}
		dev->fds[1] = fd;
	}
	if (dev->fds[2] == -1) {
		fd = open(dev->devnodes[2], O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("%s: Failed to open '%s': %d\n", dev->name,
				dev->devnode, errno);
			return -1;
		}
		dev->fds[2] = fd;
	}

	self->priv->watchman.name = dev->name;

	ret = vive_get_firmware_version(dev);
	if (ret < 0 && errno == EPIPE) {
		g_print("%s: Failed to get firmware version\n", dev->name);
		return ret;
	}
	ret = vive_controller_usb_get_config(self);

	return 0;
}

/*
 * Handles USB HID messages from IMU, Lighouse Receiver, and Button interfaces.
 */
static void vive_controller_usb_thread(OuvrtDevice *dev)
{
	OuvrtViveControllerUSB *self = OUVRT_VIVE_CONTROLLER_USB(dev);
	unsigned char buf[64];
	struct pollfd fds[3];
	int ret;

	while (dev->active) {
		fds[0].fd = dev->fds[0];
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		fds[1].fd = dev->fds[1];
		fds[1].events = POLLIN;
		fds[1].revents = 0;
		fds[2].fd = dev->fds[2];
		fds[2].events = POLLIN;
		fds[2].revents = 0;

		ret = poll(fds, 3, 1000);
		if (ret == -1) {
			g_print("%s: Poll failure: %d\n", dev->name, errno);
			continue;
		}

		if (ret == 0) {
			g_print("%s: Poll timeout\n", dev->name);
			continue;
		}

		if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) ||
		    (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))) {
			g_print("%s: Disconnected\n", dev->name);
			dev->active = FALSE;
			break;
		}

		if (self->priv->imu.gyro_range == 0.0) {
			ret = vive_imu_get_range_modes(dev, &self->priv->imu);
			if (ret < 0) {
				g_print("%s: Failed to get gyro/accelerometer range modes\n",
					dev->name);
				continue;
			}
		}

		if (fds[0].revents & POLLIN) {
			ret = read(dev->fds[0], buf, sizeof(buf));
			if (ret == -1) {
				g_print("%s: Read error: %d\n", dev->name, errno);
				continue;
			}
			if (ret == 52 && buf[0] == VIVE_IMU_REPORT_ID) {
				vive_imu_decode_message(&self->priv->imu, buf, ret);
			} else {
				g_print("%s: Error, invalid %d-byte report 0x%02x\n",
					dev->name, ret, buf[0]);
			}
		}
		if (fds[1].revents & POLLIN) {
			ret = read(dev->fds[1], buf, sizeof(buf));
			if (ret == -1) {
				g_print("%s: Read error: %d\n", dev->name, errno);
				continue;
			}
			if (ret == 58 &&
			    buf[0] == VIVE_CONTROLLER_LIGHTHOUSE_PULSE_REPORT_ID) {
				vive_controller_decode_pulse_report(self, buf);
			} else {
				g_print("%s: Error, invalid %d-byte report 0x%02x\n",
					dev->name, ret, buf[0]);
			}
		}
		if (fds[2].revents & POLLIN) {
			ret = read(dev->fds[2], buf, sizeof(buf));
			if (ret == -1) {
				g_print("%s: Read error: %d\n", dev->name, errno);
				continue;
			}
			if (ret == 64 &&
			    buf[0] == VIVE_CONTROLLER_BUTTON_REPORT_ID) {
				vive_controller_decode_button_message(self, buf, ret);
			} else {
				g_print("%s: Error, invalid %d-byte report 0x%02x\n",
					dev->name, ret, buf[0]);
			}
		}
	}
}

/*
 * Nothing to do here.
 */
static void vive_controller_usb_stop(OuvrtDevice *dev)
{
	(void)dev;
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_controller_usb_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_controller_usb_parent_class)->finalize(object);
}

static void ouvrt_vive_controller_usb_class_init(OuvrtViveControllerUSBClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_controller_usb_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_controller_usb_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_controller_usb_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_controller_usb_stop;
}

static void ouvrt_vive_controller_usb_init(OuvrtViveControllerUSB *self)
{
	self->dev.type = DEVICE_TYPE_CONTROLLER;
	self->priv = ouvrt_vive_controller_usb_get_instance_private(self);

	self->priv->config = NULL;
	self->priv->imu.sequence = 0;
	self->priv->imu.time = 0;
	lighthouse_watchman_init(&self->priv->watchman);
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Controller device.
 */
OuvrtDevice *vive_controller_usb_new(const char *devnode G_GNUC_UNUSED)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_VIVE_CONTROLLER_USB, NULL));
}
