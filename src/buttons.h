/*
 * Button helper code
 * Copyright 2018 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef __BUTTON_H__
#define __BUTTON_H__

#define OUVRT_BUTTON_TRIGGER		0
#define OUVRT_BUTTON_THUMB		1
#define OUVRT_BUTTON_GRIP		2
#define OUVRT_BUTTON_JOYSTICK		3
#define OUVRT_BUTTON_MENU		4
#define OUVRT_BUTTON_A			5
#define OUVRT_BUTTON_B			6
#define OUVRT_BUTTON_X			7
#define OUVRT_BUTTON_Y			8
#define OUVRT_BUTTON_CROSS		9
#define OUVRT_BUTTON_CIRCLE		10
#define OUVRT_BUTTON_TRIANGLE		11
#define OUVRT_BUTTON_SQUARE		12
#define OUVRT_BUTTON_START		13
#define OUVRT_BUTTON_SELECT		14
#define OUVRT_BUTTON_SYSTEM		15
#define OUVRT_BUTTON_UP			16
#define OUVRT_BUTTON_DOWN		17
#define OUVRT_BUTTON_LEFT		18
#define OUVRT_BUTTON_RIGHT		19
#define OUVRT_BUTTON_PLUS		20
#define OUVRT_BUTTON_MINUS		21
#define OUVRT_BUTTON_BACK		22
#define OUVRT_TOUCH_THUMB		23

struct button_map {
	uint32_t bit;
	uint8_t code;
};

void ouvrt_handle_buttons(uint32_t dev_id, uint32_t buttons,
			  uint32_t last_buttons, uint8_t map_length,
			  const struct button_map *map);

#endif /* __BUTTON_H__ */
