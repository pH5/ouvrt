/*
 * JSON helpers
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __JSON_H__
#define __JSON_H__

#include <json-glib/json-glib.h>

#include "math.h"

void json_object_get_vec3_member(JsonObject *object, const char *member,
				 vec3 *out);

void json_array_get_vec3_element(JsonArray *array, guint index,
				 vec3 *out);

#endif /* __JSON_H__ */
