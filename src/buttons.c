/*
 * Button helper code
 * Copyright 2018 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <glib.h>
#include <stdint.h>

#include "buttons.h"
#include "debug.h"
#include "telemetry.h"

#define BUTTON_PRESSED	0x80

void ouvrt_handle_buttons(uint32_t dev_id, uint32_t buttons,
			  uint32_t last_buttons, uint8_t map_length,
			  const struct button_map *map)
{
	uint8_t btn_codes[map_length];
	int num_buttons = 0;
	int i;

	for (i = 0; i < map_length; i++) {
		uint32_t bit = map[i].bit;

		if ((buttons ^ last_buttons) & bit) {
			btn_codes[num_buttons] = map[i].code;
			if (buttons & bit)
				btn_codes[num_buttons] |= BUTTON_PRESSED;
			num_buttons++;
		}
	}

	telemetry_send_buttons(dev_id, btn_codes, num_buttons);
}
