/*
 * A 3D object of (possibly blinking) LEDs
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier: (LGPL-2.1-or-later OR BSL-1.0)
 */
#ifndef __LEDS_H__
#define __LEDS_H__

#include <stdint.h>

#include "tracking-model.h"

struct leds {
	struct tracking_model model;
	uint16_t *patterns;
};

void leds_init(struct leds *leds, int num_leds);
void leds_fini(struct leds *leds);
void leds_copy(struct leds *dst, struct leds *src);

void leds_dump_obj(struct leds *leds);

#endif /* __LEDS_H__ */
