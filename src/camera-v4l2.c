/*
 * V4L2 camera class
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#include <errno.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "camera-v4l2.h"
#include "debug.h"
#include "tracker.h"

struct _OuvrtCameraV4L2Private {
	uint32_t offset[3];
	void *buf[3];
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtCameraV4L2, ouvrt_camera_v4l2,
			   OUVRT_TYPE_CAMERA)

/*
 * Opens the V4L2 device and checks that it supports video streaming.
 */
static int ouvrt_camera_v4l2_open(OuvrtDevice *dev)
{
	struct v4l2_capability cap;
	int ret;

	ret = OUVRT_DEVICE_CLASS(ouvrt_camera_v4l2_parent_class)->open(dev);
	if (ret < 0)
		return ret;

	ret = ioctl(dev->fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0) {
		g_print("v4l2: QUERYCAP error: %d\n", errno);
		return ret;
	}

	if (!(cap.capabilities & V4L2_CAP_DEVICE_CAPS)) {
		g_print("v4l2: Device does not report capabilities\n");
		return -EINVAL;
	}

	/* Silently ignore UVC metadata capture devices */
	if (cap.device_caps & V4L2_CAP_META_CAPTURE) {
		g_print("v4l2: Ignoring metadata capture device\n");
		return -ENODEV;
	}

	if (!(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)) {
		g_print("v4l2: Device does not capture video\n");
		return -ENODEV;
	}

	return 0;
}

/*
 * Requests buffers and starts streaming.
 *
 * Returns 0 on success, negative values on error.
 */
static int ouvrt_camera_v4l2_start(OuvrtDevice *dev)
{
	OuvrtCameraV4L2 *v4l2 = OUVRT_CAMERA_V4L2(dev);
	OuvrtCameraV4L2Private *priv = v4l2->priv;
	OuvrtCamera *camera = OUVRT_CAMERA(dev);
	int width = camera->width;
	int height = camera->height;
	struct v4l2_format format = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.fmt.pix = {
			.width = width,
			.height = height,
			.pixelformat = v4l2->pixelformat,
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
	__u32 prio = V4L2_PRIORITY_RECORD;
	int fd = dev->fd;
	int ret;
	unsigned int i;

	if (v4l2->pixelformat == V4L2_PIX_FMT_GREY)
		format.fmt.pix.bytesperline = width;
	else if (v4l2->pixelformat == V4L2_PIX_FMT_YUYV)
		format.fmt.pix.bytesperline = width * 2;
	format.fmt.pix.sizeimage = format.fmt.pix.bytesperline * height;
	camera->sizeimage = format.fmt.pix.sizeimage;

	ret = ioctl(fd, VIDIOC_S_FMT, &format);
	if (ret < 0) {
		g_print("v4l2: S_FMT error: %d\n", errno);
		return ret;
	}

	ret = ioctl(fd, VIDIOC_S_PARM, &parm);
	if (ret < 0)
		g_print("v4l2: S_PARM error: %d\n", errno);

	if (1 /* DEBUG */) {
		reqbufs.memory = V4L2_MEMORY_USERPTR;
		camera->sizeimage += sizeof(struct ouvrt_debug_attachment);
	}

	ret = ioctl(fd, VIDIOC_REQBUFS, &reqbufs);
	if (ret < 0)
		g_print("v4l2: REQBUFS error: %d\n", errno);
	if (reqbufs.count > 3) {
		g_print("v4l2: REQBUFS error: %d buffers\n", reqbufs.count);
		return -1;
	}

	g_print("v4l2: %dx%d %4.4s %d Hz, %d buffers à %d bytes\n",
		format.fmt.pix.width, format.fmt.pix.height,
		(char *)&format.fmt.pix.pixelformat,
		parm.parm.capture.timeperframe.denominator /
		parm.parm.capture.timeperframe.numerator,
		reqbufs.count, format.fmt.pix.sizeimage);

	for (i = 0; i < reqbufs.count; i++) {
		struct v4l2_buffer buf;

		buf.index = i;
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
		if (ret < 0)
			g_print("v4l2: QUERYBUF error\n");

		if (reqbufs.memory == V4L2_MEMORY_MMAP) {
			priv->buf[i] = mmap(NULL, camera->sizeimage,
					    PROT_READ | PROT_WRITE,
					    MAP_SHARED, dev->fd,
					    buf.m.offset);
			priv->offset[i] = buf.m.offset;
		} else if (reqbufs.memory == V4L2_MEMORY_USERPTR) {
			priv->buf[i] = malloc(camera->sizeimage);
			buf.m.userptr = (unsigned long)priv->buf[i];
		}

		ret = ioctl(fd, VIDIOC_QBUF, &buf);
		if (ret < 0)
			g_print("v4l2: QBUF error\n");
	}

	ret = ioctl(fd, VIDIOC_STREAMON, &format.type);
	if (ret < 0) {
		g_print("v4l2: STREAMON error\n");
		reqbufs.count = 0;
		ioctl(fd, VIDIOC_REQBUFS, &reqbufs);
	}

	g_print("v4l2: Started streaming\n");

	ret = ioctl(dev->fd, VIDIOC_S_PRIORITY, &prio);
	if (ret < 0)
		g_print("v4l2: S_PRIORITY error\n");

	camera->debug = debug_stream_new(width, height, camera->framerate);

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

static dquat rot;
static dvec3 trans;

/*
 * Receives frames from the camera and processes them.
 */
static void ouvrt_camera_v4l2_thread(OuvrtDevice *dev)
{
	OuvrtCameraV4L2 *v4l2 = OUVRT_CAMERA_V4L2(dev);
	OuvrtCameraV4L2Private *priv = v4l2->priv;
	OuvrtCamera *camera = OUVRT_CAMERA(dev);
	struct v4l2_buffer buf;
	int width = camera->width;
	int height = camera->height;
	double timestamps[4];
	struct timespec tp;
	struct pollfd pfd;
	int skipped;
	void *raw;
	int ret;

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = priv->offset[1] ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;

	pfd.fd = dev->fd;
	pfd.events = POLLIN;

	while (dev->active) {
		ret = poll(&pfd, 1, 1000);
		if (ret == -1 || ret == 0) {
			if (ret == -1)
				g_print("v4l2: poll error: %d\n", errno);
			continue;
		}

		if (pfd.events & (POLLERR | POLLHUP | POLLNVAL))
			break;

		ret = ioctl(dev->fd, VIDIOC_DQBUF, &buf);
		if (ret < 0) {
			if (errno == ENODEV)
				g_print("v4l2: camera disconnected, disabling\n");
			else
				g_print("v4l2: DQBUF error: %d, disabling camera\n",
				        errno);
			break;
		}

		clock_gettime(CLOCK_MONOTONIC, &tp);
		timestamps[0] = buf.timestamp.tv_sec + 1e-6 * buf.timestamp.tv_usec;
		timestamps[1] = tp.tv_sec + 1e-9 * tp.tv_nsec;

		if (buf.memory == V4L2_MEMORY_MMAP) {
			raw = priv->buf[buf.index];
			if (buf.m.offset != priv->offset[buf.index])
				raw = NULL;
		} else if (buf.memory == V4L2_MEMORY_USERPTR) {
			raw = (void *)buf.m.userptr;
			if (raw != priv->buf[buf.index])
				raw = NULL;
		} else {
			raw = NULL;
		}
		if (!raw) {
			g_print("v4l2: DQBUF error: %d, disabling camera\n",
				errno);
			dev->active = FALSE;
			break;
		}

		if (v4l2->pixelformat == V4L2_PIX_FMT_YUYV)
			convert_yuyv_to_grayscale(raw, width, height);

		skipped = buf.sequence - camera->sequence - 1;
		if (skipped < 0)
			skipped = 0;
		camera->sequence = buf.sequence;

		/*
		 * Find bright blobs in the camera image and identify individual LEDs
		 * using the estimated pose at time of exposure or, if that is not
		 * available, using the LED blinking pattern.
		 */
		struct blobservation *ob = NULL;
		if (camera->tracker) {
			ouvrt_tracker_process_frame(camera->tracker,
						    raw, width, height, skipped,
						    &ob);
		}

		clock_gettime(CLOCK_MONOTONIC, &tp);
		timestamps[2] = tp.tv_sec + 1e-9 * tp.tv_nsec;

		if (ob && camera->tracker) {
			/*
			 * If we got an observation, calculate the pose from
			 * blob detector output, intrinsic camera parameters,
			 * and the known LED positions.
			 */
			ouvrt_tracker_process_blobs(camera->tracker, ob->blobs,
						    ob->num_blobs,
						    &camera->camera_matrix,
						    camera->dist_coeffs,
						    &rot, &trans);
		}

		clock_gettime(CLOCK_MONOTONIC, &tp);
		timestamps[3] = tp.tv_sec + 1e-9 * tp.tv_nsec;

		ret = OUVRT_CAMERA_GET_CLASS(dev)->process_frame(camera, raw);
		if (ret == 0) {
			debug_stream_frame_push(camera->debug, raw,
						camera->sizeimage, width * height,
						ob, &rot, &trans, timestamps);
		}

		ret = ioctl(dev->fd, VIDIOC_QBUF, &buf);
		if (ret < 0) {
			g_print("v4l2: QBUF error: %d, disabling camera\n",
				errno);
			dev->active = FALSE;
			break;
		}
	}
}

/*
 * Stops streaming and releases buffers.
 */
static void ouvrt_camera_v4l2_stop(OuvrtDevice *dev)
{
	OuvrtCameraV4L2 *v4l2 = OUVRT_CAMERA_V4L2(dev);
	OuvrtCameraV4L2Private *priv = v4l2->priv;
	OuvrtCamera *camera = OUVRT_CAMERA(dev);
	struct v4l2_requestbuffers reqbufs = {
		.count = 0,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_MMAP,
	};
	__u32 prio = V4L2_PRIORITY_UNSET;
	int ret;
	int i;

	ret = ioctl(dev->fd, VIDIOC_S_PRIORITY, &prio);
	if (ret < 0)
		g_print("v4l2: S_PRIORITY error\n");

	if (priv->offset[1]) {
		for (i = 0; i < 3; i++) {
			munmap(priv->buf[i], camera->sizeimage);
			priv->buf[i] = NULL;
		}
	} else {
		reqbufs.memory = V4L2_MEMORY_USERPTR;
		for (i = 0; i < 3; i++) {
			free(priv->buf[i]);
			priv->buf[i] = NULL;
		}
	}

	ret = ioctl(dev->fd, VIDIOC_STREAMOFF, &reqbufs.type);
	if (ret < 0 && errno != ENODEV)
		g_print("v4l2: STREAMOFF error: %d\n", errno);

	ret = ioctl(dev->fd, VIDIOC_REQBUFS, &reqbufs);
	if (ret < 0 && errno != ENODEV)
		g_print("v4l2: REQBUFS error: %d\n", errno);

	g_print("v4l2: Stopped streaming\n");

	/* TODO: move up to camera */
	camera->debug = debug_stream_unref(camera->debug);
}

static void ouvrt_camera_v4l2_class_init(OuvrtCameraV4L2Class *klass)
{
	OuvrtDeviceClass *device_class = OUVRT_DEVICE_CLASS(klass);

	device_class->open = ouvrt_camera_v4l2_open;
	device_class->start = ouvrt_camera_v4l2_start;
	device_class->thread = ouvrt_camera_v4l2_thread;
	device_class->stop = ouvrt_camera_v4l2_stop;
}

/*
 * Initializes common fields of the camera structure.
 *
 * Returns 0 on success, negative values on error.
 */
static void ouvrt_camera_v4l2_init(OuvrtCameraV4L2 *self)
{
        self->priv = ouvrt_camera_v4l2_get_instance_private(self);
}
