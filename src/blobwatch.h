#ifndef __BLOBWATCH_H__
#define __BLOBWATCH_H__

#include <stdint.h>

#define MAX_EXTENTS_PER_LINE 11

struct extent {
	uint16_t start;
	uint16_t end;
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
	/* bounding box */
	uint16_t left;
	uint16_t right;
	uint16_t top;
	uint16_t bottom;
};

void process_frame_extents(uint8_t *lines, int width, int height,
			   struct extent_line *extent_lines);
int process_extent_blobs(struct extent_line *el, int height,
			 struct blob *blobs, int num_blobs_per_frame);

#endif /* __BLOBWATCH_H__*/
