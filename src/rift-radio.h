/*
 * Oculus Rift CV1 Radio
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __RIFT_RADIO_H__
#define __RIFT_RADIO_H__

#include <unistd.h>
#include <stdbool.h>

#include "imu.h"
#include "tracking-model.h"

struct rift_wireless_device {
	unsigned long dev_id;
	const char *name;
	uint32_t address;
	uint8_t id;
	bool present;
	bool active;
	char firmware_date[11+1];
	char firmware_version[10+1];
	char serial[14+1];
};

struct rift_remote {
	struct rift_wireless_device base;
	uint16_t buttons;
};

struct rift_touch_calibration {
	vec3 imu_position;
	float gyro_calibration[12];
	float acc_calibration[12];
	uint16_t joy_x_range_min;
	uint16_t joy_x_range_max;
	uint16_t joy_x_dead_min;
	uint16_t joy_x_dead_max;
	uint16_t joy_y_range_min;
	uint16_t joy_y_range_max;
	uint16_t joy_y_dead_min;
	uint16_t joy_y_dead_max;
	uint16_t trigger_min_range;
	uint16_t trigger_mid_range;
	uint16_t trigger_max_range;
	uint16_t middle_min_range;
	uint16_t middle_mid_range;
	uint16_t middle_max_range;
	bool middle_flipped;
	uint16_t cap_sense_min[8];
	uint16_t cap_sense_touch[8];
};

struct rift_touch_controller {
	struct rift_wireless_device base;
	struct rift_touch_calibration calibration;
	struct tracking_model model;
	struct imu_state imu;
	uint32_t last_timestamp;
	float trigger;
	float grip;
	float stick[2];
	float cap_a_x;
	float cap_b_y;
	float cap_rest;
	float cap_stick;
	float cap_trigger;
	uint8_t haptic_counter;
	uint8_t buttons;
};

struct rift_radio {
	const char *name;
	uint8_t address[5];
	bool pairing;
	struct rift_remote remote;
	struct rift_touch_controller touch[2];
};

int rift_radio_get_address(int fd, uint8_t address[5]);
int rift_get_firmware_version(int fd);

void rift_decode_radio_report(struct rift_radio *radio, int fd,
			      const unsigned char *buf, size_t len);
void rift_radio_init(struct rift_radio *radio);

#endif /* __RIFT_RADIO_H__ */
