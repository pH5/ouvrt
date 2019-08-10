/*
 * Oculus Rift HMDs
 * Copyright 2015-2016 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __RIFT_H__
#define __RIFT_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"
#include "tracker.h"

struct tracker;

#define OUVRT_TYPE_RIFT (ouvrt_rift_get_type())
G_DECLARE_FINAL_TYPE(OuvrtRift, ouvrt_rift, OUVRT, RIFT, OuvrtDevice)

OuvrtDevice *rift_dk2_new(const char *devnode);
OuvrtDevice *rift_cv1_new(const char *devnode);

void ouvrt_rift_set_flicker(OuvrtRift *camera, gboolean flicker);
OuvrtTracker *ouvrt_rift_get_tracker(OuvrtRift *rift);

#endif /* __RIFT_H__ */
