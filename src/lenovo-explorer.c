/*
 * Lenovo Explorer
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "lenovo-explorer.h"
#include "device.h"
#include "hidraw.h"

struct _OuvrtLenovoExplorer {
	OuvrtDevice dev;

	bool proximity;
};

G_DEFINE_TYPE(OuvrtLenovoExplorer, ouvrt_lenovo_explorer, OUVRT_TYPE_DEVICE)

/*
 * Opens the Lenovo Explorer device, powers it on, and reads device info.
 */
static int lenovo_explorer_start(G_GNUC_UNUSED OuvrtDevice *dev)
{
	return 0;
}

/*
 * Handles Lenovo Explorer messages.
 */
static void lenovo_explorer_thread(OuvrtDevice *dev)
{
	OuvrtLenovoExplorer *self = OUVRT_LENOVO_EXPLORER(dev);
	unsigned char buf[64];
	struct pollfd fds;
	int ret;

	while (dev->active) {
		fds.fd = dev->fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		if (ret == -1) {
			g_print("%s: Poll failure: %d\n", dev->name, errno);
			continue;
		}

		if (ret == 0)
			continue;

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			dev->active = FALSE;
			break;
		}

		if (!(fds.revents & POLLIN)) {
			g_print("%s: Unhandled poll event: 0x%x\n", dev->name,
				fds.revents);
			continue;
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("%s: Read error: %d\n", dev->name, errno);
			continue;
		}
		if (ret != 2 || buf[0] != 0x01) {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
			continue;
		}

		self->proximity = buf[1];
		g_print("%s: Proximity: %d\n", dev->name, buf[1]);
	}
}

/*
 * Powers off the Lenovo Explorer device.
 */
static void lenovo_explorer_stop(G_GNUC_UNUSED OuvrtDevice *dev)
{
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_lenovo_explorer_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_lenovo_explorer_parent_class)->finalize(object);
}

static void
ouvrt_lenovo_explorer_class_init(OuvrtLenovoExplorerClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_lenovo_explorer_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = lenovo_explorer_start;
	OUVRT_DEVICE_CLASS(klass)->thread = lenovo_explorer_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = lenovo_explorer_stop;
}

static void ouvrt_lenovo_explorer_init(OuvrtLenovoExplorer *self)
{
	self->dev.type = DEVICE_TYPE_HMD;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Lenovo Explorer device.
 */
OuvrtDevice *lenovo_explorer_new(G_GNUC_UNUSED const char *devnode)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_LENOVO_EXPLORER,
					 NULL));
}
