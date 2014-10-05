#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "camera-dk2.h"
#include "camera-v4l2.h"
#include "device.h"
#include "esp570.h"
#include "mt9v034.h"

#define WIDTH		752
#define HEIGHT		480
#define FRAMERATE	60

struct camera_dk2 {
	struct camera_v4l2 v4l2;
	char *version;
};

/*
 * Starts streaming and sets up the sensor for the exposure synchronization
 * signal from the Rift DK2.
 *
 * Returns 0 on success, negative values on error.
 */
static int camera_dk2_start(struct device *dev)
{
	struct camera_dk2 *camera = (struct camera_dk2 *)dev;
	int fd = camera->v4l2.fd;
	int ret;

	ret = camera_v4l2_start(dev);
	if (ret < 0)
		return ret;

	/* Initialize the MT9V034 sensor for DK2 tracking */
	ret = mt9v034_sensor_setup(fd);
	if (ret < 0) {
		printf("Failed to initialize sensor: %d\n", ret);
		camera_v4l2_stop(dev);
		return ret;
	}

	/* I have no idea what this does */
	esp570_i2c_write(fd, 0x60, 0x05, 0x0001);
	esp570_i2c_write(fd, 0x60, 0x06, 0x0020);

	return 0;
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
 * Allocates and initializes the device structure, reads version and
 * serial from EEPROM, and does some unknown initialization.
 *
 * Returns the newly allocated DK2 camera device.
 */
struct device *camera_dk2_new(const char *devnode)
{
	struct camera_dk2 *camera;
	char buf[0x20 + 1];
	int ret;
	int fd;

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

	fd = camera->v4l2.fd;

	/* I have no idea what this does */
	esp570_setup_unknown_3(fd);

	memset(buf, 0, sizeof(buf));

	ret = esp570_eeprom_read(fd, 0x0ff0, 0x10, buf);
	if (ret == 0x10)
		camera->version = strdup(buf);

	ret = esp570_eeprom_read(fd, 0x2800, 0x20, buf);
	if (ret == 0x20)
		camera->v4l2.dev.serial = strdup(buf);

	return (struct device *)camera;
}
