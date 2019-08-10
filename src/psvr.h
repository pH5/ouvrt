/*
 * Sony PlayStation VR Headset
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __PSVR_H__
#define __PSVR_H__

#include <glib.h>
#include <glib-object.h>

#include "usb-device.h"

G_BEGIN_DECLS

#define OUVRT_TYPE_PSVR (ouvrt_psvr_get_type())
G_DECLARE_FINAL_TYPE(OuvrtPSVR, ouvrt_psvr, OUVRT, PSVR, OuvrtUSBDevice)

OuvrtDevice *psvr_new(const char *devnode);

G_END_DECLS

#endif /* __PSVR_H__ */
