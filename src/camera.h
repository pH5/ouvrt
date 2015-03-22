#ifndef __CAMERA_H__
#define __CAMERA_H__

#include <glib-object.h>

#include "device.h"

struct blobwatch;
struct debug_gst;

#define OUVRT_TYPE_CAMERA		(ouvrt_camera_get_type())
#define OUVRT_CAMERA(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_CAMERA, OuvrtCamera))
#define OUVRT_IS_CAMERA(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_CAMERA))
#define OUVRT_CAMERA_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
					 OUVRT_TYPE_CAMERA, OuvrtCameraClass))
#define OUVRT_IS_CAMERA_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_CAMERA))
#define OUVRT_CAMERA_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_CAMERA, OuvrtCameraClass))

typedef struct _OuvrtCamera		OuvrtCamera;
typedef struct _OuvrtCameraClass	OuvrtCameraClass;

struct _OuvrtCamera {
	OuvrtDevice dev;

	int width;
	int height;
	int framerate;
	double camera_matrix[9];
	double dist_coeffs[5];
	int sizeimage;
	int sequence;
	struct blobwatch *bw;
	struct debug_gst *debug;
};

struct _OuvrtCameraClass {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_camera_get_type(void);

#endif /* __CAMERA_H__ */
