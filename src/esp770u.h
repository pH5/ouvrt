/*
 * Etron Technology eSP770U webcam controller specific UVC functionality
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __ESP770U_H__
#define __ESP770U_H__

#include <libusb.h>
#include <stdint.h>

int esp770u_flash_read(libusb_device_handle *devh, uint32_t addr,
		       uint8_t *data, uint16_t len);
int esp770u_i2c_read(libusb_device_handle *devh, uint8_t addr, uint16_t reg,
		     uint16_t *val);
int esp770u_i2c_write(libusb_device_handle *devh, uint8_t addr, uint16_t reg,
		      uint16_t val);

int esp770u_query_firmware_version(libusb_device_handle *devh, uint8_t *val);
int esp770u_init_radio(libusb_device_handle *devh);
int esp770u_setup_radio(libusb_device_handle *devh, uint8_t radio_id[5]);
int esp770u_init_unknown(libusb_device_handle *devh);

#endif /* __ESP770U_H__ */
