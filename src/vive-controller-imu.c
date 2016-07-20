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
#include "vive-hid-reports.h"
#include "vive-imu.h"
#include "device.h"
#include "hidraw.h"
#include "imu.h"
#include "json.h"
#include "math.h"

struct _OuvrtViveControllerIMUPrivate {
	JsonNode *config;
	const gchar *serial;
	struct vive_imu imu;
	vec3 acc_bias;
	vec3 acc_scale;
	vec3 gyro_bias;
	vec3 gyro_scale;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveControllerIMU, ouvrt_vive_controller_imu, \
			   OUVRT_TYPE_DEVICE)

/*
 * Retrieves the controller firmware version
 */
static int vive_controller_imu_get_firmware_version(OuvrtViveControllerIMU *self)
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
static int vive_controller_imu_get_config(OuvrtViveControllerIMU *self)
{
	char *config_json;
	JsonObject *object;

	config_json = ouvrt_vive_get_config(&self->dev);
	if (!config_json)
		return -1;

	self->priv->config = json_from_string(config_json, NULL);
	g_free(config_json);
	if (!self->priv->config) {
		g_print("Vive Controller %s: Parsing JSON configuration data failed\n",
			self->dev.serial);
		return -1;
	}

	object = json_node_get_object(self->priv->config);

	json_object_get_vec3_member(object, "acc_bias", &self->priv->acc_bias);
	json_object_get_vec3_member(object, "acc_scale", &self->priv->acc_scale);

	self->priv->serial = json_object_get_string_member(object,
							   "device_serial_number");

	json_object_get_vec3_member(object, "gyro_bias", &self->priv->gyro_bias);
	json_object_get_vec3_member(object, "gyro_scale", &self->priv->gyro_scale);

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

	if (fd == -1) {
		fd = open(dev->devnode, O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("%s: Failed to open '%s': %d\n", dev->name,
				dev->devnode, errno);
			return -1;
		}
		dev->fd = fd;
	}

	ret = vive_controller_imu_get_firmware_version(self);
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
			g_print("Vive Controller %s: Read error: %d\n",
				self->priv->serial, errno);
			continue;
		}
		if (ret == 52 && buf[0] == VIVE_IMU_REPORT_ID) {
			vive_imu_decode_message(&self->priv->imu, buf, ret);
		} else {
			g_print("Vive Controller %s: Error, invalid %d-byte report 0x%02x\n",
				self->priv->serial, ret, buf[0]);
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
OuvrtDevice *vive_controller_imu_new(const char *devnode)
{
	OuvrtViveControllerIMU *vive;

	vive = g_object_new(OUVRT_TYPE_VIVE_CONTROLLER_IMU, NULL);
	if (vive == NULL)
		return NULL;

	vive->dev.devnode = g_strdup(devnode);

	return &vive->dev;
}
