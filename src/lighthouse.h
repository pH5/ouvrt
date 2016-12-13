/*
 * Lighthouse Watchman
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __LIGHTHOUSE_H__
#define __LIGHTHOUSE_H__

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "math.h"

enum pulse_mode {
	SYNC,
	SWEEP
};

struct lighthouse_rotor_calibration {
	float tilt;
	float phase;
	float curve;
	float gibphase;
	float gibmag;
};

struct lighthouse_base_calibration {
	struct lighthouse_rotor_calibration rotor[2];
};

struct lighthouse_base {
	int data_sync;
	int data_word;
	int data_bit;
	uint8_t ootx[40];

	int firmware_version;
	uint32_t serial;
	struct lighthouse_base_calibration calibration;
	vec3 gravity;
	char channel;
	int model_id;
	int reset_count;
};

struct lighthouse_pulse {
	uint32_t timestamp;
	uint16_t duration;
};

struct lighthouse_sensor {
	struct lighthouse_pulse sync;
	struct lighthouse_pulse sweep;
};

struct lighthouse_watchman {
	const gchar *name;
	gboolean base_visible;
	struct lighthouse_base base[2];
	enum pulse_mode mode;
	uint32_t seen_by;
	uint32_t last_timestamp;
	uint16_t duration;
	struct lighthouse_sensor sensor[32];
	struct lighthouse_pulse last_sync;
};

void lighthouse_watchman_handle_pulse(struct lighthouse_watchman *priv,
				      uint8_t id, uint16_t duration,
				      uint32_t timestamp);

#endif
