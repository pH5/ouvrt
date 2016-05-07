#ifndef __HIDRAW_H__
#define __HIDRAW_H__

#include <linux/hidraw.h>
#include <sys/ioctl.h>

/*
 * Receives a feature report from the HID device.
 */
static inline int hid_get_feature_report(int fd, const unsigned char *data,
					 size_t length)
{
	return ioctl(fd, HIDIOCGFEATURE(length), data);
}

/*
 * Sends a feature report to the HID device.
 */
static inline int hid_send_feature_report(int fd, const unsigned char *data,
					  size_t length)
{
	return ioctl(fd, HIDIOCSFEATURE(length), data);
}

#endif /* __HIDRAW_H__ */
