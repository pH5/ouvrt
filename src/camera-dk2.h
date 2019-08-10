/*
 * Oculus Positional Tracker DK2 camera
 * Copyright 2014-2015 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __CAMERA_DK2_H__
#define __CAMERA_DK2_H__

#include <glib.h>
#include <glib-object.h>

#include "camera-v4l2.h"
#include "device.h"
#include "tracker.h"

G_BEGIN_DECLS

#define OUVRT_TYPE_CAMERA_DK2 (ouvrt_camera_dk2_get_type())
G_DECLARE_FINAL_TYPE(OuvrtCameraDK2, ouvrt_camera_dk2, OUVRT, CAMERA_DK2, \
		     OuvrtCameraV4L2)

OuvrtDevice *camera_dk2_new(const char *devnode);

void ouvrt_camera_dk2_set_tracker(OuvrtCameraDK2 *self, OuvrtTracker *tracker);
void ouvrt_camera_dk2_set_sync_exposure(OuvrtCameraDK2 *self, gboolean sync);

G_END_DECLS

#endif /* __CAMERA_DK2_H__ */
