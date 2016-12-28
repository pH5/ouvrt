/*
 * Blob detection
 * Copyright 2014-2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __BLOBWATCH_H__
#define __BLOBWATCH_H__

#include <stdbool.h>
#include <stdint.h>

struct leds;

#define MAX_EXTENTS_PER_LINE 11
#define MAX_BLOBS_PER_FRAME  42

struct extent {
	uint16_t start;
	uint16_t end;
	/* inherited parameters */
	uint16_t top;
	uint16_t left;
	uint16_t right;
	uint8_t index;
	uint32_t area;
};

struct extent_line {
	struct extent extents[MAX_EXTENTS_PER_LINE];
	uint16_t num;
	uint16_t padding[3];
};

struct blob {
	/* center of bounding box */
	uint16_t x;
	uint16_t y;
	int16_t vx;
	int16_t vy;
	/* bounding box */
	uint16_t width;
	uint16_t height;
	uint32_t area;
	uint32_t last_area;
	uint32_t age;
	int16_t track_index;
	uint16_t pattern;
	int8_t led_id;
};

/*
 * Stores all blobs observed in a single frame.
 */
struct blobservation {
	int num_blobs;
	struct blob blobs[MAX_BLOBS_PER_FRAME];
	int tracked_blobs;
	uint8_t tracked[MAX_BLOBS_PER_FRAME];
};

struct blobwatch;

struct blobwatch *blobwatch_new(int width, int height);
void blobwatch_process(struct blobwatch *bw, uint8_t *frame,
		       int width, int height, int skipped,
		       struct leds *leds,
		       struct blobservation **output);
void blobwatch_set_flicker(bool enable);

#endif /* __BLOBWATCH_H__*/
