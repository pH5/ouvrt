#include <stdint.h>
#include <unistd.h>

#include "blobwatch.h"

#define THRESHOLD 0x9f

/*
 * Collects contiguous ranges of pixels with values larger than a threshold of
 * 0x9f in a given scanline and stores them in extents. Processing stops after
 * num_extents.
 *
 * Returns the number of extents found.
 */
static int process_scanline(uint8_t *line, int width,
			    struct extent *extents, int num_extents)
{
	int x, e;

	for (x = 0, e = 0; x < width; x++) {
		/* Loop until pixel value exceeds threshold */
		if (line[x] <= THRESHOLD)
			continue;

		extents[e].start = x++;

		/* Loop until pixel value falls below threshold */
		while (x < width && line[x] > THRESHOLD)
			x++;

		extents[e].end = x - 1;

		if (e == num_extents)
			return e;
		e++;
	}

	return e;
}

/*
 * Collect extents from all scanlines in a frame and stores them in
 * extent_lines.
 */
void process_frame_extents(uint8_t *lines, int width, int height,
			   struct extent_line *extent_lines)
{
	struct extent_line *el = extent_lines;
	int y;

	for (y = 0; y < height; y++) {
		el->num = process_scanline(lines, width,
					   el->extents, MAX_EXTENTS_PER_LINE);
		lines += width;
		el++;
	}
}

/*
 * Finds an extent in the next line that is part of the same blob as the given
 * extent.
 *
 * Returns an extent in the following line that is part of the same blob as the
 * given extent, or NULL.
 */
static struct extent *find_next_extent(struct extent *extent,
				       struct extent_line *el)
{
	struct extent *e;
	int center = (extent->start + extent->end) / 2;

	for (e = el->extents; e < el->extents + el->num; e++) {
		if (e->start < center && e->end > center) {
			/* Mark extent as visited by switching start and end */
			uint16_t tmp = extent->start;

			extent->start = extent->end;
			extent->end = tmp;
			return e;
		}
	}

	return NULL;
}

/*
 * Combines extents across multiple scanlines into blobs and stores them in
 * blobs. Processing stops after num_blobs.
 *
 * Returns the number of blobs found.
 */
int process_extent_blobs(struct extent_line *el, int height,
			 struct blob *blobs, int num_blobs)
{
	struct extent *e, *e2;
	int y, dy, b;

	for (y = 0, b = 0; y < height; y++) {
		for (e = el->extents; e < el->extents + el->num; e++) {
			int width;

			/* Skip already visited extents */
			if (e->end <= e->start)
				continue;

			blobs[b].left = e->start;
			blobs[b].right = e->end;
			blobs[b].top = y;

			for (dy = 1, e2 = e; dy < height - y; dy++) {
				e2 = find_next_extent(e2, el + dy);
				if (!e2)
					break;
				if (e2->start < blobs[b].left)
					blobs[b].left = e2->start;
				if (e2->end > blobs[b].right)
					blobs[b].right = e2->end;
			}

			/* Filter out tall and wide (<= 1:2, >= 2:1) blobs */
			width = blobs[b].right - blobs[b].left + 1;
			if (2 * width <= dy || width >= 2 * dy)
				continue;

			blobs[b].bottom = y + dy - 1;
			blobs[b].x = (blobs[b].left + blobs[b].right) / 2;
			blobs[b].y = (blobs[b].top + blobs[b].bottom) / 2;

			if (b == num_blobs)
				return b;
			b++;
		}
		el++;
	}

	return b;
}
