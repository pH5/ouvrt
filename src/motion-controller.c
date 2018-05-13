/*
 * Windows Mixed Reality Motion Controller
 * Copyright 2018 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "motion-controller.h"
#include "buttons.h"
#include "device.h"
#include "hidraw.h"
#include "imu.h"
#include "telemetry.h"

struct _OuvrtMotionController {
	OuvrtDevice dev;

	bool missing;
	uint64_t last_timestamp;
	uint8_t buttons;
	uint8_t battery;
	uint8_t touchpad[2];

	struct imu_state imu;
};

G_DEFINE_TYPE(OuvrtMotionController, ouvrt_motion_controller, OUVRT_TYPE_DEVICE)

#define MOTION_CONTROLLER_BUTTON_STICK		0x01
#define MOTION_CONTROLLER_BUTTON_WINDOWS	0x02
#define MOTION_CONTROLLER_BUTTON_MENU		0x04
#define MOTION_CONTROLLER_BUTTON_GRIP		0x08
#define MOTION_CONTROLLER_BUTTON_PAD_PRESS	0x10
#define MOTION_CONTROLLER_BUTTON_PAD_TOUCH	0x40

static const struct button_map motion_controller_button_map[6] = {
	{ MOTION_CONTROLLER_BUTTON_STICK, OUVRT_BUTTON_JOYSTICK },
	{ MOTION_CONTROLLER_BUTTON_WINDOWS, OUVRT_BUTTON_SYSTEM },
	{ MOTION_CONTROLLER_BUTTON_MENU, OUVRT_BUTTON_MENU },
	{ MOTION_CONTROLLER_BUTTON_GRIP, OUVRT_BUTTON_GRIP },
	{ MOTION_CONTROLLER_BUTTON_PAD_PRESS, OUVRT_BUTTON_THUMB },
	{ MOTION_CONTROLLER_BUTTON_PAD_TOUCH, OUVRT_TOUCH_THUMB },
};

static void motion_controller_decode_message(OuvrtMotionController *self,
					     const unsigned char *buf,
					     G_GNUC_UNUSED const struct timespec *ts)
{
	uint8_t buttons = buf[1];
	uint16_t stick[2] = {
		buf[2] | ((buf[3] & 0xf) << 8),
		((buf[3] & 0xf0) >> 4) | (buf[4] << 4),
	};
	float joy[2] = {
		stick[0] * 2.0 / 4095 - 1.0,
		stick[1] * 2.0 / 4095 - 1.0,
	};

	telemetry_send_axis(self->dev.id, 0, joy, 2);

	float trigger = buf[5] / 255.0;

	telemetry_send_axis(self->dev.id, 1, &trigger, 1);

	if (self->touchpad[0] != buf[6] || self->touchpad[1] != buf[7]) {
		self->touchpad[0] = buf[6];
		self->touchpad[1] = buf[7];
	}

	if (buf[8] != self->battery) {
		self->battery = buf[8];
	}

	int32_t accel[3] = {
		buf[9] | (buf[10] << 8) | ((int8_t)buf[11] << 16),
		buf[12] | (buf[13] << 8) | ((int8_t)buf[14] << 16),
		buf[15] | (buf[16] << 8) | ((int8_t)buf[17] << 16),
	};

	int32_t gyro[3] = {
		buf[20] | (buf[21] << 8) | ((int8_t)buf[22] << 16),
		buf[23] | (buf[24] << 8) | ((int8_t)buf[25] << 16),
		buf[26] | (buf[27] << 8) | ((int8_t)buf[28] << 16),
	};

	uint32_t time = buf[29] | (buf[30] << 8) | (buf[31] << 16) | (buf[32] << 24);
	int32_t dt = time - self->last_timestamp;
	self->last_timestamp += dt;

	struct raw_imu_sample raw = {
		.time = self->last_timestamp,
		.acc = { accel[0], accel[1], accel[2] },
		.gyro = { gyro[0], gyro[1], gyro[2] },
	};

	struct imu_sample sample = {
		.time = raw.time * 1e-7,
		.acceleration = {
			.x = accel[0] * 9.81 / 506200.,
			.y = accel[1] * 9.81 / 506200.,
			.z = accel[2] * 9.81 / 506200.,
		},
		.angular_velocity = {
			.x = gyro[0] * 1e-5,
			.y = gyro[1] * 1e-5,
			.z = gyro[2] * 1e-5,
		},
	};

	telemetry_send_imu_sample(self->dev.id, &sample);

	pose_update(dt * 1e-7, &self->imu.pose, &sample);

	self->imu.pose.translation.x = 0.0;
	self->imu.pose.translation.y = 0.0;
	self->imu.pose.translation.z = 0.0;
	telemetry_send_pose(self->dev.id, &self->imu.pose);

	if (buttons != self->buttons) {
		ouvrt_handle_buttons(self->dev.id, buttons, self->buttons,
				     6, motion_controller_button_map);
		self->buttons = buttons;
	}
}

/*
 * Nothing to do here.
 */
static int motion_controller_start(G_GNUC_UNUSED OuvrtDevice *dev)
{
	return 0;
}

/*
 * Handles Motion Controller messages.
 */
static void motion_controller_thread(OuvrtDevice *dev)
{
	OuvrtMotionController *self = OUVRT_MOTION_CONTROLLER(dev);
	unsigned char buf[64];
	struct timespec ts;
	struct pollfd fds;
	int ret;

	while (dev->active) {
		fds.fd = dev->fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		clock_gettime(CLOCK_MONOTONIC, &ts);
		if (ret == -1) {
			g_print("%s: Poll failure: %d\n", dev->name, errno);
			continue;
		}

		if (ret == 0) {
			if (!self->missing) {
				g_print("%s: Device stopped sending\n",
					dev->name);
				self->missing = true;
			}
			continue;
		}

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) {
			dev->active = FALSE;
			break;
		}

		if (!(fds.revents & POLLIN)) {
			g_print("%s: Unhandled poll event: 0x%x\n", dev->name,
				fds.revents);
			continue;
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("%s: Read error: %d\n", dev->name, errno);
			continue;
		}
		if (ret != 45 || buf[0] != 0x01) {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
			continue;
		}

		motion_controller_decode_message(self, buf, &ts);
	}
}

/*
 * Nothing to do here.
 */
static void motion_controller_stop(G_GNUC_UNUSED OuvrtDevice *dev)
{
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_motion_controller_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_motion_controller_parent_class)->finalize(object);
}

static void
ouvrt_motion_controller_class_init(OuvrtMotionControllerClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_motion_controller_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = motion_controller_start;
	OUVRT_DEVICE_CLASS(klass)->thread = motion_controller_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = motion_controller_stop;
}

static void ouvrt_motion_controller_init(OuvrtMotionController *self)
{
	self->dev.type = DEVICE_TYPE_CONTROLLER;
	self->imu.pose.rotation.w = 1.0;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Lenovo Explorer device.
 */
OuvrtDevice *motion_controller_new(G_GNUC_UNUSED const char *devnode)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_MOTION_CONTROLLER,
					 NULL));
}
