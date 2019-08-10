/*
 * LED debug tools
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "leds.h"
#include "maths.h"
#include "tracking-model.h"

void leds_init(struct leds *leds, int num_leds)
{
	tracking_model_init(&leds->model, num_leds);
	leds->patterns = malloc(num_leds * sizeof(uint16_t));
}

void leds_fini(struct leds *leds)
{
	free(leds->patterns);
	leds->patterns = NULL;
	tracking_model_fini(&leds->model);
}

void leds_copy(struct leds *dst, struct leds *src)
{
	size_t size = src->model.num_points * sizeof(uint16_t);

	tracking_model_copy(&dst->model, &src->model);
	free(dst->patterns);
	dst->patterns = malloc(size);
	memcpy(dst->patterns, src->patterns, size);
}
