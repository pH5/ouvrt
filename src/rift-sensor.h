/*
 * Oculus Rift Sensor (CV1 external camera)
 * Copyright 2017-2018 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __RIFT_SENSOR_H__
#define __RIFT_SENSOR_H__

#include <glib.h>
#include <glib-object.h>

#include "tracker.h"
#include "usb-device.h"

G_BEGIN_DECLS

#define OUVRT_TYPE_RIFT_SENSOR (ouvrt_rift_sensor_get_type())
G_DECLARE_FINAL_TYPE(OuvrtRiftSensor, ouvrt_rift_sensor, OUVRT, RIFT_SENSOR, \
		     OuvrtUSBDevice)

OuvrtDevice *rift_sensor_new(const char *devnode);

void ouvrt_rift_sensor_set_tracker(OuvrtRiftSensor *self, OuvrtTracker *tracker);

G_END_DECLS

#endif /* __RIFT_SENSOR_H__ */
