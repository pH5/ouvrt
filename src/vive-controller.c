/*
 * HTC Vive Controller
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <json-glib/json-glib.h>

#include "vive-controller.h"
#include "vive-config.h"
#include "vive-firmware.h"
#include "vive-hid-reports.h"
#include "vive-imu.h"
#include "lighthouse.h"
#include "buttons.h"
#include "device.h"
#include "hidraw.h"
#include "json.h"
#include "maths.h"
#include "usb-ids.h"
#include "telemetry.h"

struct _OuvrtViveController {
	OuvrtDevice dev;

	JsonNode *config;
	const gchar *serial;
	gboolean connected;
	struct vive_imu imu;
	struct lighthouse_watchman watchman;

	uint32_t timestamp;
	uint8_t battery;
	uint8_t buttons;
	int16_t touch_pos[2];
	uint8_t squeeze;
};

G_DEFINE_TYPE(OuvrtViveController, ouvrt_vive_controller, OUVRT_TYPE_DEVICE)

/*
 * Downloads the configuration data stored in the controller
 */
static int vive_controller_get_config(OuvrtViveController *self)
{
	char *config_json;
	JsonObject *object;
	struct vive_imu *imu = &self->imu;
	const char *device_class;
	gint64 device_pid, device_vid;

	config_json = ouvrt_vive_get_config(&self->dev);
	if (!config_json)
		return -1;

	self->config = json_from_string(config_json, NULL);
	g_free(config_json);
	if (!self->config) {
		g_print("%s: Parsing JSON configuration data failed\n",
			self->dev.name);
		return -1;
	}

	object = json_node_get_object(self->config);

	json_object_get_vec3_member(object, "acc_bias", &imu->acc_bias);
	json_object_get_vec3_member(object, "acc_scale", &imu->acc_scale);

	device_class = json_object_get_string_member(object, "device_class");
	if (strcmp(device_class, "controller") != 0) {
		g_print("%s: Unknown device class \"%s\"\n", self->dev.name,
			device_class);
	}

	device_pid = json_object_get_int_member(object, "device_pid");
	if (device_pid != PID_VIVE_CONTROLLER_USB) {
		g_print("%s: Unknown device PID: 0x%04lx\n", self->dev.name,
			device_pid);
	}

	self->serial = json_object_get_string_member(object,
						     "device_serial_number");

	device_vid = json_object_get_int_member(object, "device_vid");
	if (device_vid != VID_VALVE) {
		g_print("%s: Unknown device VID: 0x%04lx\n", self->dev.name,
			device_vid);
	}

	json_object_get_vec3_member(object, "gyro_bias", &imu->gyro_bias);
	json_object_get_vec3_member(object, "gyro_scale", &imu->gyro_scale);

	json_object_get_lighthouse_config_member(object, "lighthouse_config",
						 &self->watchman.model);
	if (!self->watchman.model.num_points) {
		g_print("%s: Failed to parse Lighthouse configuration\n",
			self->dev.name);
	}

	return 0;
}

static int vive_controller_haptic_pulse(OuvrtViveController *self)
{
	const struct vive_controller_haptic_pulse_report report = {
		.id = VIVE_CONTROLLER_COMMAND_REPORT_ID,
		.command = VIVE_CONTROLLER_HAPTIC_PULSE_COMMAND,
		.len = 7,
		.unknown = { 0x00, 0xf4, 0x01, 0xb5, 0xa2, 0x01, 0x00 },
	};

	return hid_send_feature_report(self->dev.fd, &report, sizeof(report));
}

static int vive_controller_poweroff(OuvrtViveController *self)
{
	const struct vive_controller_poweroff_report report = {
		.id = VIVE_CONTROLLER_COMMAND_REPORT_ID,
		.command = VIVE_CONTROLLER_POWEROFF_COMMAND,
		.len = 4,
		.magic = { 'o', 'f', 'f', '!' },
	};

	return hid_send_feature_report(self->dev.fd, &report, sizeof(report));
}

static void vive_controller_handle_battery(OuvrtViveController *self,
					   uint8_t battery)
{
	uint8_t charge_percent = battery & VIVE_CONTROLLER_BATTERY_CHARGE_MASK;
	gboolean charging = battery & VIVE_CONTROLLER_BATTERY_CHARGING;

	if (battery != self->battery)
		self->battery = battery;

	(void)charge_percent;
	(void)charging;
}

static const struct button_map vive_controller_button_map[6] = {
	{ VIVE_CONTROLLER_BUTTON_MENU, OUVRT_BUTTON_MENU },
	{ VIVE_CONTROLLER_BUTTON_GRIP, OUVRT_BUTTON_GRIP },
	{ VIVE_CONTROLLER_BUTTON_SYSTEM, OUVRT_BUTTON_SYSTEM },
	{ VIVE_CONTROLLER_BUTTON_THUMB, OUVRT_BUTTON_THUMB },
	{ VIVE_CONTROLLER_BUTTON_TOUCH, OUVRT_TOUCH_THUMB },
	{ VIVE_CONTROLLER_BUTTON_TRIGGER, OUVRT_BUTTON_TRIGGER },
};

static void vive_controller_handle_buttons(OuvrtViveController *self,
					   uint8_t buttons)
{
	if (buttons != self->buttons) {
		ouvrt_handle_buttons(self->dev.id, buttons, self->buttons,
				     6, vive_controller_button_map);
		self->buttons = buttons;
	}
}

static void vive_controller_handle_touch_position(OuvrtViveController *self,
						  uint8_t *buf)
{
	int16_t x = __le16_to_cpup((__le16 *)buf);
	int16_t y = __le16_to_cpup((__le16 *)(buf + 2));

	if (x != self->touch_pos[0] ||
	    y != self->touch_pos[1]) {
		self->touch_pos[0] = x;
		self->touch_pos[1] = y;
	}
}

static void vive_controller_handle_analog_trigger(OuvrtViveController *self,
						  uint8_t squeeze)
{
	if (squeeze != self->squeeze)
		self->squeeze = squeeze;
}

static void vive_controller_handle_imu_sample(OuvrtViveController *self,
					      uint8_t *buf)
{
	/* Time in 48 MHz ticks, but we are missing the low byte */
	uint32_t timestamp = self->timestamp | (*buf << 8);
	int16_t accel[3] = {
		__le16_to_cpup((__le16 *)(buf + 1)),
		__le16_to_cpup((__le16 *)(buf + 3)),
		__le16_to_cpup((__le16 *)(buf + 5)),
	};
	int16_t gyro[3] = {
		__le16_to_cpup((__le16 *)(buf + 7)),
		__le16_to_cpup((__le16 *)(buf + 9)),
		__le16_to_cpup((__le16 *)(buf + 11)),
	};

	(void)timestamp;
	(void)accel;
	(void)gyro;
}

/*
 * Dumps the raw controller message into the standard output.
 */
static void
vive_controller_dump_message(OuvrtViveController *self,
			     const struct vive_controller_message *msg)
{
	unsigned char *buf = (unsigned char *)msg;
	unsigned int len = 2 + msg->len;
	unsigned int i;

	if (len > sizeof(*msg))
		len = sizeof(*msg);

	g_print("%s:", self->dev.name);
	for (i = 0; i < len; i++)
		g_print(" %02x", buf[i]);
	g_print("\n");
}

/*
 * Decodes multiplexed Wireless Receiver messages.
 */
static void
vive_controller_decode_message(OuvrtViveController *self,
			       struct vive_controller_message *message)
{
	unsigned char *buf = message->payload;
	unsigned char *end = message->payload + message->len - 1;
	gboolean silent = TRUE;
	int i;

	self->timestamp = (message->timestamp_hi << 24) |
			  (message->timestamp_lo << 16);

	/*
	 * Handle button, touch, and IMU events. The first byte of each event
	 * has the three most significant bits set.
	 */
	while ((buf < end) && ((*buf >> 5) == 7)) {
		uint8_t type = *buf++;

		if (type & 0x10) {
			if (type & 1)
				vive_controller_handle_buttons(self, *buf++);
			if (type & 4) {
				vive_controller_handle_analog_trigger(self,
								      *buf++);
			}
			if (type & 2) {
				vive_controller_handle_touch_position(self,
								      buf);
				buf += 4;
			}
		} else {
			if (type & 1)
				vive_controller_handle_battery(self, *buf++);
			if (type & 2) {
				/* unknown, does ever happen? */
				silent = FALSE;
				buf++;
			}
		}
		if (type & 8) {
			vive_controller_handle_imu_sample(self, buf);
			buf += 13;
		}
	}

	if (buf > end)
		g_print("overshoot: %ld\n", buf - end);
	if (!silent || buf > end)
		vive_controller_dump_message(self, message);
	if (buf >= end)
		return;

	/*
	 * The remainder of the message contains encoded light pulse messages
	 * from up to 32 sensors. A differential encoding is used to keep the
	 * amount of data sent over the wireless link at a minimum.
	 *
	 * First, a number of id bytes equal to the number of pulses contained
	 * in the message lists the sensor indices in the 5 most significant
	 * bits and the number of edges observed from other sensors while the
	 * given sensor's envelope signal was active in the 3 least significant
	 * bits each, in chronological order of the falling edge.
	 *
	 * For example:
	 *
	 *            t0____t2
	 * Sensor 4 ___|    |________ 1 edge while active, id0 = (4<<3)|1 = 0x21
	 *                 _____
	 * Sensor 7 ______|     |____ 1 edge while active, id1 = (7<<3)|1 = 0x39
	 *                t1   t3
	 *
	 * The remaining bytes contain timestamp deltas between observed edges
	 * encoded as variable length little-endian data with the most
	 * significant bit denoting the start of a new value and the 7 least
	 * significant bits containing the payload. Finally, a three byte
	 * timestamp marks the falling edge of the last light pulse.
	 *
	 */
	uint32_t timestamp = ((end[-1])<<16) | ((end[-2])<<8) | (end[-3]);
	int32_t dt;

	/* Edge times are delta encoded from the last timestamp */
	uint32_t edge_ts[16];
	int num_edges;

	edge_ts[0] = timestamp;
	num_edges = 1;
	dt = 0;
	for (i = end-buf-4; i >= num_edges / 2; i--) {
		dt = (dt << 7) | (buf[i] & 0x7f);
		if (buf[i] & 0x80) {
			edge_ts[num_edges] = (edge_ts[num_edges - 1] - dt) & 0xffffff;
			dt = 0;
			num_edges++;
		}
	}

	int rising = 0;
	uint32_t mask = 0;
	uint32_t duration[8];
	uint32_t start[8];
	for (i = 0; i < num_edges / 2; i++) {
		int falling = rising + 1 + (buf[i] & 7);
		mask |= 1 << falling;
		duration[i] = edge_ts[rising] - edge_ts[falling];
		start[i] = edge_ts[falling];

		rising++;
		while (mask & (1 << rising))
			rising++;
	}
	for (i = num_edges / 2 - 1; i >= 0; i--) {
		/*
		 * Reconstruct the most significant byte of the pulse timestamp
		 * from the packet timestamp.
		 * About 99.4% of the time, it is the same as timestamp_hi, but
		 * about 0.5% of the time it is timestamp_hi - 1, and about
		 * 0.1% of the time it is timestamp_hi + 1.
		 * Prepare all three candidate timestamps and choose whichever
		 * is nearest to the packet timestamp.
		 */
		uint32_t ts1 = ((message->timestamp_hi - 1) << 24) | start[i];
		uint32_t ts2 = (message->timestamp_hi << 24) | start[i];
		uint32_t ts3 = ((message->timestamp_hi + 1) << 24) | start[i];
		int32_t dts1 = ts1 - self->timestamp;
		int32_t dts2 = ts2 - self->timestamp;
		int32_t dts3 = ts3 - self->timestamp;
		uint32_t timestamp;

		timestamp = (abs(dts1) < abs(dts2)) ? ts1 :
			    (abs(dts2) < abs(dts3)) ? ts2 : ts3;

		lighthouse_watchman_handle_pulse(&self->watchman,
						 buf[i] >> 3, duration[i],
						 timestamp);
	}
}

/*
 * Opens the Wireless Receiver HID device descriptor.
 */
static int vive_controller_start(OuvrtDevice *dev)
{
	OuvrtViveController *self = OUVRT_VIVE_CONTROLLER(dev);

	g_free(self->dev.name);
	self->dev.name = g_strdup_printf("Vive Wireless Receiver %s",
					 dev->serial);

	self->watchman.name = self->dev.name;

	return 0;
}

/*
 * Keeps the controller active.
 */
static void vive_controller_thread(OuvrtDevice *dev)
{
	OuvrtViveController *self = OUVRT_VIVE_CONTROLLER(dev);
	unsigned char buf[64];
	struct pollfd fds;
	int ret;

	ret = vive_get_firmware_version(dev);
	if (ret < 0 && errno == EPIPE) {
		g_print("%s: No connected controller found\n", dev->name);
	}
	if (!ret) {
		ret = vive_controller_get_config(self);
		if (!ret) {
			g_print("%s: Controller %s connected\n", dev->name,
				self->serial);
			g_free(dev->name);
			dev->name = g_strdup_printf("Vive Controller %s",
						    self->serial);
			self->watchman.name = dev->name;
			self->connected = TRUE;
		}
	}

	while (dev->active) {
		fds.fd = dev->fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		if (ret == -1) {
			g_print("%s: Poll failure: %d\n", dev->name, errno);
			continue;
		}

		if (ret == 0) {
			if (self->connected)
				g_print("%s: Poll timeout\n", dev->name);
			continue;
		}

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL))
			break;

		if (!(fds.revents & POLLIN)) {
			g_print("%s: Unhandled poll event: 0x%x\n", dev->name,
				fds.revents);
			continue;
		}

		if (!self->connected) {
			ret = vive_get_firmware_version(dev);
			if (ret < 0)
				continue;

			ret = vive_controller_get_config(self);
			if (ret < 0)
				continue;

			g_print("%s: Controller %s connected\n", dev->name,
				self->serial);
			g_free(dev->name);
			dev->name = g_strdup_printf("Vive Controller %s",
						    self->serial);
			self->watchman.name = dev->name;
			self->connected = TRUE;

			vive_controller_haptic_pulse(self);
		}

		if (self->imu.gyro_range == 0.0) {
			ret = vive_imu_get_range_modes(dev, &self->imu);
			if (ret < 0) {
				g_print("%s: Failed to get gyro/accelerometer range modes\n",
					dev->name);
				continue;
			}
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("%s: Read error: %d\n", dev->name, errno);
			continue;
		}
		if (ret == 30 && buf[0] == VIVE_CONTROLLER_REPORT1_ID) {
			struct vive_controller_report1 *report = (void *)buf;

			vive_controller_decode_message(self, &report->message);
		} else if (ret == 59 && buf[0] == VIVE_CONTROLLER_REPORT2_ID) {
			struct vive_controller_report2 *report = (void *)buf;

			vive_controller_decode_message(self,
						       &report->message[0]);
			vive_controller_decode_message(self,
						       &report->message[1]);
		} else if (ret == 2 &&
			   buf[0] == VIVE_CONTROLLER_DISCONNECT_REPORT_ID &&
			   buf[1] == 0x01) {
			g_free(dev->name);
			dev->name = g_strdup_printf("Vive Wireless Receiver %s",
						    dev->serial);
			self->watchman.name = dev->name;
			g_print("%s: Controller %s disconnected\n", dev->name,
				self->serial);
			self->connected = FALSE;
		} else {
			g_print("%s: Error, invalid %d-byte report 0x%02x\n",
				dev->name, ret, buf[0]);
		}
	}
}

/*
 * Powers off the controller.
 */
static void vive_controller_stop(OuvrtDevice *dev)
{
	OuvrtViveController *self = OUVRT_VIVE_CONTROLLER(dev);

	vive_controller_poweroff(self);
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_vive_controller_finalize(GObject *object)
{
	G_OBJECT_CLASS(ouvrt_vive_controller_parent_class)->finalize(object);
}

static void ouvrt_vive_controller_class_init(OuvrtViveControllerClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_vive_controller_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = vive_controller_start;
	OUVRT_DEVICE_CLASS(klass)->thread = vive_controller_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = vive_controller_stop;
}

static void ouvrt_vive_controller_init(OuvrtViveController *self)
{
	self->dev.type = DEVICE_TYPE_CONTROLLER;
	self->config = NULL;
	self->connected = FALSE;
	self->imu.sequence = 0;
	self->imu.time = 0;
	self->imu.state.pose.rotation.w = 1.0;
	lighthouse_watchman_init(&self->watchman);
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Controller device.
 */
OuvrtDevice *vive_controller_new(G_GNUC_UNUSED const char *devnode)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_VIVE_CONTROLLER, NULL));
}
