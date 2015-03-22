#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blobwatch.h"
#include "debug.h"

#include <stdio.h>

/*
 * LED pattern detector internal state
 */
struct flicker {
	int phase;
	const uint16_t *patterns;
	int num_patterns;
};

/* FIXME */
/*
 * Static copy of the blinking patterns for LEDs 0 to 39 as read from a
 * Rift DK2.
 */
static const uint16_t rift_dk2_patterns[40] = {
	0x001, 0x006, 0x01a, 0x01d, 0x028, 0x02f, 0x033, 0x04b, 0x04c, 0x057,
	0x062, 0x065, 0x079, 0x07e, 0x090, 0x0a4, 0x114, 0x151, 0x183, 0x18c,
	0x199, 0x1aa, 0x1b5, 0x1c0, 0x1cf, 0x1d6, 0x1e9, 0x1f3, 0x1fc, 0x230,
	0x252, 0x282, 0x285, 0x29b, 0x29c, 0x2ae, 0x2b7, 0x2c8, 0x2d1, 0x2e3,
};

/*
 * Allocates and initializes flicker structure.
 *
 * Returns the newly allocated flicker structure.
 */
struct flicker *flicker_new()
{
	struct flicker *fl = malloc(sizeof(*fl));

	if (!fl)
		return NULL;

	memset(fl, 0, sizeof(*fl));
	fl->phase = -1;
	fl->patterns = rift_dk2_patterns;
	fl->num_patterns = 40;

	return fl;
}

static int hamming_distance(uint16_t a, uint16_t b)
{
	uint16_t tmp = a ^ b;
	int distance = 0;
	int bit;

	for (bit = 1 << 9; bit; bit >>= 1)
		if (tmp & bit)
			distance++;

	return distance;
}

static int pattern_find_id(struct flicker *fl, uint16_t pattern, int8_t *id)
{
	int i;

	for (i = 0; i < 40; i++) {
		if (pattern == fl->patterns[i]) {
			*id = i;
			return 2;
		}
		if (hamming_distance(pattern, fl->patterns[i]) < 2) {
			*id = i;
			return 1;
		}
	}

	return -2;
}

static int pattern_get_phase(struct flicker *fl, uint16_t pattern)
{
	int i, j;

	for (i = 1; i < 10; i++) {
		pattern = ((pattern & 0x1ff) << 1) | (pattern >> 9);
		for (j = 0; j < 40; j++)
			if (pattern == fl->patterns[j])
				return i;
	}

	return -1;
}

/*
 * Records blob blinking patterns and compares against the blinking patterns
 * stored in the Rift DK2 to determine the corresponding LED IDs.
 */
void flicker_process(struct flicker *fl, struct blob *blobs, int num_blobs,
		     int skipped)
{
	struct blob *b;
	int success = 0;
	int phase = fl->phase;

	if (skipped) {
		if (skipped == 1) {
			/* Assume state unchanged */
			for (b = blobs; b < blobs + num_blobs; b++) {
				b->pattern = ((b->pattern >> 1) & 0x1ff) |
					     (b->pattern & (1 << 9));
				b->age++;
			}
			if (phase >= 0)
				phase = (phase + 1) % 10;
		} else {
			/* Reset */
			printf("flicker: Skipped %d frames, reset\n", skipped);
			for (b = blobs; b < blobs + num_blobs; b++) {
				b->pattern = 0;
				b->age = 0;
			}
			phase = -1;
		}
	}

	for (b = blobs; b < blobs + num_blobs; b++) {
		uint16_t pattern;

		/* Update pattern only if blob was observed previously */
		if (b->age < 1)
			continue;

		/*
		 * Interpret brightness change of more than 10% as rising
		 * or falling edge. Right shift the pattern and add the
		 * new brightness level as MSB.
		 */
		pattern = (b->pattern >> 1) & 0x1ff;
		if (b->area * 10 > b->last_area * 11)
			pattern |= (1 << 9);
		else if (b->area * 11 < b->last_area * 10)
			pattern |= (0 << 9);
		else
			pattern |= b->pattern & (1 << 9);
		b->pattern = pattern;

		/*
		 * Determine LED ID only if a full pattern was recorded and
		 * consensus about the blinking phase is established
		 */
		if (b->age < 9 || phase < 0)
			continue;

		/* Rotate the pattern bits according to the phase */
		pattern = ((pattern >> (10 - phase)) | (pattern << phase)) &
			  0x3ff;

		success += pattern_find_id(fl, pattern, &b->led_id);
	}

	if (success < 0 || phase < 0) {
		int phase_error[10] = { 0 };
		int max_error = 0;
		int max_phase = 0;
		int i;

		for (b = blobs; b < blobs + num_blobs; b++) {
			int phase = pattern_get_phase(fl, b->pattern);
			if (phase >= 0)
				phase_error[phase]++;
		}

		for (i = 0; i < 10; i++) {
			if (phase_error[i] > max_error) {
				max_error = phase_error[i];
				max_phase = i;
			}
		}

		if (max_error && success < 0 && phase != max_phase)
			printf("too many errors(%d), corrected phase: %d->%d\n",
			       success, phase, max_phase);
		if (max_error)
			phase = max_phase;
	}

	if (phase >= 0)
		fl->phase = (phase + 1) % 10;
}
