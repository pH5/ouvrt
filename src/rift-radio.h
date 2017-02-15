/*
 * Oculus Rift CV1 Radio
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __RIFT_RADIO_H__
#define __RIFT_RADIO_H__

#include <unistd.h>
#include <stdbool.h>

struct rift_wireless_device {
	const char *name;
	uint32_t address;
	uint8_t id;
	bool present;
	bool active;
	char firmware_date[11];
	char firmware_version[10];
	char serial[14];
};

struct rift_remote {
	struct rift_wireless_device base;
	uint16_t buttons;
};

struct rift_touch_controller {
	struct rift_wireless_device base;
	uint16_t cap_a_x;
	uint16_t cap_b_y;
	uint16_t cap_rest;
	uint16_t cap_stick;
	uint16_t cap_trigger;
};

struct rift_radio {
	const char *name;
	uint32_t address;
	bool pairing;
	struct rift_remote remote;
	struct rift_touch_controller touch[2];
};

int rift_radio_get_address(int fd, uint32_t *address);
int rift_get_firmware_version(int fd);

void rift_decode_radio_report(struct rift_radio *radio, int fd,
			      const unsigned char *buf, size_t len);
void rift_radio_init(struct rift_radio *radio);

#endif /* __RIFT_RADIO_H__ */
