/*
 * HTC Vive Headset IMU
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

#include "vive-headset-imu.h"
#include "vive-hid-reports.h"
#include "vive-config.h"
#include "vive-imu.h"
#include "device.h"
#include "hidraw.h"
#include "imu.h"
#include "json.h"
#include "math.h"

struct _OuvrtViveHeadsetIMUPrivate {
	JsonNode *config;
	struct vive_imu imu;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveHeadsetIMU, ouvrt_vive_headset_imu, \
			   OUVRT_TYPE_DEVICE)

#define VID_VALVE		0x28de
#define PID_VIVE_HEADSET_USB	0x2000

/*
 * Downloads the configuration data stored in the headset
 */
static int vive_headset_imu_get_config(OuvrtViveHeadsetIMU *self)
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
	if (strcmp(device_class, "hmd") != 0) {
		g_print("%s: Unknown device class \"%s\"\n", self->dev.name,
			device_class);
	}

	device_pid = json_object_get_int_member(object, "device_pid");
	if (device_pid != PID_VIVE_HEADSET_USB) {
		g_print("%s: Unknown device PID: 0x%04lx\n",
			self->dev.name, device_pid);
	}

	serial = json_object_get_string_member(object, "device_serial_number");
	if (strcmp(serial, self->dev.serial) != 0)
		g_print("%s: Configuration serial number differs: %s\n",
			self->dev.name, serial);

	device_vid = json_object_get_int_member(object, "device_vid");
	if (device_vid != VID_VALVE) {
		g_print("%s: Unknown device VID: 0x%04lx\n",
			self->dev.name, device_vid);
	}

	json_object_get_vec3_member(object, "gyro_bias", &imu->gyro_bias);
	json_object_get_vec3_member(object, "gyro_scale", &imu->gyro_scale);

	return 0;
}

/*
 * Retrieves the headset firmware version
 */
static int vive_headset_get_firmware_version(OuvrtViveHeadsetIMU *self)
{
	struct vive_firmware_version_report report = {
		.id = VIVE_FIRMWARE_VERSION_REPORT_ID,
	};
	uint32_t firmware_version;
	int ret;

	ret = hid_get_feature_report(self->dev.fd, &report, sizeof(report));
	if (ret < 0) {
		g_print("%s: Read error 0x05: %d\n", self->dev.name,
			errno);
		return ret;
	}

	firmware_version = __le32_to_cpu(report.firmware_version);

	g_print("%s: Headset firmware version %u %s@%s FPGA %u.%u\n",
		self->dev.name, firmware_version, report.string1,
		report.string2, report.fpga_version_major,
		report.fpga_version_minor);
	g_print("%s: Hardware revision: %d rev %d.%d.%d\n", self->dev.name,
		report.hardware_revision, report.hardware_version_major,
		report.hardware_version_minor, report.hardware_version_micro);

	return 0;
}

static int vive_headset_enable_lighthouse(OuvrtViveHeadsetIMU *self)
{
	unsigned char buf[5] = { 0x04 };
	int ret;

	ret = hid_send_feature_report(self->dev.fd, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	/*
	 * Reset Lighthouse Rx registers? Without this, inactive channels are
	 * not cleared to 0xff.
	 */
	buf[0] = 0x07;
	buf[1] = 0x02;
	return hid_send_feature_report(self->dev.fd, buf, sizeof(buf));
}

/*
 * Opens the IMU device, reads the stored configuration and enables
 * the Lighthouse receiver.
 */
static int vive_headset_imu_start(OuvrtDevice *dev)
{
	OuvrtViveHeadsetIMU *self = OUVRT_VIVE_HEADSET_IMU(dev);
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

	ret = vive_headset_get_firmware_version(self);
	if (ret < 0) {
		g_print("%s: Failed to get firmware version\n", dev->name);
		return ret;
	}

	ret = vive_headset_imu_get_config(self);
	if (ret < 0) {
		g_print("%s: Failed to read configuration\n", dev->name);
		return ret;
	}

	ret = vive_headset_enable_lighthouse(self);
	if (ret < 0) {
		g_print("%s: Failed to enable Lighthouse Receiver\n",
			dev->name);
		return ret;
	}

	return 0;
}

/*
 * Handles IMU messages.
 */
static void vive_headset_imu_thread(OuvrtDevice *dev)
{
	OuvrtViveHeadsetIMU *self = OUVRT_VIVE_HEADSET_IMU(dev);
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
		if (ret != 52 || buf[0] != VIVE_IMU_REPORT_ID) {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
			continue;
		}

		vive_imu_decode_message(&self->priv->imu, buf, 52);
	}
}

/*
 * Nothing to do here.
 */
static void vive_headset_imu_stop(OuvrtDevice *dev)
{
	(void)dev;
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_headset_imu_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_headset_imu_parent_class)->finalize(object);
}

static void ouvrt_vive_headset_imu_class_init(OuvrtViveHeadsetIMUClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_headset_imu_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_headset_imu_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_headset_imu_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_headset_imu_stop;
}

static void ouvrt_vive_headset_imu_init(OuvrtViveHeadsetIMU *self)
{
	self->dev.type = DEVICE_TYPE_HMD;
	self->priv = ouvrt_vive_headset_imu_get_instance_private(self);
	self->priv->imu.sequence = 0;
	self->priv->imu.time = 0;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Headset IMU device.
 */
OuvrtDevice *vive_headset_imu_new(const char *devnode)
{
	OuvrtViveHeadsetIMU *vive;

	vive = g_object_new(OUVRT_TYPE_VIVE_HEADSET_IMU, NULL);
	if (vive == NULL)
		return NULL;

	vive->dev.devnode = g_strdup(devnode);

	return &vive->dev;
}
