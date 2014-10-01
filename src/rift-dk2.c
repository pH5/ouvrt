#include <errno.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "rift-dk2.h"
#include "device.h"

struct rift_dk2 {
	struct device dev;
	int fd;
};

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
 * Sends a keepalive report to keep the device active for 10 seconds.
 */
static int rift_dk2_send_keepalive(struct rift_dk2 *rift)
{
	static const unsigned char keepalive[6] = {
		0x11, 0x00, 0x00, 0x0b, 0x10, 0x27
	};

	printf("Rift DK2: Sending keepalive\n");
	return hid_send_feature_report(rift->fd, keepalive, sizeof(keepalive));
}

/*
 * Enables the IR tracking LEDs.
 */
static int rift_dk2_start(struct device *dev)
{
	struct rift_dk2 *rift = (struct rift_dk2 *)dev;
	unsigned char tracking[13] = { 0x0c };
	const bool blink = false;
	const uint16_t exposure_us = 350;
	const uint16_t period_us = 16666;

	if (blink) {
		tracking[4] = 0x07;
	} else {
		tracking[3] = 0xff;
		tracking[4] = 0x05;
	}
	tracking[6] = exposure_us & 0xff;
	tracking[7] = (exposure_us >> 8) & 0xff;
	tracking[8] = period_us & 0xff;
	tracking[9] = (period_us >> 8) & 0xff;
	tracking[12] = 0x7f;
	return hid_send_feature_report(rift->fd, tracking, sizeof(tracking));
}

/*
 * Keeps the Rift active.
 */
void rift_dk2_thread(struct device *dev)
{
	struct rift_dk2 *rift = (struct rift_dk2 *)dev;
	unsigned char buf[64];
	struct pollfd fds;
	int count;
	int ret;

	rift_dk2_send_keepalive(rift);
	count = 0;

	while (dev->active) {
		fds.fd = rift->fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		if (ret == -1 || ret == 0 || count > 9000) {
			rift_dk2_send_keepalive(rift);
			count = 0;
			continue;
		}

		if (fds.events & (POLLERR | POLLHUP | POLLNVAL))
			break;

		ret = read(rift->fd, buf, sizeof(buf));
		if (ret < 64)
			printf("Error, invalid report\n");
		count++;
	}
}

/*
 * Disables the IR tracking LEDs.
 */
static void rift_dk2_stop(struct device *dev)
{
	struct rift_dk2 *rift = (struct rift_dk2 *)dev;
	unsigned char tracking[13] = { 0x0c };

	hid_get_feature_report(rift->fd, tracking, sizeof(tracking));
	tracking[4] &= ~(1 << 0);
	hid_send_feature_report(rift->fd, tracking, sizeof(tracking));
}

/*
 * Frees the device structure and its contents.
 */
static void rift_dk2_free(struct device *dev)
{
	struct rift_dk2 *rift = (struct rift_dk2 *)dev;

	close(rift->fd);
	device_fini(dev);
	free(rift);
}

static const struct device_ops rift_dk2_ops = {
	.start = rift_dk2_start,
	.thread = rift_dk2_thread,
	.stop = rift_dk2_stop,
	.free = rift_dk2_free,
};

/*
 * Allocates and initializes the device structure and opens the HID device
 * file descriptor.
 *
 * Returns the newly allocated Rift DK2 device.
 */
struct device *rift_dk2_new(const char *devnode)
{
	struct rift_dk2 *rift;

	rift = malloc(sizeof(*rift));
	if (!rift)
		return NULL;

	device_init(&rift->dev, devnode, &rift_dk2_ops);
	rift->fd = open(devnode, O_RDWR);
	if (rift->fd == -1) {
		printf("Rift DK2: Failed to open '%s': %d\n", devnode, errno);
		device_fini(&rift->dev);
		free(rift);
		return NULL;
	}

	return (struct device *)rift;
}
