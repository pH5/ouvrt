/*
 * PipeWire integration and debug streams
 * Copyright 2019 Philipp Zabel
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <glib.h>

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>

#include <spa/param/format-utils.h>
#include <spa/param/props.h>
#include <spa/param/video/format-utils.h>
#include <spa/support/type-map.h>

#include <pipewire/pipewire.h>

#include "debug.h"

struct type {
	struct spa_type_media_type media_type;
	struct spa_type_media_subtype media_subtype;
	struct spa_type_format_video format_video;
	struct spa_type_video_format video_format;
};

static inline void init_type(struct type *type, struct spa_type_map *map)
{
	spa_type_media_type_map(map, &type->media_type);
	spa_type_media_subtype_map(map, &type->media_subtype);
	spa_type_format_video_map(map, &type->format_video);
	spa_type_video_format_map(map, &type->video_format);
}

struct data;

struct debug_stream {
	struct data *data;
	struct pw_stream *stream;
	struct spa_hook listener;
	struct spa_video_info_raw format;
	enum pw_stream_state state;
	int32_t stride;
	uint32_t seq;
};

struct data {
	struct type type;

	struct pw_loop *loop;
	struct pw_thread_loop *main_loop;

	struct debug_stream dummy;

	struct pw_core *core;
	struct pw_type *t;
	struct pw_remote *remote;
	struct spa_hook remote_listener;

	GThread *thread;
};

static void *global_data;

static void on_stream_state_changed(void *data,
				    G_GNUC_UNUSED enum pw_stream_state old,
				    enum pw_stream_state state,
				    G_GNUC_UNUSED const char *error)
{
	struct debug_stream *stream = data;

	stream->state = state;
}

static void
on_stream_format_changed(void *_data, const struct spa_pod *format)
{
	struct debug_stream *stream = _data;
	struct data *data = stream->data;
	struct pw_type *t = data->t;
	uint8_t params_buffer[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(params_buffer,
							sizeof(params_buffer));
	const struct spa_pod *params[2];

	if (format == NULL) {
		pw_stream_finish_format(stream->stream, 0, NULL, 0);
		return;
	}
	spa_format_video_raw_parse(format, &stream->format,
				   &data->type.format_video);

	if (stream->format.format == data->type.video_format.GRAY8) {
		stream->stride = SPA_ROUND_UP_N(stream->format.size.width, 4);
	} else if (stream->format.format == data->type.video_format.YUY2) {
		stream->stride = SPA_ROUND_UP_N(stream->format.size.width, 2) *
						2;
	} else if (stream->format.format == data->type.video_format.RGBx) {
		stream->stride = stream->format.size.width * 4;
	} else {
		pw_stream_finish_format(stream->stream, 0, NULL, 0);
		return;
	}

	params[0] = spa_pod_builder_object(&b,
		t->param.idBuffers, t->param_buffers.Buffers,
		":", t->param_buffers.size,	"i", stream->stride *
						     stream->format.size.height,
		":", t->param_buffers.stride,	"i", stream->stride,
		":", t->param_buffers.buffers,	"iru", 2,
						SPA_POD_PROP_MIN_MAX(1, 32),
		":", t->param_buffers.align,	"i", 16);

	params[1] = spa_pod_builder_object(&b,
		t->param.idMeta, t->param_meta.Meta,
		":", t->param_meta.type, "I", t->meta.Header,
		":", t->param_meta.size, "i", sizeof(struct spa_meta_header));

	pw_stream_finish_format(stream->stream, 0, params, 2);
}

static const struct pw_stream_events stream_events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = on_stream_state_changed,
	.format_changed = on_stream_format_changed,
};

static inline uint32_t format_to_pipewire(struct data *data, uint32_t format)
{
	switch (format) {
	case FORMAT_GRAY:
		return data->type.video_format.GRAY8;
	case FORMAT_YUYV:
		return data->type.video_format.YUY2;
	case FORMAT_RGBX:
		return data->type.video_format.RGBx;
	default:
		return data->type.video_format.UNKNOWN;
	}
}

struct debug_stream *debug_stream_new(const struct debug_stream_desc *desc)
{
	struct data *data = global_data;
	enum pw_remote_state state;
	struct debug_stream *stream;
	const struct spa_pod *params[1];
	uint8_t buffer[1024];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

	if (!data) {
		g_print("PipeWire: can't create debug stream, no data\n");
		return NULL;
	}

	pw_thread_loop_lock(data->main_loop);

	state = pw_remote_get_state(data->remote, NULL);
	if (state != PW_REMOTE_STATE_CONNECTED) {
		g_print("PipeWire: can't create debug stream, not connected\n");
		pw_thread_loop_unlock(data->main_loop);
		return NULL;
	}

	stream = calloc(sizeof(*stream), 1);
	stream->data = data;
	stream->stream = pw_stream_new(data->remote, "ouvrt-camera",
				pw_properties_new(
					"media.class", "Video/Source",
					PW_NODE_PROP_MEDIA, "Video",
					PW_NODE_PROP_CATEGORY, "Capture",
					PW_NODE_PROP_ROLE, "Camera",
					NULL));

	params[0] = spa_pod_builder_object(&b,
		data->t->param.idEnumFormat, data->t->spa_format,
		"I", data->type.media_type.video,
		"I", data->type.media_subtype.raw,
		":", data->type.format_video.format,
			"I", format_to_pipewire(data, desc->format),
		":", data->type.format_video.size,
			"R", &SPA_RECTANGLE(desc->width, desc->height),
		":", data->type.format_video.framerate,
			"F", &SPA_FRACTION(desc->framerate.numerator,
					   desc->framerate.denominator));

	pw_stream_add_listener(stream->stream, &stream->listener,
			       &stream_events, stream);
	pw_stream_connect(stream->stream, PW_DIRECTION_OUTPUT, NULL,
			  PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_MAP_BUFFERS,
			  params, 1);

	pw_thread_loop_unlock(data->main_loop);

	return stream;
}

struct debug_stream *debug_stream_unref(struct debug_stream *stream)
{
	if (!stream)
		return NULL;

	pw_thread_loop_lock(stream->data->main_loop);
	pw_stream_disconnect(stream->stream);
	pw_stream_destroy(stream->stream);
	pw_thread_loop_unlock(stream->data->main_loop);

	free(stream);

	return NULL;
}

bool debug_stream_connected(struct debug_stream *stream)
{
	return stream && stream->state == PW_STREAM_STATE_STREAMING;
}

void debug_stream_frame_push(struct debug_stream *stream, void *src,
			     size_t size, size_t attach_offset,
			     struct blobservation *ob, dquat *rot, dvec3 *trans,
			     double timestamps[3])
{
	struct spa_meta_header *h;
	struct pw_buffer *buf;
	struct spa_buffer *b;

	(void)size;
	(void)attach_offset;
	(void)ob;
	(void)rot;
	(void)trans;
	(void)timestamps;

	if (!stream || !debug_stream_connected(stream))
		return;

	buf = pw_stream_dequeue_buffer(stream->stream);
	if (buf == NULL)
		return;

	b = buf->buffer;

	if (!b->datas[0].data)
		goto done;

	if ((h = spa_buffer_find_meta(b, stream->data->t->meta.Header))) {
		h->pts = -1;
		h->flags = 0;
		h->seq = stream->seq++;
		h->dts_offset = 0;
	}

	memcpy(b->datas[0].data, src,
	       stream->stride * stream->format.size.height);

	b->datas[0].chunk->size = b->datas[0].maxsize;

done:
	pw_thread_loop_lock(stream->data->main_loop);
	pw_stream_queue_buffer(stream->stream, buf);
	pw_thread_loop_unlock(stream->data->main_loop);
}

static void on_state_changed(void *_data, enum pw_remote_state old,
			     enum pw_remote_state state, const char *error)
{
	struct data *data = _data;

	(void)data;

	switch (state) {
	case PW_REMOTE_STATE_ERROR:
		if (old == PW_REMOTE_STATE_CONNECTING)
			g_print("PipeWire: Failed to connect\n");
		else
			g_print("PipeWire: Remote error: %s\n", error);
		break;
	case PW_REMOTE_STATE_UNCONNECTED:
		g_print("PipeWire: Disconnected\n");
		break;
	case PW_REMOTE_STATE_CONNECTING:
	case PW_REMOTE_STATE_CONNECTED:
	default:
		break;
	}
}

static const struct pw_remote_events remote_events = {
	PW_VERSION_REMOTE_EVENTS,
	.state_changed = on_state_changed,
};

int pipewire_init(int *argc, char **argv[])
{
	struct data *data;

	pw_init(argc, argv);

	data = calloc(sizeof(*data), 1);

	data->loop = pw_loop_new(NULL);
	data->main_loop = pw_thread_loop_new(data->loop, "pipewire-loop");
	data->core = pw_core_new(data->loop, NULL);
	data->t = pw_core_get_type(data->core);
	data->remote = pw_remote_new(data->core, NULL, 0);

	init_type(&data->type, data->t->map);

	global_data = data;

	pw_remote_add_listener(data->remote, &data->remote_listener,
			       &remote_events, &data);
	pw_remote_connect(data->remote);

	return pw_thread_loop_start(data->main_loop);
}

void pipewire_deinit(void)
{
	struct data *data = global_data;

	pw_thread_loop_stop(data->main_loop);

	pw_core_destroy(data->core);
	pw_thread_loop_destroy(data->main_loop);
	pw_loop_destroy(data->loop);

	global_data = NULL;
	free(data);
}

void debug_stream_init(int *argc, char **argv[])
{
	(void)argc;
	(void)argv;
}

void debug_stream_deinit(void)
{
}
