#include <asm/byteorder.h>
#include <errno.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <math.h>

#include "rift-dk2.h"
#include "debug.h"
#include "device.h"
#include "math.h"
#include "leds.h"
#include "tracker.h"

/* temporary global */
gboolean rift_dk2_flicker = false;

struct _OuvrtRiftDK2Private {
	gboolean flicker;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtRiftDK2, ouvrt_rift_dk2, OUVRT_TYPE_DEVICE)

/*
 * Receives a feature report from the HID device.
 */
static int hid_get_feature_report(int fd, const unsigned char *data,
				  size_t length)
{
	return ioctl(fd, HIDIOCGFEATURE(length), data);
}

/*
 * Sends a feature report to the HID device.
 */
static int hid_send_feature_report(int fd, const unsigned char *data,
				   size_t length)
{
	return ioctl(fd, HIDIOCSFEATURE(length), data);
}

/*
 * Obtains the factory calibrated position data of IR LEDs and IMU
 * from the Rift DK2. Values are stored with µm accuracy in the
 * Rift's local reference frame: the positive x axis points left,
 * the y axis points upward, and z forward:
 *
 *      up
 *       y z forward
 * left  |/
 *    x--+
 */
static int rift_dk2_get_positions(OuvrtRiftDK2 *rift)
{
	unsigned char buf[30] = { 0x0f };
	int fd = rift->dev.fd;
	uint8_t type;
	vec3 pos, dir;
	uint16_t index;
	uint16_t num;
	int ret;
	int i;

	ret = hid_get_feature_report(fd, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	num = __le16_to_cpup((__le16 *)(buf + 26));
	if (num > MAX_POSITIONS)
		return -1;

	for (i = 0; ; i++) {
		index = __le16_to_cpup((__le16 *)(buf + 24));
		if (index >= num)
			return -1;

		type = __le16_to_cpup((__le16 *)(buf + 28));

		/* Position in µm */
		pos.x = 1e-6f * (int32_t)__le32_to_cpup((__le32 *)(buf + 4));
		pos.y = 1e-6f * (int32_t)__le32_to_cpup((__le32 *)(buf + 8));
		pos.z = 1e-6f * (int32_t)__le32_to_cpup((__le32 *)(buf + 12));

		if (type == 0) {
			rift->leds.positions[index] = pos;

			/* Direction, magnitude in unknown units */
			dir.x = 1e-6f * (int16_t)__le16_to_cpup((__le16 *)(buf + 16));
			dir.y = 1e-6f * (int16_t)__le16_to_cpup((__le16 *)(buf + 18));
			dir.z = 1e-6f * (int16_t)__le16_to_cpup((__le16 *)(buf + 20));
			rift->leds.directions[index] = dir;
		} else if (type == 1) {
			rift->imu.position = pos;
		}

		/* Break out before reading the first report again */
		if (i + 1 == num)
			break;

		ret = hid_get_feature_report(fd, buf, sizeof(buf));
		if (ret < 0)
			return ret;
	}

	rift->leds.num = num - 1;

	return 0;
}

/*
 * Obtains the blinking patterns of the 40 IR LEDs from the Rift DK2.
 */
static int rift_dk2_get_led_patterns(OuvrtRiftDK2 *rift)
{
	unsigned char buf[12] = { 0x10 };
	int fd = rift->dev.fd;
	uint8_t pattern_length;
	uint32_t pattern;
	uint16_t index;
	uint16_t num;
	int ret;
	int i;

	ret = hid_get_feature_report(fd, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	num = __le16_to_cpup((__le16 *)(buf + 10));
	if (num > MAX_LEDS)
		return -1;

	for (i = 0; ; i++) {
		index = __le16_to_cpup((__le16 *)(buf + 8));
		if (index >= num)
			return -1;

		pattern_length = buf[3];
		pattern = __le32_to_cpup((__le32 *)(buf + 4));

		/* pattern_length should be 10 */
		if (pattern_length != 10) {
			g_print("Rift DK2: Unexpected pattern length: %d\n",
				pattern_length);
			return -1;
		}

		/*
		 * pattern should consist of 10 2-bit values that are either
		 * 1 (dark) or 3 (bright).
		 */
		if ((pattern & ~0xaaaaa) != 0x55555) {
			g_print("Rift DK2: Unexpected pattern: 0x%x\n",
				pattern);
			return -1;
		}

		/* Convert into 10 single-bit values 1 -> 0, 3 -> 1 */
		pattern &= 0xaaaaa;
		pattern |= pattern >> 1;
		pattern &= 0x66666;
		pattern |= pattern >> 2;
		pattern &= 0xe1e1e;
		pattern |= pattern >> 4;
		pattern &= 0xe01fe;
		pattern |= pattern >> 8;
		pattern = (pattern >> 1) & 0x3ff;

		rift->leds.patterns[index] = pattern;

		/* Break out before reading the first report again */
		if (i + 1 == num)
			break;

		ret = hid_get_feature_report(fd, buf, sizeof(buf));
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * Sends a keepalive report to keep the device active for 10 seconds.
 */
static int rift_dk2_send_keepalive(OuvrtRiftDK2 *rift)
{
	unsigned char buf[6] = { 0x11 };
	const uint16_t keepalive_ms = 10000;

	buf[3] = 0xb;
	*(uint16_t *)(buf + 4) = __cpu_to_le16(keepalive_ms);

	return hid_send_feature_report(rift->dev.fd, buf, sizeof(buf));
}

/*
 * Sends a tracking report to enable the IR tracking LEDs.
 */
static int rift_dk2_send_tracking(OuvrtRiftDK2 *rift, bool blink)
{
	unsigned char buf[13] = { 0x0c };
	const uint16_t exposure_us = 350;
	const uint16_t period_us = 16666;
	const uint16_t vsync_offset = 0;
	const uint8_t duty_cycle = 0x7f;

	if (blink) {
		buf[4] = 0x07;
	} else {
		buf[3] = 0xff;
		buf[4] = 0x05;
	}
	*(uint16_t *)(buf + 6) = __cpu_to_le16(exposure_us);
	*(uint16_t *)(buf + 8) = __cpu_to_le16(period_us);
	*(uint16_t *)(buf + 10) = __cpu_to_le16(vsync_offset);
	buf[12] = duty_cycle;

	return hid_send_feature_report(rift->dev.fd, buf, sizeof(buf));
}

/*
 * Enables the IR tracking LEDs and registers them with the tracker.
 */
static int rift_dk2_start(OuvrtDevice *dev)
{
	OuvrtRiftDK2 *rift = OUVRT_RIFT_DK2(dev);
	int fd = rift->dev.fd;
	int ret;

	if (fd == -1) {
		fd = open(rift->dev.devnode, O_RDWR);
		if (fd == -1) {
			g_print("Rift DK2: Failed to open '%s': %d\n",
				rift->dev.devnode, errno);
			return -1;
		}
		rift->dev.fd = fd;
	}

	ret = rift_dk2_get_positions(rift);
	if (ret < 0) {
		g_print("Rift DK2: Error reading factory calibrated positions\n");
		return ret;
	}

	ret = rift_dk2_get_led_patterns(rift);
	if (ret < 0) {
		g_print("Rift DK2: Error reading IR LED blinking patterns\n");
		return ret;
	}
	if (rift->leds.num != 40)
		g_print("Rift DK2: Reported %d IR LEDs\n", rift->leds.num);

	ret = rift_dk2_send_tracking(rift, TRUE);
	if (ret < 0)
		return ret;

	tracker_register_leds(&rift->leds);

	return 0;
}

/*
 * Keeps the Rift active.
 */
static void rift_dk2_thread(OuvrtDevice *dev)
{
	OuvrtRiftDK2 *rift = OUVRT_RIFT_DK2(dev);
	unsigned char buf[64];
	struct pollfd fds;
	int count;
	int ret;

	g_print("Rift DK2: Sending keepalive\n");
	rift_dk2_send_keepalive(rift);
	count = 0;

	while (dev->active) {
		fds.fd = rift->dev.fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		if (ret == -1 || ret == 0 ||
		    count > 9000) {
			if (ret == -1 || ret == 0)
				g_print("Rift DK2: Resending keepalive\n");
			rift_dk2_send_keepalive(rift);
			count = 0;
			continue;
		}

		if (fds.events & (POLLERR | POLLHUP | POLLNVAL))
			break;

		ret = read(rift->dev.fd, buf, sizeof(buf));
		if (ret < 64) {
			g_print("Error, invalid report\n");
			continue;
		}

		count++;
	}
}

/*
 * Disables the IR tracking LEDs and unregisters model from the
 * tracker.
 */
static void rift_dk2_stop(OuvrtDevice *dev)
{
	OuvrtRiftDK2 *rift = OUVRT_RIFT_DK2(dev);
	unsigned char tracking[13] = { 0x0c };
	int fd = rift->dev.fd;

	tracker_unregister_leds(&rift->leds);

	hid_get_feature_report(fd, tracking, sizeof(tracking));
	tracking[4] &= ~(1 << 0);
	hid_send_feature_report(fd, tracking, sizeof(tracking));
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_rift_dk2_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_rift_dk2_parent_class)->finalize(object);
}

static void ouvrt_rift_dk2_class_init(OuvrtRiftDK2Class *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_rift_dk2_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = rift_dk2_start;
	OUVRT_DEVICE_CLASS(klass)->thread = rift_dk2_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = rift_dk2_stop;
}

static void ouvrt_rift_dk2_init(OuvrtRiftDK2 *self)
{
	self->dev.type = DEVICE_TYPE_HMD;
	self->priv = ouvrt_rift_dk2_get_instance_private(self);
	self->priv->flicker = false;
}

/*
 * Allocates and initializes the device structure and opens the HID device
 * file descriptor.
 *
 * Returns the newly allocated Rift DK2 device.
 */
OuvrtDevice *rift_dk2_new(const char *devnode)
{
	OuvrtRiftDK2 *rift;

	rift = g_object_new(OUVRT_TYPE_RIFT_DK2, NULL);
	if (rift == NULL)
		return NULL;

	rift->dev.devnode = g_strdup(devnode);

	return &rift->dev;
}

void ouvrt_rift_dk2_set_flicker(OuvrtRiftDK2 *rift, gboolean flicker)
{
	if (rift->priv->flicker == flicker)
		return;

	rift->priv->flicker = flicker;
	rift_dk2_flicker = flicker;

	if (rift->dev.active)
		rift_dk2_send_tracking(rift, flicker);
}
