/*
 * Microsoft HoloLens Sensors (Windows Mixed Reality) IMU
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hololens-imu.h"
#include "hololens-hid-reports.h"
#include "device.h"
#include "hidraw.h"
#include "imu.h"
#include "telemetry.h"

struct _OuvrtHoloLensIMU {
	OuvrtDevice dev;

	uint64_t last_timestamp;
	struct imu_state imu;
};

G_DEFINE_TYPE(OuvrtHoloLensIMU, ouvrt_hololens_imu, OUVRT_TYPE_DEVICE)

/*
 * Sends a command to the HoloLens Sensors HID device
 */
static int hololens_imu_send_command(OuvrtDevice *dev, uint8_t command)
{
	uint8_t data[64] = { 0x02, command };
	int ret;

	ret = write(dev->fd, data, sizeof data);

	return ret < 0 ? ret : 0;
}

static int hololens_imu_handle_imu_report(OuvrtHoloLensIMU *self,
					  struct hololens_imu_report *report)
{
	if (memcmp(report->gyro_timestamp,
		   report->accel_timestamp,
		   sizeof report->gyro_timestamp)) {
		g_print("%s: Gyro and accel timestamps do not match\n",
			self->dev.name);
	}

	for (int i = 0; i < 4; i++) {
		struct raw_imu_sample raw;
		struct imu_sample imu;
		uint16_t temperature;
		int64_t dt;

		/* Temperature in 10⁻² °C */
		temperature = __le16_to_cpu(report->temperature[i]);
		/* Time in 10⁻⁷ s @ 1 kHz */
		raw.time = __le64_to_cpu(report->gyro_timestamp[i]);
		/* Acceleration in 10⁻³ m/s² @ 1 kHz */
		raw.acc[0] = (int32_t)__le32_to_cpu(report->accel[0][i]);
		raw.acc[1] = (int32_t)__le32_to_cpu(report->accel[1][i]);
		raw.acc[2] = (int32_t)__le32_to_cpu(report->accel[2][i]);
		/* Angular velocity in 10⁻³ rad/s @ 8 kHz */
		raw.gyro[0] = (int16_t)__le16_to_cpu(report->gyro[0][8 * i + 0]) +
			      (int16_t)__le16_to_cpu(report->gyro[0][8 * i + 1]) +
			      (int16_t)__le16_to_cpu(report->gyro[0][8 * i + 2]) +
			      (int16_t)__le16_to_cpu(report->gyro[0][8 * i + 3]) +
			      (int16_t)__le16_to_cpu(report->gyro[0][8 * i + 4]) +
			      (int16_t)__le16_to_cpu(report->gyro[0][8 * i + 5]) +
			      (int16_t)__le16_to_cpu(report->gyro[0][8 * i + 6]) +
			      (int16_t)__le16_to_cpu(report->gyro[0][8 * i + 7]),
		raw.gyro[1] = (int16_t)__le16_to_cpu(report->gyro[1][8 * i + 0]) +
			      (int16_t)__le16_to_cpu(report->gyro[1][8 * i + 1]) +
			      (int16_t)__le16_to_cpu(report->gyro[1][8 * i + 2]) +
			      (int16_t)__le16_to_cpu(report->gyro[1][8 * i + 3]) +
			      (int16_t)__le16_to_cpu(report->gyro[1][8 * i + 4]) +
			      (int16_t)__le16_to_cpu(report->gyro[1][8 * i + 5]) +
			      (int16_t)__le16_to_cpu(report->gyro[1][8 * i + 6]) +
			      (int16_t)__le16_to_cpu(report->gyro[1][8 * i + 7]);
		raw.gyro[2] = (int16_t)__le16_to_cpu(report->gyro[2][8 * i + 0]) +
			      (int16_t)__le16_to_cpu(report->gyro[2][8 * i + 1]) +
			      (int16_t)__le16_to_cpu(report->gyro[2][8 * i + 2]) +
			      (int16_t)__le16_to_cpu(report->gyro[2][8 * i + 3]) +
			      (int16_t)__le16_to_cpu(report->gyro[2][8 * i + 4]) +
			      (int16_t)__le16_to_cpu(report->gyro[2][8 * i + 5]) +
			      (int16_t)__le16_to_cpu(report->gyro[2][8 * i + 6]) +
			      (int16_t)__le16_to_cpu(report->gyro[2][8 * i + 7]);

		telemetry_send_raw_imu_sample(self->dev.id, &raw);

		dt = raw.time - self->last_timestamp;

		/*
		 * Transform from IMU coordinate system into common coordinate
		 * system:
		 *
		 *   -y                                y
		 *    |          ⎡ 0 -1  0 ⎤ ⎡x⎤       |
		 *    +-- -x ->  ⎢-1  0  0 ⎥ ⎢y⎥  ->   +-- x
		 *   /           ⎣ 0  0 -1 ⎦ ⎣z⎦      /
		 * -z                                z
		 *
		 */
		imu.acceleration.x = raw.acc[1] * -1e-3;
		imu.acceleration.y = raw.acc[0] * -1e-3;
		imu.acceleration.z = raw.acc[2] * -1e-3;
		imu.angular_velocity.x = raw.gyro[1] * -(1e-3 / 8.0);
		imu.angular_velocity.y = raw.gyro[0] * -(1e-3 / 8.0);
		imu.angular_velocity.z = raw.gyro[2] * -(1e-3 / 8.0);
		imu.temperature = temperature * 0.01;
		imu.time = raw.time * 1e-7;

		telemetry_send_imu_sample(self->dev.id, &imu);

		pose_update(1e-7 * dt, &self->imu.pose, &imu);

		telemetry_send_pose(self->dev.id, &self->imu.pose);

		self->last_timestamp = raw.time;
	}

	if (report->message[0].code)
		g_print("%s: [%02x] %s\n", self->dev.name,
			report->message[0].code, report->message[0].text);
	if (report->message[1].code)
		g_print("%s: [%02x] %s\n", self->dev.name,
			report->message[1].code, report->message[1].text);

	return 0;
}

static int
hololens_imu_handle_control_report(G_GNUC_UNUSED OuvrtHoloLensIMU *self,
				   struct hololens_control_report *report)
{
	if (report->code == 4) {
		/* Reply to code 7 */
	}

	return 0;
}

/*
 * Blocks waiting for a reply to a sent command report.
 */
int hololens_imu_wait_reply(OuvrtDevice *dev,
			    struct hololens_control_report *report)
{
	struct pollfd fds;
	int ret;

	fds.fd = dev->fd;
	fds.events = POLLIN;
	fds.revents = 0;

	ret = poll(&fds, 1, 1000);
	if (ret == -1) {
		g_print("%s: Poll failure: %d\n", dev->name, errno);
		return -errno;
	}
	if (ret == 0) {
		g_print("%s: Poll failure: 0\n", dev->name);
		return -EINVAL;
	}

	if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) {
		return -ENODEV;
	}

	if (!(fds.revents & POLLIN)) {
		g_print("%s: Unhandled poll event: 0x%x\n", dev->name,
			fds.revents);
		return -EINVAL;
	}

	ret = read(dev->fd, report, sizeof(*report));
	if (ret == -1)
		return -errno;
	if (ret != 33 || report->id != 0x02) {
		g_print("%s: Unexpected %d-byte read\n", dev->name, ret);
		return -EINVAL;
	}

	return 0;
}

/*
 * Sends a command to the HoloLens Sensors HID device and synchronously waits
 * for an answer.
 */
static int hololens_imu_command_sync(OuvrtDevice *dev, uint8_t command,
				     struct hololens_control_report *report)
{
	int ret;

	ret = hololens_imu_send_command(dev, command);
	if (ret < 0) {
		g_print("Failed issue command 0x%02x: %d\n", command, ret);
		return ret;
	}

	ret = hololens_imu_wait_reply(dev, report);
	if (ret < 0) {
		g_print("Failed to receive reply: %d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * Reads the configuration metadata (command == HOLOLENS_COMMAND_CONFIG_META)
 * or data store (command == HOLOLENS_COMMAND_CONFIG_DATA).
 */
static int hololens_imu_config_read(OuvrtDevice *dev, uint8_t command,
				    uint8_t *buf, size_t count)
{
	struct hololens_control_report report;
	unsigned int offset = 0;
	int ret;

	ret = hololens_imu_command_sync(dev, HOLOLENS_COMMAND_CONFIG_START,
					&report);
	if (ret < 0)
		return ret;
	if (report.code != 0x04) {
		g_print("%s: Unexpected reply 0x%02x\n", dev->name,
			report.code);
		return -EINVAL;
	}

	ret = hololens_imu_command_sync(dev, command, &report);
	if (ret < 0)
		return ret;
	if (report.code != 0x00) {
		g_print("%s: Unexpected reply 0x%02x\n", dev->name,
			report.code);
		return -EINVAL;
	}

	for (;;) {
		ret = hololens_imu_command_sync(dev,
						HOLOLENS_COMMAND_CONFIG_READ,
						&report);
		if (ret < 0)
			return ret;
		if (report.code == 0x02)
			break;
		if (report.code != 0x01 || report.len > 30) {
			g_print("%s: Unexpected reply 0x%02x\n", dev->name,
				report.code);
			return -EINVAL;
		}
		if (offset + report.len > count) {
			g_print("%s: Out of space at %u+%u/%lu\n", dev->name,
				offset, report.len, count);
			return -ENOSPC;
		}
		memcpy(buf + offset, report.data, report.len);
		offset += report.len;
	}

	return offset;
}

/*
 * Opens the HoloLens Sensors HID device and powers up the IMU.
 */
static int hololens_imu_start(OuvrtDevice *dev)
{
	uint8_t config_meta[66];
	uint16_t config_len;
	uint8_t *config;
	int ret;

	ret = hololens_imu_config_read(dev, HOLOLENS_COMMAND_CONFIG_META,
				       config_meta, sizeof(config_meta));
	if (ret < 0) {
		g_print("%s: Failed to read configuration metadata: %d\n",
			dev->name, ret);
		return ret;
	}

	config_len = __le16_to_cpup((__le16 *)config_meta);

	config = calloc(config_len, 1);
	if (!config)
		return -ENOMEM;

	ret = hololens_imu_config_read(dev, HOLOLENS_COMMAND_CONFIG_DATA,
				       config, config_len);
	if (ret < 0) {
		g_print("%s: Failed to read configuration data: %d\n",
			dev->name, ret);
		return ret;
	}

	g_print("%s: Manufacturer: %.64s\n", dev->name, config + 8);
	g_print("%s: Model: %.64s\n", dev->name, config + 0x48);
	g_print("%s: Serial: %.64s\n", dev->name, config + 0x88);
	g_print("%s: GUID: %.39s\n", dev->name, config + 0xc8);
	g_print("%s: Name: %.64s\n", dev->name, config + 0x1c3);
	g_print("%s: Revision: %.32s\n", dev->name, config + 0x203);
	g_print("%s: Date: %.32s\n", dev->name, config + 0x223);

	g_free(config);

	ret = hololens_imu_send_command(dev, HOLOLENS_COMMAND_START_IMU);
	if (ret != 0)
		g_print("Failed to start\n");

	return 0;
}

/*
 * Handles HoloLens IMU messages
 */
static void hololens_imu_thread(OuvrtDevice *dev)
{
	OuvrtHoloLensIMU *self = OUVRT_HOLOLENS_IMU(dev);
	unsigned char buf[HOLOLENS_IMU_REPORT_SIZE];
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

		if (ret == 0)
			continue;

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

		if (ret == HOLOLENS_IMU_REPORT_SIZE &&
		    buf[0] == HOLOLENS_IMU_REPORT_ID) {
			hololens_imu_handle_imu_report(self, (void *)buf);
		} else if (ret == HOLOLENS_CONTROL_REPORT_SIZE &&
			   buf[0] == HOLOLENS_CONTROL_REPORT_ID) {
			hololens_imu_handle_control_report(self, (void *)buf);
		} else {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
			continue;
		}
	}
}

/*
 * Powers off the IMU.
 */
static void hololens_imu_stop(G_GNUC_UNUSED OuvrtDevice *dev)
{
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_hololens_imu_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_hololens_imu_parent_class)->finalize(object);
}

static void
ouvrt_hololens_imu_class_init(OuvrtHoloLensIMUClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_hololens_imu_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = hololens_imu_start;
	OUVRT_DEVICE_CLASS(klass)->thread = hololens_imu_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = hololens_imu_stop;
}

static void ouvrt_hololens_imu_init(OuvrtHoloLensIMU *self)
{
	self->dev.type = DEVICE_TYPE_HMD;
	self->imu.pose.rotation.w = 1.0;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated HoloLens IMU device.
 */
OuvrtDevice *hololens_imu_new(const char *devnode G_GNUC_UNUSED)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_HOLOLENS_IMU, NULL));
}
