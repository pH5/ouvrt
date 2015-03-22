#ifndef __RIFT_DK2_H__
#define __RIFT_DK2_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"
#include "leds.h"
#include "math.h"

#define OUVRT_TYPE_RIFT_DK2		(ouvrt_rift_dk2_get_type())
#define OUVRT_RIFT_DK2(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_RIFT_DK2, OuvrtRiftDK2))
#define OUVRT_IS_RIFT_DK2(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_RIFT_DK2))
#define OUVRT_RIFT_DK2_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
					OUVRT_TYPE_RIFT_DK2, \
					OuvrtRiftDK2Class))
#define OUVRT_IS_RIFT_DK2_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_RIFT_DK2))
#define OUVRT_RIFT_DK2_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_RIFT_DK2, \
					 OuvrtRiftDK2Class))

#define MAX_POSITIONS	(MAX_LEDS + 1)

struct imu {
	vec3 position;
};

typedef struct _OuvrtRiftDK2		OuvrtRiftDK2;
typedef struct _OuvrtRiftDK2Class	OuvrtRiftDK2Class;
typedef struct _OuvrtRiftDK2Private	OuvrtRiftDK2Private;

struct _OuvrtRiftDK2Private;

struct _OuvrtRiftDK2 {
	OuvrtDevice dev;

	struct leds leds;
	struct imu imu;

	OuvrtRiftDK2Private *priv;
};

struct _OuvrtRiftDK2Class {
	OuvrtDeviceClass parent_class;
};

GType ouvrt_rift_dk2_get_type(void);

OuvrtDevice *rift_dk2_new(const char *devnode);

#endif /* __RIFT_DK2_H__ */
