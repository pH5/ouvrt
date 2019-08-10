/*
 * JSON helpers
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __JSON_H__
#define __JSON_H__

#include <json-glib/json-glib.h>

#include "maths.h"

struct tracking_model;

void json_object_get_vec3_member(JsonObject *object, const char *member,
				 vec3 *out);

void json_array_get_vec3_element(JsonArray *array, guint index,
				 vec3 *out);

void json_object_get_lighthouse_config_member(JsonObject *object,
					      const gchar *member_name,
					      struct tracking_model *model);

#endif /* __JSON_H__ */
