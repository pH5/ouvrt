/*
 * Microsoft HoloLens Sensors (Windows Mixed Reality) stereo camera
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/fcntl.h>

#include <glib-object.h>

#include "hololens-camera.h"
#include "camera-v4l2.h"
#include "device.h"

#define HOLOLENS_CAMERA_WIDTH		1280
#define HOLOLENS_CAMERA_HEIGHT		481
#define HOLOLENS_CAMERA_FRAMERATE	90

struct _OuvrtHoloLensCamera {
	OuvrtCameraV4L2 v4l2;
};

G_DEFINE_TYPE(OuvrtHoloLensCamera, ouvrt_hololens_camera, \
	      OUVRT_TYPE_CAMERA_V4L2)

static int hololens_camera_process_frame(OuvrtCamera *camera, void *frame)
{
	uint8_t *buf = frame;
	uint16_t gain; /* or could be additional exposure time */

	/* The first line contains metadata, possibly register values */
	gain = __be16_to_cpup((__be16 *)(buf + 6));

	if (gain == 155 || /* 30 fps */
	    gain == 300) { /* 90 fps */
		/* Bright frame, headset tracking */
	} else if (gain == 0) {
		/* Dark frame, controller tracking */
		return 1; /* do not push into debug pipeline */
	} else {
		g_print("Unexpected gain: %u\n", gain);
	}

	return 0;
}

static void ouvrt_hololens_camera_class_init(OuvrtHoloLensCameraClass *klass)
{
	OUVRT_CAMERA_CLASS(klass)->process_frame = hololens_camera_process_frame;
}

/*
 * Initializes the Video4Linux parameters.
 */
static void ouvrt_hololens_camera_init(OuvrtHoloLensCamera *self)
{
	OuvrtCamera *camera = OUVRT_CAMERA(self);

	camera->width = HOLOLENS_CAMERA_WIDTH;
	camera->height = HOLOLENS_CAMERA_HEIGHT;
	camera->framerate = HOLOLENS_CAMERA_FRAMERATE;
	self->v4l2.pixelformat = V4L2_PIX_FMT_GREY;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated HoloLens camera device.
 */
OuvrtDevice *hololens_camera_new(const char *devnode)
{
	OuvrtHoloLensCamera *camera;
	int fd;

	camera = g_object_new(OUVRT_TYPE_HOLOLENS_CAMERA, NULL);
	if (!camera)
		return NULL;

	fd = open(devnode, O_RDWR);
	if (fd == -1) {
		g_print("HoloLens Camera: Failed to open '%s': %d\n",
			devnode, errno);
		g_object_unref(camera);
		return NULL;
	}

	camera->v4l2.camera.dev.devnode = g_strdup(devnode);
	camera->v4l2.camera.dev.fd = fd;

	return &camera->v4l2.camera.dev;
}
