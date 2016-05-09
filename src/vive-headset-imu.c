/*
 * HTC Vive Headset IMU
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

#include "vive-headset-imu.h"
#include "device.h"
#include "hidraw.h"

struct _OuvrtViveHeadsetIMUPrivate {
	uint8_t sequence;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveHeadsetIMU, ouvrt_vive_headset_imu, \
			   OUVRT_TYPE_DEVICE)

/*
 * Retrieves the headset firmware version
 */
static int vive_headset_get_firmware_version(OuvrtViveHeadsetIMU *self)
{
	uint32_t firmware_version;
	unsigned char buf[64];
	int ret;

	buf[0] = 0x05;
	ret = hid_get_feature_report(self->dev.fd, buf, sizeof(buf));
	if (ret < 0) {
		g_print("%s: Read error 0x05: %d\n", self->dev.name,
			errno);
		return ret;
	}

	firmware_version = __le32_to_cpup((__le32 *)(buf + 1));

	g_print("%s: Headset firmware version %u %s@%s FPGA %u.%u\n",
		self->dev.name, firmware_version, buf + 9, buf + 25, buf[50],
		buf[49]);
	g_print("%s: Hardware revision: %d rev %d.%d.%d\n",
		self->dev.name, buf[44], buf[43], buf[42], buf[41]);

	return 0;
}

/*
 * Decodes the periodic sensor message containing IMU sample(s).
 */
static void vive_headset_imu_decode_message(OuvrtViveHeadsetIMU *self,
					    const unsigned char *buf,
					    size_t len)
{
	uint8_t last_seq = self->priv->sequence;
	uint8_t seq[3];
	int i, j;

	(void)len;

	/*
	 * The three samples are updated round-robin. New messages
	 * can contain already seen samples in any place, but the
	 * sequence numbers should always be consecutive.
	 */
	seq[0] = buf[17];
	seq[1] = buf[34];
	seq[2] = buf[51];

	/* Start at the sample with the oldest sequence number */
	if (seq[0] == (uint8_t)(seq[1] + 2))
		i = 1;
	else if (seq[1] == (uint8_t)(seq[2] + 2))
		i = 2;
	else
		i = 0;
	j = (i + 2) % 3;

	/* From there, handle all new samples */
	for (; i != j; i = (i + 1) % 3) {
		const unsigned char *sample = buf + 1 + i * 17;
		int16_t acc[3];
		int16_t gyro[3];
		uint32_t time;

		/* Skip already seen samples */
		if (seq[i] == last_seq ||
		    seq[i] == (uint8_t)(last_seq - 1) ||
		    seq[i] == (uint8_t)(last_seq - 2))
			continue;

		acc[0] = __le16_to_cpup((__le16 *)sample);
		acc[1] = __le16_to_cpup((__le16 *)(sample + 2));
		acc[2] = __le16_to_cpup((__le16 *)(sample + 4));
		gyro[0] = __le16_to_cpup((__le16 *)(sample + 6));
		gyro[1] = __le16_to_cpup((__le16 *)(sample + 8));
		gyro[2] = __le16_to_cpup((__le16 *)(sample + 10));
		time = __le32_to_cpup((__le32 *)(sample + 12));

		(void)acc;
		(void)gyro;
		(void)time;

		self->priv->sequence = seq[j];
	}
}

/*
 * Opens the IMU HID device file descriptor.
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

		if (fds.events & (POLLERR | POLLHUP | POLLNVAL))
			break;

		if (!(fds.revents & POLLIN)) {
			g_print("%s: Poll timeout\n", dev->name);
			continue;
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("%s: Read error: %d\n", dev->name, errno);
			continue;
		}
		if (ret != 52) {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
			continue;
		}

		vive_headset_imu_decode_message(self, buf, 52);
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
