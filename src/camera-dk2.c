#include <errno.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/fcntl.h>

#include <glib-object.h>

#include "camera-dk2.h"
#include "camera-v4l2.h"
#include "device.h"
#include "esp570.h"
#include "mt9v034.h"

#define WIDTH		752
#define HEIGHT		480
#define FRAMERATE	60

struct _OuvrtCameraDK2Private {
	char *version;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtCameraDK2, ouvrt_camera_dk2, \
			   OUVRT_TYPE_CAMERA_V4L2)

/*
 * Starts streaming and sets up the sensor for the exposure synchronization
 * signal from the Rift DK2.
 *
 * Returns 0 on success, negative values on error.
 */
static int camera_dk2_start(OuvrtDevice *dev)
{
	OuvrtCameraDK2 *camera = OUVRT_CAMERA_DK2(dev);
	OuvrtCameraDK2Class *klass = OUVRT_CAMERA_DK2_GET_CLASS(camera);
	int fd = camera->v4l2.camera.dev.fd;
	int ret;

	/* Call camera_v4l2_start to start streaming */
	ret = OUVRT_DEVICE_CLASS(ouvrt_camera_dk2_parent_class)->start(dev);
	if (ret < 0)
		return ret;

	/* Initialize the MT9V034 sensor for DK2 tracking */
	ret = mt9v034_sensor_setup(fd);
	if (ret < 0) {
		g_print("Camera DK2: Failed to initialize sensor: %d\n", ret);
		OUVRT_DEVICE_CLASS(klass)->stop(dev);
		return ret;
	}

	/* I have no idea what this does */
	esp570_i2c_write(fd, 0x60, 0x05, 0x0001);
	esp570_i2c_write(fd, 0x60, 0x06, 0x0020);

	return 0;
}

/*
 * Frees common fields of the device structure. To be called from the device
 * specific free operation.
 */
static void ouvrt_camera_dk2_finalize(GObject *object)
{
	free(OUVRT_CAMERA_DK2(object)->priv->version);
	G_OBJECT_CLASS(ouvrt_camera_dk2_parent_class)->finalize(object);
}

static void ouvrt_camera_dk2_class_init(OuvrtCameraDK2Class *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_camera_dk2_finalize;

	OUVRT_DEVICE_CLASS(klass)->start = camera_dk2_start;
}

static void ouvrt_camera_dk2_init(OuvrtCameraDK2 *self)
{
	OuvrtCamera *camera = OUVRT_CAMERA(self);

	camera->width = WIDTH;
	camera->height = HEIGHT;
	camera->framerate = FRAMERATE;
	self->v4l2.pixelformat = V4L2_PIX_FMT_GREY;
	self->priv = ouvrt_camera_dk2_get_instance_private(self);
}

static void camera_dk2_get_calibration(OuvrtCameraDK2 *camera_dk2)
{
	OuvrtCamera *camera = OUVRT_CAMERA(camera_dk2);
	OuvrtDevice *dev = &camera->dev;
	double * const A = camera->camera_matrix;
	double * const k = camera->dist_coeffs;
	char buf[128];
	double fx, fy, cx, cy;
	double k1, k2, p1, p2, k3;
	int ret;
	int i;

	/* Read 4 32-byte blocks at EEPROM address 0x2000 */
	for (i = 0; i < 128; i += 32) {
		ret = esp570_eeprom_read(dev->fd, 0x2000 + i, 32, buf + i);
		if (ret < 0)
			return;
	}

	fx = *(double *)(buf + 18);
	fy = *(double *)(buf + 30);
	cx = *(double *)(buf + 42);
	cy = *(double *)(buf + 54);
	k1 = *(double *)(buf + 66);
	k2 = *(double *)(buf + 78);
	p1 = *(double *)(buf + 90);
	p2 = *(double *)(buf + 102);
	k3 = *(double *)(buf + 114);

	/*
	 *     ⎡ fx 0  cx ⎤
	 * A = ⎢ 0  fy cy ⎥
	 *     ⎣ 0  0  1  ⎦
	 */
	A[0] = fx;  A[1] = 0.0; A[2] = cx;
	A[3] = 0.0; A[4] = fy;  A[5] = cy;
	A[6] = 0.0; A[7] = 0.0; A[8] = 1.0;

	/*
	 * k = [ k₁ k₂, p₁, p₂, k₃ ]
	 */
	k[0] = k1; k[1] = k2; k[2] = p1; k[3] = p2; k[4] = k3;
}

/*
 * Allocates and initializes the device structure, reads version and
 * serial from EEPROM, and does some unknown initialization.
 *
 * Returns the newly allocated DK2 camera device.
 */
OuvrtDevice *camera_dk2_new(const char *devnode)
{
	OuvrtCameraDK2 *camera;
	char buf[0x20 + 1];
	int ret;
	int fd;

	camera = g_object_new(OUVRT_TYPE_CAMERA_DK2, NULL);
	if (!camera)
		return NULL;

	fd = open(devnode, O_RDWR);
	if (fd == -1) {
		g_print("Camera DK2: Failed to open '%s': %d\n",
			devnode, errno);
		g_object_unref(camera);
		return NULL;
	}

	camera->v4l2.camera.dev.devnode = g_strdup(devnode);
	camera->v4l2.camera.dev.fd = fd;

	/* I have no idea what this does */
	esp570_setup_unknown_3(fd);

	memset(buf, 0, sizeof(buf));

	ret = esp570_eeprom_read(fd, 0x0ff0, 0x10, buf);
	if (ret == 0x10)
		camera->priv->version = g_strdup(buf);

	ret = esp570_eeprom_read(fd, 0x2800, 0x20, buf);
	if (ret == 0x20)
		camera->v4l2.camera.dev.serial = strdup(buf);

	camera_dk2_get_calibration(camera);

	return &camera->v4l2.camera.dev;
}
