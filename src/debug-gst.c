#include <gst/gst.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "debug-gst.h"

struct debug_gst {
	GstElement *pipeline;
	GstElement *appsrc;
	GstBuffer *buffer;
	GstMapInfo map;
	bool connected;
};

/*
 * Enables the output of debug frames whenever a GStreamer shmsrc connects to
 * the ouvrtd-gst socket.
 */
static void debug_gst_client_connected(GstElement *sink, gint arg1,
				       gpointer data)
{
	struct debug_gst *gst = data;

	(void)sink;
	(void)arg1;

	gst->connected = true;
	printf("debug: connected\n");
}

/*
 * Disables the output of debug frames whenever a GStreamer shmsrc disconnects
 * from the ouvrtd-gst socket.
 */
static void debug_gst_client_disconnected(GstElement *sink, gint arg1,
					  gpointer data)
{
	struct debug_gst *gst = data;

	(void)sink;
	(void)arg1;

	gst->connected = false;
	printf("debug: disconnected\n");
}

/*
 * Enables GStreamer debug output of BGRx frames into a shmsink.
 */
struct debug_gst *debug_gst_new(int width, int height, int framerate)
{
	struct debug_gst *gst;
	GstElement *pipeline, *src, *sink;
	GstCaps *caps;

	unlink("/tmp/ouvrtd-gst");

	pipeline = gst_pipeline_new(NULL);

	src = gst_element_factory_make("appsrc", "src");
	sink = gst_element_factory_make("shmsink", "sink");
	if (src == NULL || sink == NULL)
		g_error("Could not create elements");

	caps = gst_caps_new_simple("video/x-raw",
		"format", G_TYPE_STRING, "BGRx",
		"framerate", GST_TYPE_FRACTION, framerate, 1,
		"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
		"width", G_TYPE_INT, width,
		"height", G_TYPE_INT, height,
		NULL);

	g_object_set(src, "caps", caps, NULL);
	g_object_set(src, "stream-type", 0, NULL);
	g_object_set(sink, "socket-path", "/tmp/ouvrtd-gst", NULL);

	gst_caps_unref(caps);

	gst_bin_add_many(GST_BIN(pipeline), src, sink, NULL);
	gst_element_link_many(src, sink, NULL);

	gst = malloc(sizeof(*gst));
	if (!gst)
		return NULL;
	gst->pipeline = pipeline;
	gst->appsrc = src;
	gst->buffer = NULL;
	gst->connected = false;

	g_signal_connect(G_OBJECT(sink), "client-connected",
			 G_CALLBACK(debug_gst_client_connected), gst);
	g_signal_connect(G_OBJECT(sink), "client-disconnected",
			 G_CALLBACK(debug_gst_client_disconnected), gst);

	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	return gst;
}

struct debug_gst *debug_gst_unref(struct debug_gst *gst)
{
	gst_element_set_state(gst->pipeline, GST_STATE_NULL);
	if (gst->buffer)
		gst_buffer_unref(gst->buffer);
	gst_object_unref(gst->pipeline);
	free(gst);

	return NULL;
}

static inline uint32_t xrgb(int red, int green, int blue)
{
	return (0xff << 24) | (red << 16) | (green << 8) | blue;
}

/*
 * Allocates and maps a new frame, copies the captured source image into it.
 *
 * Returns a pointer to the pixel data.
 */
uint32_t *debug_gst_frame_new(struct debug_gst *gst, uint8_t *src,
			      int width, int height)
{
	uint32_t *dst;
	int x, y;

	if (!gst->connected || gst->buffer != NULL)
		return NULL;

	gst->buffer = gst_buffer_new_and_alloc(width * height * 4);

	if (!gst_buffer_map(gst->buffer, &gst->map, GST_MAP_WRITE)) {
		gst_buffer_unref(gst->buffer);
		gst->buffer = NULL;
		return NULL;
	}

	dst = (uint32_t *)gst->map.data;
	for (y = 0; y < height; y++, src += width, dst += width) {
		for (x = 0; x < width; x++)
			dst[x] = xrgb(src[x] >> 1, src[x] >> 4, src[x] >> 4);
	}

	return (uint32_t *)gst->map.data;
}

/*
 * Unmaps the frame and releases it into the GStreamer pipeline.
 */
void debug_gst_frame_push(struct debug_gst *gst)
{
	int ret;

	if (gst->buffer == NULL)
		return;

	gst_buffer_unmap(gst->buffer, &gst->map);
	g_signal_emit_by_name(gst->appsrc, "push-buffer", gst->buffer, &ret);
	gst_buffer_unref(gst->buffer);
	gst->buffer = NULL;
}
