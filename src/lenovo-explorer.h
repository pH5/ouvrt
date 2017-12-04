/*
 * Lenovo Explorer
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __LENOVO_EXPLORER_H__
#define __LENOVO_EXPLORER_H__

#include <glib.h>
#include <glib-object.h>

#include "device.h"

#define OUVRT_TYPE_LENOVO_EXPLORER (ouvrt_lenovo_explorer_get_type())
G_DECLARE_FINAL_TYPE(OuvrtLenovoExplorer, ouvrt_lenovo_explorer, \
		     OUVRT, LENOVO_EXPLORER, OuvrtDevice)

OuvrtDevice *lenovo_explorer_new(const char *devnode);

#endif /* __LENOVO_EXPLORER_H__ */
