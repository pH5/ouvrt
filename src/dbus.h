/*
 * D-Bus interface implementation
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef __DBUS_H__
#define __DBUS_H__

#include "device.h"

guint ouvrt_dbus_own_name(void);

void ouvrt_dbus_export_device(OuvrtDevice *dev);
void ouvrt_dbus_unexport_device(OuvrtDevice *dev);

#endif /* __DBUS_H__ */
