/*
 * Etron Technology eSP570 webcam controller specific UVC functionality
 * Copyright 2014 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#include <stdint.h>

int esp570_eeprom_read(int fd, uint16_t addr, uint8_t len, char *buf);
int esp570_i2c_read(int fd, uint8_t addr, uint8_t reg, uint16_t *val);
int esp570_i2c_write(int fd, uint8_t addr, uint8_t reg, uint16_t val);
int esp570_setup_unknown_3(int fd);
