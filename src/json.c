/*
 * JSON helpers
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <json-glib/json-glib.h>

#include "json.h"
#include "tracking-model.h"

void json_object_get_vec3_member(JsonObject *object,
				 const char *member_name,
				 vec3 *out)
{
	JsonArray *member = json_object_get_array_member(object, member_name);

	out->x = json_array_get_double_element(member, 0);
	out->y = json_array_get_double_element(member, 1);
	out->z = json_array_get_double_element(member, 2);
}

void json_array_get_vec3_element(JsonArray *array, guint index,
				 vec3 *out)
{
	JsonArray *element = json_array_get_array_element(array, index);

	out->x = json_array_get_double_element(element, 0);
	out->y = json_array_get_double_element(element, 1);
	out->z = json_array_get_double_element(element, 2);
}

void json_object_get_lighthouse_config_member(JsonObject *object,
					      const gchar *member_name,
					      struct tracking_model *model)
{
	JsonObject *config;
	JsonArray *channel_map, *model_normals, *model_points;
	gint64 num_channels, num_normals, num_points;
	int i;

	config = json_object_get_object_member(object, member_name);
	if (!config)
		return;

	channel_map = json_object_get_array_member(config, "channelMap");
	if (!channel_map)
		return;

	num_channels = json_array_get_length(channel_map);
	for (i = 0; i < num_channels; i++) {
		gint64 channel_id = json_array_get_int_element(channel_map, i);

		if (channel_id != i)
			return;
	}

	model_normals = json_object_get_array_member(config, "modelNormals");
	if (!model_normals)
		return;

	num_normals = json_array_get_length(model_normals);
	if (num_normals != num_channels)
		return;

	model_points = json_object_get_array_member(config, "modelPoints");
	if (!model_points)
		return;

	num_points = json_array_get_length(model_points);
	if (num_points != num_channels)
		return;

	tracking_model_init(model, num_points);

	for (i = 0; i < num_points; i++) {
		json_array_get_vec3_element(model_normals, i, &model->normals[i]);
		json_array_get_vec3_element(model_points, i, &model->points[i]);
	}
}
