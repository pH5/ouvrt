#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <glib.h>
#include <glib-object.h>

enum device_type {
	DEVICE_TYPE_HMD,
	DEVICE_TYPE_CAMERA,
};

#define OUVRT_TYPE_DEVICE		(ouvrt_device_get_type())
#define OUVRT_DEVICE(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
					 OUVRT_TYPE_DEVICE, OuvrtDevice))
#define OUVRT_IS_DEVICE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
					 OUVRT_TYPE_DEVICE))
#define OUVRT_DEVICE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
					 OUVRT_TYPE_DEVICE, OuvrtDeviceClass))
#define OUVRT_IS_DEVICE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
					 OUVRT_TYPE_DEVICE))
#define OUVRT_DEVICE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
					 OUVRT_TYPE_DEVICE, OuvrtDeviceClass))

typedef struct _OuvrtDevice		OuvrtDevice;
typedef struct _OuvrtDeviceClass	OuvrtDeviceClass;
typedef struct _OuvrtDevicePrivate	OuvrtDevicePrivate;

struct _OuvrtDevice {
	GObject parent_instance;

	enum device_type type;
	char *devnode;
	char *serial;
	gboolean active;
	int fd;

	OuvrtDevicePrivate *priv;
};

struct _OuvrtDeviceClass {
	GObjectClass parent_class;

	int (*start)(OuvrtDevice *dev);
	void (*thread)(OuvrtDevice *dev);
	void (*stop)(OuvrtDevice *dev);
};

GType ouvrt_device_get_type(void);

int ouvrt_device_start(OuvrtDevice *dev);
void ouvrt_device_stop(OuvrtDevice *dev);

#endif /* __DEVICE_H__ */
