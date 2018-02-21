/*
 * Debug output helpers
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdint.h>

#define DEBUG_MODE_SHM	1
#define DEBUG_MODE_X	2
#define DEBUG_MODE_PNG	3

#include "blobwatch.h"
#include "imu.h"
#include "maths.h"

extern int debug_mode;

struct debug_stream;

struct ouvrt_debug_attachment {
	struct blobservation blobservation;
	dquat rot;
	dvec3 trans;
	int num_imu_samples;
	struct imu_state imu_samples[32];
	double timestamps[4];
};

int debug_parse_arg(const char *arg);

unsigned int debug_imu_fifo_in(struct imu_state *samples, unsigned int n);
unsigned int debug_imu_fifo_out(struct imu_state *samples, unsigned int n);

#ifdef HAVE_DEBUG_STREAM
void debug_stream_init(int *argc, char **argv[]);
struct debug_stream *debug_stream_new(int width, int height, int framerate);
struct debug_stream *debug_stream_unref(struct debug_stream *gst);
void debug_stream_frame_push(struct debug_stream *stream,
			     void *frame, size_t size, size_t attach_offset,
			     struct blobservation *ob, dquat *rot,
			     dvec3 *trans, double timestamps[3]);
void debug_stream_deinit(void);
#else
static inline void debug_stream_init(int *argc, char **argv[])
{
}

static inline struct debug_stream *debug_stream_new(int width, int height,
					      int framerate)
{
	return NULL;
}

static inline struct debug_stream *debug_stream_unref(struct debug_stream *gst)
{
	return NULL;
}

static inline void debug_stream_frame_push(struct debug_stream *stream,
					   void *frame, size_t size,
					   size_t attach_offset,
					   struct blobservation *ob, dquat *rot,
					   dvec3 *trans, double timestamps[3])
{
}

static inline void debug_stream_deinit(void)
{
}
#endif /* HAVE_DEBUG_STREAM */

#endif /* __DEBUG_H__ */
