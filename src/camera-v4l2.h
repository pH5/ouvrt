#ifndef __CAMERA_V4L2_H__
#define __CAMERA_V4L2_H__

#include <stdint.h>

#include "device.h"

struct debug_gst;

struct camera_v4l2 {
	struct device dev;
	int fd;
	int width;
	int height;
	uint32_t pixelformat;
	int framerate;
	int sizeimage;
	struct debug_gst *debug;
};

int camera_v4l2_start(struct device *dev);
void camera_v4l2_thread(struct device *dev);
void camera_v4l2_stop(struct device *dev);

int camera_v4l2_init(struct device *dev, const char *devnode,
		     const struct device_ops *ops, int width, int height,
		     uint32_t pixelformat, int framerate);
void camera_v4l2_fini(struct device *dev);

#endif /* __CAMERA_V4L2_H__ */
