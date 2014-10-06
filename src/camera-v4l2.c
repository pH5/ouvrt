#include <errno.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "camera-v4l2.h"
#include "blobwatch.h"
#include "debug.h"
#include "debug-gst.h"

#define MAX_BLOBS_PER_FRAME 42

/*
 * Requests buffers and starts streaming.
 *
 * Returns 0 on success, negative values on error.
 */
int camera_v4l2_start(struct device *dev)
{
	struct camera_v4l2 *camera = (struct camera_v4l2 *)dev;
	int width = camera->width;
	int height = camera->height;
	struct v4l2_format format = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.fmt.pix = {
			.width = width,
			.height = height,
			.pixelformat = camera->pixelformat,
			.field = V4L2_FIELD_ANY,
		}
	};
	struct v4l2_streamparm parm = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.parm.capture = {
			.timeperframe = {
				.numerator = 1,
				.denominator = camera->framerate,
			}
		}
	};
	struct v4l2_requestbuffers reqbufs = {
		.count = 3,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_MMAP,
	};
	int ret;
	int fd;
	unsigned int i;

	fd = camera->fd;

	if (camera->pixelformat == V4L2_PIX_FMT_GREY) {
		format.fmt.pix.bytesperline = width;
		format.fmt.pix.sizeimage = width * height;
	} else if (camera->pixelformat == V4L2_PIX_FMT_YUYV) {
		format.fmt.pix.bytesperline = width * 2;
		format.fmt.pix.sizeimage = width * height * 2;
	}
	camera->sizeimage = format.fmt.pix.sizeimage;

	ret = ioctl(fd, VIDIOC_S_FMT, &format);
	if (ret < 0)
		printf("v4l2: S_FMT error: %d\n", errno);

	ret = ioctl(fd, VIDIOC_S_PARM, &parm);
	if (ret < 0)
		printf("v4l2: S_PARM error: %d\n", errno);

	ret = ioctl(fd, VIDIOC_REQBUFS, &reqbufs);
	if (ret < 0)
		printf("v4l2: REQBUFS error: %d\n", errno);

	printf("v4l2: %dx%d %4.4s %d Hz, %d buffers\n",
	       format.fmt.pix.width, format.fmt.pix.height,
	       (char *)&format.fmt.pix.pixelformat,
	       parm.parm.capture.timeperframe.denominator /
	       parm.parm.capture.timeperframe.numerator,
	       reqbufs.count);

	for (i = 0; i < reqbufs.count; i++) {
		struct v4l2_buffer buf;

		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
		if (ret < 0)
			printf("v4l2: QUERYBUF error\n");

		ret = ioctl(fd, VIDIOC_QBUF, &buf);
		if (ret < 0)
			printf("v4l2: QBUF error\n");
	}

	ret = ioctl(fd, VIDIOC_STREAMON, &format.type);
	if (ret < 0) {
		printf("v4l2: STREAMON error\n");
		reqbufs.count = 0;
		ioctl(fd, VIDIOC_REQBUFS, &reqbufs);
	}

	printf("v4l2: Started streaming\n");

	camera->debug = debug_gst_new(width, height, camera->framerate);

	return ret;
}

/*
 * convert_yuyv_to_grayscale - convert YUYV frame to grayscale in place by
 *                             dropping the chroma components
 */
static void convert_yuyv_to_grayscale(uint8_t *frame, int width, int height)
{
	uint8_t *src, *dst;
	int x, y;

	for (y = 0, src = frame, dst = frame; y < height; y++) {
		for (x = 0; x < width; x++)
			dst[x] = src[2 * x];

		src += 2 * width;
		dst += width;
	}
}

/*
 * Receives frames from the camera and processes them.
 */
void camera_v4l2_thread(struct device *dev)
{
	struct camera_v4l2 *camera = (struct camera_v4l2 *)dev;
	struct blob blobs[MAX_BLOBS_PER_FRAME];
	struct v4l2_buffer buf;
	int width = camera->width;
	int height = camera->height;
	int sizeimage = camera->sizeimage;
	uint32_t *debug_frame;
	struct pollfd pfd;
	int num_blobs;
	void *raw;
	int ret;

	pfd.fd = camera->fd;
	pfd.events = POLLIN;

	while (dev->active) {
		ret = poll(&pfd, 1, 1000);
		if (ret == -1 || ret == 0) {
			if (ret == -1)
				printf("v4l2: poll error: %d\n", errno);
			continue;
		}

		if (pfd.events & (POLLERR | POLLHUP | POLLNVAL))
			break;

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ret = ioctl(camera->fd, VIDIOC_DQBUF, &buf);
		if (ret < 0) {
			printf("v4l2: DQBUF error: %d, disabling camera\n",
			       errno);
			dev->active = false;
			break;
		}

		raw = mmap(NULL, sizeimage, PROT_READ | PROT_WRITE,
			   MAP_SHARED, camera->fd, buf.m.offset);

		if (camera->pixelformat == V4L2_PIX_FMT_YUYV)
			convert_yuyv_to_grayscale(raw, width, height);

		debug_frame = debug_gst_frame_new(camera->debug, raw,
						  width, height);

		process_frame_extents(raw, width, height, raw);

		debug_draw_extents(debug_frame, width, height, raw);

		num_blobs = process_extent_blobs(raw, height,
						 blobs, MAX_BLOBS_PER_FRAME);

		munmap(raw, sizeimage);

		ret = ioctl(camera->fd, VIDIOC_QBUF, &buf);
		if (ret < 0) {
			printf("v4l2: QBUF error: %d, disabling camera\n",
			       errno);
			dev->active = false;
			break;
		}

		debug_draw_blobs(debug_frame, width, height, blobs, num_blobs);

		debug_gst_frame_push(camera->debug);
	}
}

/*
 * Stops streaming and releases buffers.
 */
void camera_v4l2_stop(struct device *dev)
{
	struct camera_v4l2 *camera = (struct camera_v4l2 *)dev;
	const struct v4l2_requestbuffers reqbufs = {
		.count = 0,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_MMAP,
	};
	int ret;

	ret = ioctl(camera->fd, VIDIOC_STREAMOFF, &reqbufs.type);
	if (ret < 0)
		printf("v4l2: STREAMOFF error: %d\n", errno);

	ret = ioctl(camera->fd, VIDIOC_REQBUFS, &reqbufs);
	if (ret < 0)
		printf("v4l2: REQBUFS error: %d\n", errno);

	printf("v4l2: Stopped streaming\n");

	camera->debug = debug_gst_unref(camera->debug);
}

/*
 * Frees common fields of the device structure. To be called from the camera
 * specific free operation.
 */
void camera_v4l2_fini(struct device *dev)
{
	struct camera_v4l2 *camera = (struct camera_v4l2 *)dev;

	close(camera->fd);
	device_fini(dev);
}

/*
 * Frees the camera structure.
 */
void camera_v4l2_free(struct device *dev)
{
	camera_v4l2_fini(dev);
	free(dev);
}

const struct device_ops camera_v4l2_ops = {
	.start = camera_v4l2_start,
	.thread = camera_v4l2_thread,
	.stop = camera_v4l2_stop,
	.free = camera_v4l2_free,
};

/*
 * Initializes common fields of the camera structure.
 *
 * Returns 0 on success, negative values on error.
 */
int camera_v4l2_init(struct device *dev, const char *devnode,
		     const struct device_ops *ops, int width, int height,
		     uint32_t pixelformat, int framerate)
{
	struct camera_v4l2 *camera = (struct camera_v4l2 *)dev;

	if (ops == NULL)
		ops = &camera_v4l2_ops;

	device_init(dev, devnode, ops);

	camera->fd = open(devnode, O_RDWR);
	if (camera->fd == -1) {
		printf("v4l2: Failed to open '%s': %d\n", devnode, errno);
		device_fini(&camera->dev);
		return camera->fd;
	}

	camera->width = width;
	camera->height = height;
	camera->pixelformat = pixelformat;
	camera->framerate = framerate;

	return 0;
}
