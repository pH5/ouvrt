/*
 * HTC Vive Headset
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_HEADSET_H__
#define __VIVE_HEADSET_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

G_BEGIN_DECLS

#define OUVRT_TYPE_VIVE_HEADSET (ouvrt_vive_headset_get_type())
G_DECLARE_FINAL_TYPE(OuvrtViveHeadset, ouvrt_vive_headset, OUVRT, \
		     VIVE_HEADSET, OuvrtDevice)

OuvrtDevice *vive_headset_new(const char *devnode);

#endif /* __VIVE_HEADSET_H__ */
