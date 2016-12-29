#ifndef __HIDRAW_H__
#define __HIDRAW_H__

#include <errno.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <time.h>

/*
 * Receives a feature report from the HID device.
 */
static inline int hid_get_feature_report(int fd, void *data, size_t length)
{
	return ioctl(fd, HIDIOCGFEATURE(length), data);
}

/*
 * Sends a feature report to the HID device.
 */
static inline int hid_send_feature_report(int fd, const void *data,
					  size_t length)
{
	return ioctl(fd, HIDIOCSFEATURE(length), data);
}

/*
 * Repeatedly tries to receive a feature report from the HID device
 * every millisecond until the timeout in milliseconds expires.
 */
static inline int hid_get_feature_report_timeout(int fd, void *buf, size_t len,
						 unsigned int timeout)
{
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 };
	unsigned int i;
	int ret;

	for (i = 0; i < timeout; i++) {
		ret = hid_get_feature_report(fd, buf, len);
		if (ret != -1 || errno != EPIPE)
			break;

		ts.tv_sec = 0;
		ts.tv_nsec = 1000000;
		nanosleep(&ts, NULL);
	}

	return ret;
}

#endif /* __HIDRAW_H__ */
