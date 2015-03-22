#ifndef __FLICKER_H__
#define __FLICKER_H__

#include <stdbool.h>
#include <stdint.h>

struct flicker;
struct blob;

struct flicker *flicker_new();
void flicker_process(struct flicker *fl, struct blob *blobs, int num_blobs,
		     int skipped);

#endif /* __BLOBWATCH_H__*/
