/*
 * Camera base class
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#include <glib.h>

#include "camera.h"

G_DEFINE_TYPE(OuvrtCamera, ouvrt_camera, OUVRT_TYPE_DEVICE)

static void ouvrt_camera_class_init(G_GNUC_UNUSED OuvrtCameraClass *klass)
{
}

/*
 * Initializes common fields of the camera structure.
 *
 * Returns 0 on success, negative values on error.
 */
static void ouvrt_camera_init(OuvrtCamera *camera)
{
	camera->dev.type = DEVICE_TYPE_CAMERA;
}
