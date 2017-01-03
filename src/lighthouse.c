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

static unsigned int watchman_id;

static inline float __le16_to_float(__le16 le16)
{
	return f16_to_float(__le16_to_cpu(le16));
}

static inline gboolean pulse_in_this_sync_window(int32_t dt, uint16_t duration)
{
	return dt > -duration && (dt + duration) < (6500 + 250);
}

static inline gboolean pulse_in_next_sync_window(int32_t dt, uint16_t duration)
{
	int32_t dt_end = dt + duration;

	/*
	 * Allow 2000 pulses (40 µs) deviation from the expected interval
	 * between bases, and 1000 pulses (20 µs) for a single base.
	 */
	return (dt > (20000 - 2000) && (dt_end) < (20000 + 6500 + 2000)) ||
	       (dt > (380000 - 2000) && (dt_end) < (380000 + 6500 + 2000)) ||
	       (dt > (400000 - 1000) && (dt_end) < (400000 + 6500 + 1000));
}

static inline gboolean pulse_in_sweep_window(int32_t dt, uint16_t duration)
{
	/*
	 * The J axis (horizontal) sweep starts 71111 ticks after the sync
	 * pulse start (32°) and ends at 346667 ticks (156°).
	 * The K axis (vertical) sweep starts at 55555 ticks (23°) and ends
	 * at 331111 ticks (149°).
	 */
	return dt > (55555 - 1000) && (dt + duration) < (346667 + 1000);
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
lighthouse_base_handle_ootx_data_bit(struct lighthouse_watchman *watchman,
				     struct lighthouse_base *base,
				     gboolean data)
{
	if (base->data_word >= (int)sizeof(base->ootx) / 2) {
		base->data_word = -1;
	} else if (base->data_word >= 0) {
		if (base->data_bit == 16) {
			/* Sync bit */
			base->data_bit = 0;
			if (data) {
				base->data_word++;
				lighthouse_base_handle_ootx_data_word(watchman,
								      base);
			} else {
				g_print("%s: Missed a sync bit, restarting\n",
					watchman->name);
				/* Missing sync bit, restart */
				base->data_word = -1;
			}
		} else if (base->data_bit < 16) {
			/*
			 * Each 16-bit payload word contains two bytes,
			 * transmitted MSB-first.
			 */
			if (data) {
				int idx = 2 * base->data_word +
					  (base->data_bit >> 3);

				base->ootx[idx] |= 0x80 >> (base->data_bit % 8);
			}
			base->data_bit++;
		}
	}

	/* Preamble detection */
	if (data) {
		if (base->data_sync > 16) {
			/* Preamble detected, restart bit capture */
			memset(base->ootx, 0, sizeof(base->ootx));
			base->data_word = 0;
			base->data_bit = 0;
		}
		base->data_sync = 0;
	} else {
		base->data_sync++;
	}
}

static void lighthouse_base_handle_frame(struct lighthouse_watchman *watchman,
					 struct lighthouse_base *base,
					 uint32_t sync_timestamp)
{
	struct lighthouse_frame *frame = &base->frame;

	(void)watchman;

	if (!frame->num_sweeps)
		return;

	frame->frame_duration = sync_timestamp - frame->sync_timestamp;

	frame->sync_timestamp = 0;
	frame->sync_duration = 0;
	frame->num_sweeps = 0;
}

/*
 * The pulse length encodes three bits. The skip bit indicates whether the
 * emitting base will enable the sweeping laser in the next sweep window.
 * The data bit is collected to eventually assemble the OOTX frame. The rotor
 * bit indicates whether the next sweep will be horizontal (0) or vertical (1):
 *
 * duration  3000 3500 4000 4500 5000 5500 6000 6500 (in 48 MHz ticks)
 * skip         0    0    0    0    1    1    1    1
 * data         0    0    1    1    0    0    1    1
 * rotor        0    1    0    1    0    1    0    1
 */
#define SKIP_BIT	4
#define DATA_BIT	2
#define ROTOR_BIT	1

static void lighthouse_handle_sync_pulse(struct lighthouse_watchman *watchman,
					 struct lighthouse_pulse *sync)
{
	struct lighthouse_base *base;
	unsigned char channel;
	int32_t dt;
	unsigned int code;

	if (!sync->duration)
		return;

	if (sync->duration < 2750 || sync->duration > 6750) {
		g_print("%s: Unknown pulse length: %d\n", watchman->name,
			sync->duration);
		return;
	}
	code = (sync->duration - 2750) / 500;

	dt = sync->timestamp - watchman->last_timestamp;

	/* 48 MHz / 120 Hz = 400000 cycles per sync pulse */
	if (dt > (400000 - 1000) && dt < (400000 + 1000)) {
		/* Observing a single base station, channel A (or B, actually) */
		channel = 'A';
	} else if (dt > (380000 - 1000) && dt < (380000 + 1000)) {
		/* Observing two base stations, this is channel B */
		channel = 'B';
	} else if (dt > (20000 - 1000) && dt < (20000 + 1000)) {
		/* Observing two base stations, this is channel C */
		channel = 'C';
	} else {
		if (dt > -1000 && dt < 1000) {
			/*
			 * Ignore, this means we prematurely finished
			 * assembling the last sync pulse.
			 */
		} else {
			/* Irregular sync pulse */
			if (watchman->last_timestamp)
				g_print("%s: Irregular sync pulse: %08x -> %08x (%+d)\n",
					watchman->name, watchman->last_timestamp,
					sync->timestamp, dt);
			lighthouse_base_reset(&watchman->base[0]);
			lighthouse_base_reset(&watchman->base[1]);
		}

		watchman->last_timestamp = sync->timestamp;
		return;
	}

	base = &watchman->base[channel == 'C'];
	base->channel = channel;
	base->last_sync_timestamp = sync->timestamp;
	lighthouse_base_handle_ootx_data_bit(watchman, base, (code & DATA_BIT));
	lighthouse_base_handle_frame(watchman, base, sync->timestamp);

	base->active_rotor = (code & ROTOR_BIT);
	if (!(code & SKIP_BIT)) {
		watchman->active_base = base;
		base->frame.sync_timestamp = sync->timestamp;
		base->frame.sync_duration = sync->duration;
	}

	watchman->last_timestamp = sync->timestamp;
}

static void lighthouse_handle_sweep_pulse(struct lighthouse_watchman *watchman,
					  uint8_t id, uint32_t timestamp,
					  uint16_t duration)
{
	struct lighthouse_base *base = watchman->active_base;
	struct lighthouse_frame *frame = &base->frame;
	int32_t offset;

	(void)id;

	if (!base) {
		g_print("%s: sweep without sync\n", watchman->name);
		return;
	}

	offset = timestamp - base->last_sync_timestamp;

	/* Ignore short sync pulses or sweeps without a corresponding sync */
	if (offset > 379000)
		return;

	if (!pulse_in_sweep_window(offset, duration)) {
		g_print("%s: sweep offset out of range: rotor %u offset %u duration %u\n",
			watchman->name, base->active_rotor, offset, duration);
		return;
	}

	if (frame->num_sweeps == 32) {
		g_print("%s: frame already contains 32 sweep pulses\n",
			watchman->name);
		return;
	}

	frame->sweep_duration[frame->num_sweeps] = duration;
	frame->sweep_offset[frame->num_sweeps] = offset;
	frame->sweep_id[frame->num_sweeps] = id;
	frame->num_sweeps++;
}

static void accumulate_sync_pulse(struct lighthouse_watchman *watchman,
				  uint8_t id, uint32_t timestamp,
				  uint16_t duration)
{
	int32_t dt = timestamp - watchman->last_sync.timestamp;

	if (dt > watchman->last_sync.duration || watchman->last_sync.duration == 0) {
		watchman->seen_by = 1 << id;
		watchman->last_sync.timestamp = timestamp;
		watchman->last_sync.duration = duration;
		watchman->last_sync.id = id;
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
}

void lighthouse_watchman_handle_pulse(struct lighthouse_watchman *watchman,
				      uint8_t id, uint16_t duration,
				      uint32_t timestamp)
{
	int32_t dt;

	dt = timestamp - watchman->last_sync.timestamp;

	if (watchman->sync_lock) {
		if (watchman->seen_by && dt > watchman->last_sync.duration) {
			lighthouse_handle_sync_pulse(watchman, &watchman->last_sync);
			watchman->seen_by = 0;
		}

		if (pulse_in_this_sync_window(dt, duration) ||
		    pulse_in_next_sync_window(dt, duration)) {
			accumulate_sync_pulse(watchman, id, timestamp, duration);
		} else if (pulse_in_sweep_window(dt, duration)) {
			lighthouse_handle_sweep_pulse(watchman, id, timestamp,
						      duration);
		} else {
			/*
			 * Spurious pulse - this could be due to a reflection or
			 * misdetected sync. If dt > period, drop the sync lock.
			 * Maybe we should ignore a single missed sync.
			 */
			if (dt > 407500) {
				watchman->sync_lock = FALSE;
				g_print("%s: late pulse, lost sync\n",
					watchman->name);
			} else {
				g_print("%s: spurious pulse: %08x (%02x %d %u)\n",
					watchman->name, timestamp, id, dt,
					duration);
			}
			watchman->seen_by = 0;
		}
	} else {
		/*
		 * If we've not locked onto the periodic sync signals, try to
		 * treat all pulses within the right duration range as potential
		 * sync pulses.
		 * This is still a bit naive. If the sensors are moved too
		 * close to the lighthouse base station, sweep pulse durations
		 * may fall into this range and sweeps may be misdetected as
		 * sync floods.
		 */
		if (duration >= 2750 && duration <= 6750) {
			/*
			 * Decide we've locked on if the pulse falls into any
			 * of the expected time windows from the last
			 * accumulated sync pulse.
			 */
			if (pulse_in_next_sync_window(dt, duration)) {
				g_print("%s: sync locked\n", watchman->name);
				watchman->sync_lock = TRUE;
			}

			accumulate_sync_pulse(watchman, id, timestamp, duration);
		} else {
			/* Assume this is a sweep, ignore it until we lock */
		}
	}
}

void lighthouse_watchman_init(struct lighthouse_watchman *watchman)
{
	watchman->id = watchman_id++;
	watchman->name = NULL;
	watchman->seen_by = 0;
	watchman->last_timestamp = 0;
	watchman->last_sync.timestamp = 0;
	watchman->last_sync.duration = 0;
}
