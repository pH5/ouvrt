/*
 * GStreamer debug video output
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef __DEBUG_GST_H__
#define __DEBUG_GST_H__

#include "blobwatch.h"
#include "math.h"

struct debug_gst;

#ifdef HAVE_GST
void debug_gst_init(int *argc, char **argv[]);
struct debug_gst *debug_gst_new(int width, int height, int framerate);
struct debug_gst *debug_gst_unref(struct debug_gst *gst);
void debug_gst_frame_push(struct debug_gst *gst, void *frame, size_t size,
			  size_t attach_offset, struct blobservation *ob,
			  dquat *rot, dvec3 *trans, double timestamps[3]);
void debug_gst_deinit(void);
#else
static inline void debug_gst_init(int *argc, char **argv[])
{
}

static inline struct debug_gst *debug_gst_new(int width, int height,
					      int framerate)
{
	return NULL;
}

static inline struct debug_gst *debug_gst_unref(struct debug_gst *gst)
{
	return NULL;
}

static inline void debug_gst_frame_push(struct debug_gst *gst, void *frame,
					size_t size, size_t attach_offset,
					struct blobservation *ob, dquat *rot,
					dvec3 *trans, double timestamps[3])
{
}

static inline void debug_gst_deinit(void)
{
}
#endif /* HAVE_GST */

#endif /* __DEBUG_GST_H__ */
