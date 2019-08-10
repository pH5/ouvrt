/*
 * HTC Vive Headset Mainboard
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __VIVE_HEADSET_MAINBOARD_H__
#define __VIVE_HEADSET_MAINBOARD_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_HEADSET_MAINBOARD (ouvrt_vive_headset_mainboard_get_type())
G_DECLARE_FINAL_TYPE(OuvrtViveHeadsetMainboard, ouvrt_vive_headset_mainboard, \
		     OUVRT, VIVE_HEADSET_MAINBOARD, OuvrtDevice)

OuvrtDevice *vive_headset_mainboard_new(const char *devnode);

#endif /* __VIVE_HEADSET_MAINBOARD_H__ */
