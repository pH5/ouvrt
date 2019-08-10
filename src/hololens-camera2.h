/*
 * Microsoft HoloLens Sensors (Windows Mixed Reality) stereo camera
 * Copyright 2019 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __HOLOLENS_CAMERA2_H__
#define __HOLOLENS_CAMERA2_H__

#include <glib.h>
#include <glib-object.h>

#include "usb-device.h"

G_BEGIN_DECLS

#define OUVRT_TYPE_HOLOLENS_CAMERA2 (ouvrt_hololens_camera2_get_type())
G_DECLARE_FINAL_TYPE(OuvrtHoloLensCamera2, ouvrt_hololens_camera2, OUVRT, \
		     HOLOLENS_CAMERA2, OuvrtUSBDevice)

OuvrtDevice *hololens_camera2_new(const char *devnode);

G_END_DECLS

#endif /* __HOLOLENS_CAMERA2_H__ */
