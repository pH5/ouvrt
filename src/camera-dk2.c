#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "camera-dk2.h"
#include "camera-v4l2.h"
#include "device.h"

#define WIDTH		752
#define HEIGHT		480
#define FRAMERATE	60

struct camera_dk2 {
	struct camera_v4l2 v4l2;
	char *version;
};

/*
 * Starts streaming.
 *
 * Returns 0 on success, negative values on error.
 */
static int camera_dk2_start(struct device *dev)
{
	return camera_v4l2_start(dev);
}

/*
 * Frees the device structure and its contents.
 */
static void camera_dk2_free(struct device *dev)
{
	struct camera_dk2 *camera = (struct camera_dk2 *)dev;

	free(camera->version);
	camera_v4l2_fini(dev);
	free(camera);
}

static const struct device_ops camera_dk2_ops = {
	.start = camera_dk2_start,
	.thread = camera_v4l2_thread,
	.stop = camera_v4l2_stop,
	.free = camera_dk2_free,
};

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated DK2 camera device.
 */
struct device *camera_dk2_new(const char *devnode)
{
	struct camera_dk2 *camera;
	int ret;

	camera = malloc(sizeof(*camera));
	if (!camera)
		return NULL;

	memset(camera, 0, sizeof(*camera));
	ret = camera_v4l2_init(&camera->v4l2.dev, devnode, &camera_dk2_ops,
			       WIDTH, HEIGHT, V4L2_PIX_FMT_GREY, FRAMERATE);
	if (ret < 0) {
		free(camera);
		return NULL;
	}

	return (struct device *)camera;
}
