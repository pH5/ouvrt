/*
 * Device base class
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#include <glib.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include "device.h"

struct _OuvrtDevicePrivate {
	GThread *thread;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(OuvrtDevice, ouvrt_device, G_TYPE_OBJECT)

static GHashTable *serial_to_id_table;

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

/*
 * Opens all file descriptors related to the device.
 */
static int ouvrt_device_open_default(OuvrtDevice *dev)
{
	int i;

	for (i = 0; i < 3; i++) {
		if (dev->fds[i] == -1 && dev->devnodes[i] != NULL) {
			dev->fds[i] = open(dev->devnodes[i],
					   O_RDWR | O_NONBLOCK);
			if (dev->fds[i] == -1) {
				g_print("%s: Failed to open '%s': %d (%s)\n",
					dev->name, dev->devnodes[i], errno,
					strerror(errno));
				return -1;
			}
		}
	}

	return 0;
}

/*
 * Closes all file descriptors related to the device.
 */
static void ouvrt_device_close_default(OuvrtDevice *dev)
{
	int i;

	for (i = 2; i >= 0; i--) {
		if (dev->fds[i] != -1)
			close(dev->fds[i]);
		dev->fds[i] = -1;
	}
}

static void ouvrt_device_class_init(OuvrtDeviceClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_device_finalize;
	G_OBJECT_CLASS(klass)->dispose = ouvrt_device_dispose;
	klass->open = ouvrt_device_open_default;
	klass->close = ouvrt_device_close_default;
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
 * Creates or returns an existing stable id for a given serial number.
 */
unsigned long ouvrt_device_claim_id(OuvrtDevice *dev, const char *serial)
{
	unsigned long id;

	if (!serial_to_id_table)
		serial_to_id_table = g_hash_table_new(g_str_hash, g_str_equal);

	if (!g_hash_table_lookup_extended(serial_to_id_table, serial, NULL,
					  (gpointer *)&id)) {
		id = g_hash_table_size(serial_to_id_table);
		g_hash_table_replace(serial_to_id_table, g_strdup(serial),
				     (gpointer)id);
		g_print("%s: acquired new id %lu for serial %s\n", dev->name,
			id, serial);
	} else {
		g_print("%s: reclaimed id %lu for serial %s\n", dev->name, id,
			serial);
	}

	return id;
}

/*
 * Opens the device.
 */
int ouvrt_device_open(OuvrtDevice *dev)
{
	return OUVRT_DEVICE_GET_CLASS(dev)->open(dev);
}

/*
 * Starts the device and its worker thread.
 */
int ouvrt_device_start(OuvrtDevice *dev)
{
	int ret;

	if (dev->active)
		return 0;

	ret = ouvrt_device_open(dev);
	if (ret < 0)
		return ret;

	ret = OUVRT_DEVICE_GET_CLASS(dev)->start(dev);
	if (ret < 0)
		return ret;

	if (dev->serial)
		dev->id = ouvrt_device_claim_id(dev, dev->serial);

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
	OUVRT_DEVICE_GET_CLASS(dev)->close(dev);
}
