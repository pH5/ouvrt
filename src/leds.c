/*
 * LED debug tools
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <stdio.h>
#include <stdlib.h>

#include "leds.h"
#include "math.h"
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

void leds_fini(struct leds *leds);
