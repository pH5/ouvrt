/*
 * JSON helpers
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <json-glib/json-glib.h>

#include "json.h"

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
