#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blobwatch.h"
#include "debug.h"
#include "leds.h"

#include <stdio.h>

/*
 * LED pattern detector internal state
 */
struct flicker {
	int phase;
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

static int pattern_find_id(uint16_t *patterns, int num_patterns,
			   uint16_t pattern, int8_t *id)
{
	int i;

	for (i = 0; i < num_patterns; i++) {
		if (pattern == patterns[i]) {
			*id = i;
			return 2;
		}
		if (hamming_distance(pattern, patterns[i]) < 2) {
			*id = i;
			return 1;
		}
	}

	return -2;
}

static int pattern_get_phase(uint16_t *patterns, int num_patterns, uint16_t pattern)
{
	int i, j;

	for (i = 1; i < 10; i++) {
		pattern = ((pattern & 0x1ff) << 1) | (pattern >> 9);
		for (j = 0; j < num_patterns; j++)
			if (pattern == patterns[j])
				return i;
	}

	return -1;
}

/*
 * Records blob blinking patterns and compares against the blinking patterns
 * stored in the Rift DK2 to determine the corresponding LED IDs.
 */
void flicker_process(struct flicker *fl, struct blob *blobs, int num_blobs,
		     int skipped, struct leds *leds)
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

		success += pattern_find_id(leds->patterns, leds->num, pattern,
					   &b->led_id);
	}

	if (success < 0 || phase < 0) {
		int phase_error[10] = { 0 };
		int max_error = 0;
		int max_phase = 0;
		int i;

		for (b = blobs; b < blobs + num_blobs; b++) {
			int phase = pattern_get_phase(leds->patterns, leds->num,
						      b->pattern);
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
