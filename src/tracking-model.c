/*
 * A 3D object of tracking reference points
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tracking-model.h"
#include "math.h"

void tracking_model_init(struct tracking_model *model, unsigned int num_points)
{
	model->num_points = num_points;
	model->points = malloc(num_points * sizeof(vec3));
	model->normals = malloc(num_points * sizeof(vec3));
}

void tracking_model_fini(struct tracking_model *model)
{
	free(model->points);
	free(model->normals);
	memset(model, 0, sizeof(*model));
}

void tracking_model_dump_obj(struct tracking_model *model, const char *name)
{
	unsigned int i;

	printf("# ouvrt OBJ File: ''\n"
	       "o %s\n", name);

	for (i = 0; i < model->num_points; i++) {
		vec3 *p = &model->points[i];
		vec3 *n = &model->normals[i];

		printf("v %9.6f %9.6f %9.6f\n"
		       "v %9.6f %9.6f %9.6f\n",
		       p->x, p->y, p->z,
		       p->x + n->x, p->y + n->y, p->z + n->z);
	}
	for (i = 0; i < model->num_points; i++)
		printf("l %d %d\n", i * 2 + 1, i * 2 + 2);
}

void tracking_model_dump_struct(struct tracking_model *model)
{
	unsigned int i;

	printf("struct tracking_model model = {\n"
	       "\t.num_points = %d\n"
	       "\t.points = {\n", model->num_points);

	for (i = 0; i < model->num_points; i++) {
		vec3 *p = &model->points[i];

		printf("\t\t{ %9.6f, %9.6f, %9.6f },\n", p->x, p->y, p->z);
	}

	printf("\t},\n"
	       "\t.normals = {\n");

	for (i = 0; i < model->num_points; i++) {
		vec3 *n = &model->normals[i];

		printf("\t\t{ %9.6f, %9.6f, %9.6f },\n", n->x, n->y, n->z);
	}

	printf("\t},\n"
	       "};\n");
}
