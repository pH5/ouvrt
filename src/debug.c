#include <stdint.h>
#include <stdlib.h>

#include "blobwatch.h"

#define COLOR_GREEN	0xff0fff0f
#define COLOR_YELLOW	0xffffff0f

/*
 * Draws a rectangle which must be completely contained in the frame.
 */
static void debug_draw_rect(uint32_t *frame, int width,
			    int left, int top, int right, int bottom,
			    uint32_t color)
{
	uint32_t *line;
	int x, y;

	line = frame + width * top;
	for (x = left; x <= right; x++)
		line[x] = color;
	for (y = top + 1; y < bottom; y++) {
		line = frame + width * y;
		if (left >= 0)
			line[left] = color;
		if (right < width)
			line[right] = color;
	}
	line = frame + width * bottom;
	for (x = left; x <= right; x++)
		line[x] = color;
}

/*
 * Draws a yellow pixel at the start and end of each extent.
 */
void debug_draw_extents(uint32_t *frame, int width, int height,
			struct extent_line *el)
{
	struct extent *e;
	uint32_t *line;
	int y;

	if (frame == NULL)
		return;

	line = frame;
	for (y = 0; y < height; y++) {
		for (e = el->extents; e < el->extents + el->num; e++) {
			line[e->start] = COLOR_YELLOW;
			line[e->end] = COLOR_YELLOW;
		}

		line += width;
		el++;
	}
}

/*
 * Draws a green bounding box for each blob.
 */
void debug_draw_blobs(uint32_t *frame, int width, int height,
		      struct blob *blobs, int num_blobs)
{
	struct blob *b;

	if (frame == NULL)
		return;

	(void)height;
	for (b = blobs; b < blobs + num_blobs; b++) {
		debug_draw_rect(frame, width, b->left, b->top,
				b->right, b->bottom, COLOR_GREEN);
	}
}
