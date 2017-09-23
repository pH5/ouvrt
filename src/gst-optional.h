/*
 * Optional GStreamer integration
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef __GST_OPTIONAL_H__
#define __GST_OPTIONAL_H__
#if HAVE_GST
#include <gst/gst.h>
#else
static inline int gst_init(int *argc, char **argv)
{
	return 0;
}
static inline void gst_deinit() {}
#endif /* HAVE_GST */
#endif /* __GST_OPTIONAL_H__ */
