/*
 * Windows Mixed Reality Motion Controller
 * Copyright 2018 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __MOTION_CONTROLLER_H__
#define __MOTION_CONTROLLER_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_MOTION_CONTROLLER (ouvrt_motion_controller_get_type())
G_DECLARE_FINAL_TYPE(OuvrtMotionController, ouvrt_motion_controller, \
		     OUVRT, MOTION_CONTROLLER, OuvrtDevice)

OuvrtDevice *motion_controller_new(const char *devnode);

#endif /* __MOTION_CONTROLLER_H__ */
