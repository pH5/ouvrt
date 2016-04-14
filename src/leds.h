/*
 * A 3D object of (possibly blinking) LEDs
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __LEDS_H__
#define __LEDS_H__

#include <stdint.h>

#include "math.h"

#define MAX_LEDS 40

struct leds {
	int num;
	vec3 positions[MAX_LEDS];
	vec3 directions[MAX_LEDS];
	uint16_t patterns[MAX_LEDS];
};

void leds_dump_obj(struct leds *leds);
void leds_dump_struct(struct leds *leds);

#endif /* __LEDS_H__ */
