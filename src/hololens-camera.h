/*
 * Microsoft HoloLens Sensors (Windows Mixed Reality) stereo camera
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __HOLOLENS_CAMERA_H__
#define __HOLOLENS_CAMERA_H__

#include <glib.h>
#include <glib-object.h>

#include "camera-v4l2.h"
#include "device.h"

G_BEGIN_DECLS

#define OUVRT_TYPE_HOLOLENS_CAMERA (ouvrt_hololens_camera_get_type())
G_DECLARE_FINAL_TYPE(OuvrtHoloLensCamera, ouvrt_hololens_camera, \
		     OUVRT, HOLOLENS_CAMERA, OuvrtCameraV4L2)

OuvrtDevice *hololens_camera_new(const char *devnode);

G_END_DECLS

#endif /* __HOLOLENS_CAMERA_H__ */
