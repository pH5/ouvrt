/*
 * HTC Vive Controller IMU
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

#include "vive-controller-imu.h"
#include "vive-config.h"
#include "vive-firmware.h"
#include "vive-hid-reports.h"
#include "vive-imu.h"
#include "device.h"
#include "hidraw.h"
#include "imu.h"
#include "json.h"
#include "math.h"
#include "tracking-model.h"

struct _OuvrtViveControllerIMUPrivate {
	JsonNode *config;
	const gchar *serial;
	struct vive_imu imu;
	vec3 acc_bias;
	vec3 acc_scale;
	vec3 gyro_bias;
	vec3 gyro_scale;
	struct tracking_model model;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveControllerIMU, ouvrt_vive_controller_imu, \
			   OUVRT_TYPE_DEVICE)

#define VID_VALVE		0x28de
#define PID_VIVE_CONTROLLER_USB	0x2012

/*
 * Downloads the configuration data stored in the controller
 */
static int vive_controller_imu_get_config(OuvrtViveControllerIMU *self)
{
	char *config_json;
	JsonObject *object;
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

	json_object_get_vec3_member(object, "acc_bias", &self->priv->acc_bias);
	json_object_get_vec3_member(object, "acc_scale", &self->priv->acc_scale);

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

	json_object_get_vec3_member(object, "gyro_bias", &self->priv->gyro_bias);
	json_object_get_vec3_member(object, "gyro_scale", &self->priv->gyro_scale);

	json_object_get_lighthouse_config_member(object, "lighthouse_config",
						 &self->priv->model);
	if (!self->priv->model.num_points) {
		g_print("%s: Failed to parse Lighthouse configuration\n",
			self->dev.name);
	}

	g_debug("%f,%f,%f\t%f,%f,%f\t%f,%f,%f\t%f,%f,%f\n",
		self->priv->acc_bias.x, self->priv->acc_bias.y, self->priv->acc_bias.z,
		self->priv->acc_scale.x, self->priv->acc_scale.y, self->priv->acc_scale.z,
		self->priv->gyro_bias.x, self->priv->gyro_bias.y, self->priv->gyro_bias.z,
		self->priv->gyro_scale.x, self->priv->gyro_scale.y, self->priv->gyro_scale.z);

	return 0;
}

/*
 * Opens the Vive Controller IMU HID device.
 */
static int vive_controller_imu_start(OuvrtDevice *dev)
{
	OuvrtViveControllerIMU *self = OUVRT_VIVE_CONTROLLER_IMU(dev);
	int fd = dev->fd;
	int ret;

	g_free(dev->name);
	dev->name = g_strdup_printf("Vive Controller %s IMU", dev->serial);

	if (fd == -1) {
		fd = open(dev->devnode, O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("%s: Failed to open '%s': %d\n", dev->name,
				dev->devnode, errno);
			return -1;
		}
		dev->fd = fd;
	}

	ret = vive_get_firmware_version(dev);
	if (ret < 0 && errno == EPIPE) {
		g_print("%s: Failed to get firmware version\n", dev->name);
		return ret;
	}
	ret = vive_controller_imu_get_config(self);

	return 0;
}

/*
 * Handles IMU messages.
 */
static void vive_controller_imu_thread(OuvrtDevice *dev)
{
	OuvrtViveControllerIMU *self = OUVRT_VIVE_CONTROLLER_IMU(dev);
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
			g_print("%s: Poll timeout\n", dev->name);
			continue;
		}

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			g_print("%s: Disconnected\n", dev->name);
			dev->active = FALSE;
			break;
		}

		if (!(fds.revents & POLLIN)) {
			g_print("%s: Unhandled poll event: 0x%x\n", dev->name,
				fds.revents);
			continue;
		}

		if (self->priv->imu.gyro_range == 0.0) {
			ret = vive_imu_get_range_modes(dev, &self->priv->imu);
			if (ret < 0) {
				g_print("%s: Failed to get gyro/accelerometer range modes\n",
					dev->name);
				continue;
			}
		}

		ret = read(dev->fd, buf, sizeof(buf));
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
}

/*
 * Nothing to do here.
 */
static void vive_controller_imu_stop(OuvrtDevice *dev)
{
	(void)dev;
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_controller_imu_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_controller_imu_parent_class)->finalize(object);
}

static void ouvrt_vive_controller_imu_class_init(OuvrtViveControllerIMUClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_controller_imu_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_controller_imu_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_controller_imu_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_controller_imu_stop;
}

static void ouvrt_vive_controller_imu_init(OuvrtViveControllerIMU *self)
{
	self->dev.type = DEVICE_TYPE_CONTROLLER;
	self->priv = ouvrt_vive_controller_imu_get_instance_private(self);

	self->priv->config = NULL;
	self->priv->imu.sequence = 0;
	self->priv->imu.time = 0;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Controller device.
 */
OuvrtDevice *vive_controller_imu_new(const char *devnode G_GNUC_UNUSED)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_VIVE_CONTROLLER_IMU, NULL));
}
