/*
 * Aptina AR0134 Image Sensor initialization
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __AR0134_H__
#define __AR0134_H__

#include <libusb.h>
#include <stdbool.h>
#include <stdint.h>

int ar0134_init(libusb_device_handle *devh);
int ar0134_set_gain(libusb_device_handle *devh, uint16_t gain);
int ar0134_set_ae(libusb_device_handle *devh, bool enabled);
int ar0134_set_timings(libusb_device_handle *devh, bool tight);
int ar0134_set_sync(libusb_device_handle *devh, bool enabled);

#endif /* __AR0134_H__ */
