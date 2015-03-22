#ifndef __CAMERA_DK2_H__
#define __CAMERA_DK2_H__

#include <glib.h>
#include <glib-object.h>

#include "camera-v4l2.h"
#include "device.h"

#define OUVRT_TYPE_CAMERA_DK2		(ouvrt_camera_dk2_get_type())
#define OUVRT_CAMERA_DK2(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_CAMERA_DK2, OuvrtCameraDK2))
#define OUVRT_IS_CAMERA_DK2(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_CAMERA_DK2))
#define OUVRT_CAMERA_DK2_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass), \
					 OUVRT_TYPE_CAMERA_DK2, \
					 OuvrtCameraDK2Class))
#define OUVRT_IS_CAMERA_DK2_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_CAMERA_DK2))
#define OUVRT_CAMERA_DK2_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_CAMERA_DK2, \
					 OuvrtCameraDK2Class))

typedef struct _OuvrtCameraDK2		OuvrtCameraDK2;
typedef struct _OuvrtCameraDK2Class	OuvrtCameraDK2Class;
typedef struct _OuvrtCameraDK2Private	OuvrtCameraDK2Private;

struct _OuvrtCameraDK2 {
	OuvrtCameraV4L2 v4l2;

	OuvrtCameraDK2Private *priv;
};

struct _OuvrtCameraDK2Class {
	OuvrtCameraV4L2Class parent_class;
};

GType ouvrt_camera_dk2_get_type(void);

OuvrtDevice *camera_dk2_new(const char *devnode);

void ouvrt_camera_dk2_set_sync_exposure(OuvrtCameraDK2 *camera, gboolean sync);

#endif /* __CAMERA_DK2_H__ */
