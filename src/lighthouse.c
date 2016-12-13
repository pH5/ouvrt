/*
 * Lighthouse Watchman
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <glib.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>
#include "lighthouse.h"
#include "math.h"

struct lighthouse_sync_pulse {
	uint16_t duration;
	gboolean skip;
	gboolean rotor;
	gboolean data;
};

static struct lighthouse_sync_pulse pulse_table[8] = {
	{ 3000, 0, 0, 0 },
	{ 3500, 0, 1, 0 },
	{ 4000, 0, 0, 1 },
	{ 4500, 0, 1, 1 },
	{ 5000, 1, 0, 0 },
	{ 5500, 1, 1, 0 },
	{ 6000, 1, 0, 1 },
	{ 6500, 1, 1, 1 },
};

struct lighthouse_ootx_report {
	__le16 version;
	__le32 serial;
	__le16 phase[2];
	__le16 tilt[2];
	__u8 reset_count;
	__u8 model_id;
	__le16 curve[2];
	__s8 gravity[3];
	__le16 gibphase[2];
	__le16 gibmag[2];
} __attribute__((packed));

static inline float __le16_to_float(__le16 le16)
{
	return f16_to_float(__le16_to_cpu(le16));
}

static void lighthouse_base_handle_ootx_frame(struct lighthouse_base *base)
{
	struct lighthouse_ootx_report *report = (void *)(base->ootx + 2);
	uint16_t len = __le16_to_cpup((__le16 *)base->ootx);
	uint32_t crc = crc32(0L, Z_NULL, 0);
	gboolean serial_changed = FALSE;
	uint32_t ootx_crc;
	uint16_t version;
	int ootx_version;
	vec3 gravity;
	int i;

	if (len != 33) {
		g_print("Lighthouse Base %X: unexpected OOTX payload length: %d\n",
			base->serial, len);
		return;
	}

	ootx_crc = __le32_to_cpup((__le32 *)(base->ootx + 36)); /* (len+3)/4*4 */
	crc = crc32(crc, base->ootx + 2, 33);
	if (ootx_crc != crc) {
		g_print("Lighthouse Base %X: CRC error: %08x != %08x\n",
			base->serial, crc, ootx_crc);
		return;
	}

	version = __le16_to_cpu(report->version);
	ootx_version = version & 0x3f;
	if (ootx_version != 6) {
		g_print("Lighthouse Base %X: unexpected OOTX frame version: %d\n",
			base->serial, ootx_version);
		return;
	}

	base->firmware_version = version >> 6;

	if (base->serial != __le32_to_cpu(report->serial)) {
		base->serial = __le32_to_cpu(report->serial);
		serial_changed = TRUE;
	}

	for (i = 0; i < 2; i++) {
		struct lighthouse_rotor_calibration *rotor;

		rotor = &base->calibration.rotor[i];
		rotor->tilt = __le16_to_float(report->tilt[i]);
		rotor->phase = __le16_to_float(report->phase[i]);
		rotor->curve = __le16_to_float(report->curve[i]);
		rotor->gibphase = __le16_to_float(report->gibphase[i]);
		rotor->gibmag = __le16_to_float(report->gibmag[i]);
	}

	base->model_id = report->model_id;

	if (serial_changed) {
		g_print("Lighthouse Base %X: firmware version: %d, model id: %d, channel: %c\n",
			base->serial, base->firmware_version, base->model_id,
			base->channel);

		for (i = 0; i < 2; i++) {
			struct lighthouse_rotor_calibration *rotor;

			rotor = &base->calibration.rotor[i];

			g_print("Lighthouse Base %X: rotor %d: [ %12.9f %12.9f %12.9f %12.9f %12.9f ]\n",
				base->serial, i, rotor->tilt, rotor->phase,
				rotor->curve, rotor->gibphase, rotor->gibmag);
		}
	}

	gravity.x = report->gravity[0];
	gravity.y = report->gravity[1];
	gravity.z = report->gravity[2];
	vec3_normalize(&gravity);
	if (gravity.x != base->gravity.x ||
	    gravity.y != base->gravity.y ||
	    gravity.z != base->gravity.z) {
		base->gravity = gravity;
		g_print("Lighthouse Base %X: gravity: [ %9.6f %9.6f %9.6f ]\n",
			base->serial, gravity.x, gravity.y, gravity.z);
	}

	if (base->reset_count != report->reset_count) {
		base->reset_count = report->reset_count;
		g_print("Lighthouse Base %X: reset count: %d\n", base->serial,
			base->reset_count);
	}
}

static void lighthouse_base_reset(struct lighthouse_base *base)
{
	base->data_sync = 0;
	base->data_word = -1;
	base->data_bit = 0;
	memset(base->ootx, 0, sizeof(base->ootx));
}

static void
lighthouse_base_handle_ootx_data_word(struct lighthouse_watchman *watchman,
				      struct lighthouse_base *base)
{
	uint16_t len = __le16_to_cpup((__le16 *)base->ootx);

	/* After 4 OOTX words we have received the base station serial number */
	if (base->data_word == 4) {
		struct lighthouse_ootx_report *report = (void *)(base->ootx + 2);
		uint16_t ootx_version = __le16_to_cpu(report->version) & 0x3f;
		uint32_t serial = __le32_to_cpu(report->serial);

		if (len != 33) {
			g_print("%s: unexpected OOTX frame length %d\n",
				watchman->name, len);
			return;
		}

		if (ootx_version == 6 && serial != base->serial) {
			g_print("%s: spotted Lighthouse Base %X\n",
				watchman->name, serial);
		}
	}
	if (len == 33 && base->data_word == 20) { /* (len + 3)/4 * 2 + 2 */
		lighthouse_base_handle_ootx_frame(base);
	}
}

static void
lighthouse_base_handle_sync_pulse(struct lighthouse_watchman *watchman,
				  struct lighthouse_sync_pulse *pulse,
				  char channel)
{
	struct lighthouse_base *base = &watchman->base[channel == 'C'];

	base->channel = channel;

	if (base->data_word >= (int)sizeof(base->ootx) / 2)
		base->data_word = -1;
	if (base->data_word >= 0) {
		if (base->data_bit == 16) {
			/* Sync bit */
			base->data_bit = 0;
			if (pulse->data) {
				base->data_word++;
				lighthouse_base_handle_ootx_data_word(watchman,
								      base);
			} else {
				g_print("%s: Missed a sync bit, restarting\n",
					watchman->name);
				/* Missing sync bit, restart */
				base->data_word = -1;
			}
		} else if (base->data_bit < 8) {
			base->ootx[2 * base->data_word] |=
					pulse->data << (7 - base->data_bit);
			base->data_bit++;
		} else if (base->data_bit < 16) {
			base->ootx[2 * base->data_word + 1] |=
					pulse->data << (15 - base->data_bit);
			base->data_bit++;
		}
	}

	/* Preamble detection */
	if (base->data_sync > 16 && pulse->data == 1) {
		memset(base->ootx, 0, sizeof(base->ootx));
		base->data_word = 0;
		base->data_bit = 0;
	}
	if (pulse->data)
		base->data_sync = 0;
	else
		base->data_sync++;
}

static void lighthouse_handle_sync_pulse(struct lighthouse_watchman *watchman,
					 struct lighthouse_pulse *sync)
{
	struct lighthouse_sync_pulse *pulse;
	int32_t dt;
	int i;

	if (!sync->duration)
		return;

	for (i = 0, pulse = pulse_table; i < 8; i++, pulse++) {
		if (sync->duration > (pulse->duration - 250) &&
		    sync->duration < (pulse->duration + 250)) {
			break;
		}
	}
	if (i == 8) {
		g_print("%s: Unknown pulse length: %d\n", watchman->name,
			sync->duration);
		return;
	}

	dt = sync->timestamp - watchman->last_timestamp;

	/* 48 MHz / 120 Hz = 400000 cycles per sync pulse */
	if (dt > (400000 - 4000) && dt < (400000 + 4000)) {
		/* Observing a single base station, channel A */
		lighthouse_base_handle_sync_pulse(watchman, pulse, 'A');
	} else if (dt > (380000 - 4000) && dt < (380000 + 4000)) {
		/* Observing two base stations, this is channel B */
		lighthouse_base_handle_sync_pulse(watchman, pulse, 'B');
	} else if (dt > (20000 - 4000) && dt < (20000 + 4000)) {
		/* Observing two base stations, this is channel C */
		lighthouse_base_handle_sync_pulse(watchman, pulse, 'C');
	} else if (dt > -1000 && dt < 1000) {
		/* Ignore */
	} else {
		/* Irregular sync pulse */
		if (watchman->last_timestamp)
			g_print("%s: Irregular sync pulse: %u -> %u (%+d)\n",
				watchman->name, watchman->last_timestamp,
				sync->timestamp, dt);
		lighthouse_base_reset(&watchman->base[0]);
		lighthouse_base_reset(&watchman->base[1]);
	}

	watchman->last_timestamp = sync->timestamp;
}

void lighthouse_watchman_handle_pulse(struct lighthouse_watchman *watchman,
				      uint8_t id, uint16_t duration,
				      uint32_t timestamp)
{
	int32_t dt;

	dt = timestamp - watchman->last_sync.timestamp;

	if (watchman->mode == SYNC && watchman->seen_by && dt > watchman->last_sync.duration) {
		lighthouse_handle_sync_pulse(watchman, &watchman->last_sync);
		watchman->seen_by = 0;
	}

	if (duration >= 2750) {
		if (dt > watchman->last_sync.duration || watchman->last_sync.duration == 0) {
			watchman->seen_by = 1 << id;
			watchman->last_sync.timestamp = timestamp;
			watchman->last_sync.duration = duration;
		} else {
			watchman->seen_by |= 1 << id;
			if (timestamp < watchman->last_sync.timestamp) {
				watchman->last_sync.duration += watchman->last_sync.timestamp - timestamp;
				watchman->last_sync.timestamp = timestamp;
			}
			if (duration > watchman->last_sync.duration)
				watchman->last_sync.duration = duration;
			watchman->last_sync.duration = duration;
		}
		watchman->mode = SYNC;
	} else {
		watchman->mode = SWEEP;
	}
}
