/*
 * Device base class
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "device.h"

struct _OuvrtDevicePrivate {
	GThread *thread;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtDevice, ouvrt_device, G_TYPE_OBJECT)

/*
 * Stops the device before disposing of it
 */
static void ouvrt_device_dispose(GObject *object)
{
	ouvrt_device_stop(OUVRT_DEVICE(object));

	G_OBJECT_CLASS(ouvrt_device_parent_class)->dispose(object);
}

/*
 * Frees common fields of the device structure. To be called from the device
 * specific free operation.
 */
static void ouvrt_device_finalize(GObject *object)
{
	OuvrtDevice *dev = OUVRT_DEVICE(object);

	if (dev->fd != -1)
		close(dev->fd);
	free(dev->devnode);
	free(dev->name);
	free(dev->serial);
	G_OBJECT_CLASS(ouvrt_device_parent_class)->finalize(object);
}

static void ouvrt_device_class_init(OuvrtDeviceClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_device_finalize;
	G_OBJECT_CLASS(klass)->dispose = ouvrt_device_dispose;
}

/*
 * Initializes common fields of the device structure.
 */
static void ouvrt_device_init(OuvrtDevice *self)
{
	self->type = 0;
	self->devnodes[0] = NULL;
	self->devnodes[1] = NULL;
	self->devnodes[2] = NULL;
	self->name = NULL;
	self->serial = NULL;
	self->active = FALSE;
	self->fds[0] = -1;
	self->fds[1] = -1;
	self->fds[2] = -1;
	self->priv = ouvrt_device_get_instance_private(self);
	self->priv->thread = NULL;
}

/*
 * GThreadFunc that wraps the device specific thread worker function
 */
static gpointer device_start_routine(gpointer data)
{
	OuvrtDevice *dev = OUVRT_DEVICE(data);

	OUVRT_DEVICE_GET_CLASS(dev)->thread(dev);

	return NULL;
}

/*
 * Starts the device and its worker thread.
 */
int ouvrt_device_start(OuvrtDevice *dev)
{
	int ret;

	if (dev->active)
		return 0;

	ret = OUVRT_DEVICE_GET_CLASS(dev)->start(dev);
	if (ret < 0)
		return ret;

	dev->active = TRUE;
	dev->priv->thread = g_thread_new(NULL, device_start_routine, dev);

	return 0;
}

/*
 * Stops the device and its worker thread.
 */
void ouvrt_device_stop(OuvrtDevice *dev)
{
	if (!dev->active)
		return;

	dev->active = FALSE;

	g_thread_join(dev->priv->thread);
	dev->priv->thread = NULL;

	OUVRT_DEVICE_GET_CLASS(dev)->stop(dev);
}
