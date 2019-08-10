/*
 * HTC Vive firmware and hardware version readout
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __VIVE_FIRMWARE_H__
#define __VIVE_FIRMWARE_H__

#include "device.h"

int vive_get_firmware_version(OuvrtDevice *dev);

#endif /* __VIVE_FIRMWARE_H__ */
