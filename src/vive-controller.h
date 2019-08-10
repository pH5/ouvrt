/*
 * HTC Vive Controller
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __VIVE_CONTROLLER_H__
#define __VIVE_CONTROLLER_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_VIVE_CONTROLLER (ouvrt_vive_controller_get_type())
G_DECLARE_FINAL_TYPE(OuvrtViveController, ouvrt_vive_controller, OUVRT, \
		     VIVE_CONTROLLER, OuvrtDevice)

OuvrtDevice *vive_controller_new(const char *devnode);

#endif /* __VIVE_CONTROLLER_H__ */
