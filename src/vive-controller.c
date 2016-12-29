/*
 * HTC Vive Controller
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <json-glib/json-glib.h>

#include "vive-controller.h"
#include "vive-config.h"
#include "vive-hid-reports.h"
#include "lighthouse.h"
#include "device.h"
#include "hidraw.h"
#include "json.h"
#include "math.h"

struct _OuvrtViveControllerPrivate {
	JsonNode *config;
	const gchar *serial;
	gboolean connected;
	vec3 acc_bias;
	vec3 acc_scale;
	vec3 gyro_bias;
	vec3 gyro_scale;

	uint32_t timestamp;
	uint8_t battery;
	uint8_t buttons;
	int16_t touch_pos[2];
	uint8_t squeeze;
};

G_DEFINE_TYPE_WITH_PRIVATE(OuvrtViveController, ouvrt_vive_controller, \
			   OUVRT_TYPE_DEVICE)

/*
 * Retrieves the controller firmware version
 */
static int vive_controller_get_firmware_version(OuvrtViveController *self)
{
	struct vive_firmware_version_report report = {
		.id = VIVE_FIRMWARE_VERSION_REPORT_ID,
	};
	uint32_t firmware_version;
	int ret;

	ret = hid_get_feature_report_timeout(self->dev.fd, &report,
					     sizeof(report), 100);
	if (ret < 0) {
		if (errno != EPIPE) {
			g_print("%s: Read error 0x05: %d\n", self->dev.name,
				errno);
		}
		return ret;
	}

	firmware_version = __le32_to_cpu(report.firmware_version);

	g_print("%s: Controller firmware version %u %s@%s FPGA %u.%u\n",
		self->dev.name, firmware_version, report.string1,
		report.string2, report.fpga_version_major,
		report.fpga_version_minor);
	g_print("%s: Hardware revision: %d rev %d.%d.%d\n",
		self->dev.name, report.hardware_revision,
		report.hardware_version_major, report.hardware_version_minor,
		report.hardware_version_micro);

	return 0;
}

/*
 * Downloads the configuration data stored in the controller
 */
static int vive_controller_get_config(OuvrtViveController *self)
{
	char *config_json;
	JsonObject *object;

	config_json = ouvrt_vive_get_config(&self->dev);
	if (!config_json)
		return -1;

	self->priv->config = json_from_string(config_json, NULL);
	g_free(config_json);
	if (!self->priv->config) {
		g_print("Vive Wireless Receiver %s: Parsing JSON configuration data failed\n",
			self->dev.serial);
		return -1;
	}

	object = json_node_get_object(self->priv->config);

	json_object_get_vec3_member(object, "acc_bias", &self->priv->acc_bias);
	json_object_get_vec3_member(object, "acc_scale", &self->priv->acc_scale);

	self->priv->serial = json_object_get_string_member(object,
							   "device_serial_number");

	json_object_get_vec3_member(object, "gyro_bias", &self->priv->gyro_bias);
	json_object_get_vec3_member(object, "gyro_scale", &self->priv->gyro_scale);

	return 0;
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

	if (battery != self->priv->battery)
		self->priv->battery = battery;

	(void)charge_percent;
	(void)charging;
}

static void vive_controller_handle_buttons(OuvrtViveController *self,
					   uint8_t buttons)
{
	if (buttons != self->priv->buttons)
		self->priv->buttons = buttons;
}

static void vive_controller_handle_touch_position(OuvrtViveController *self,
						  uint8_t *buf)
{
	int16_t x = __le16_to_cpup((__le16 *)buf);
	int16_t y = __le16_to_cpup((__le16 *)(buf + 2));

	if (x != self->priv->touch_pos[0] ||
	    y != self->priv->touch_pos[1]) {
		self->priv->touch_pos[0] = x;
		self->priv->touch_pos[1] = y;
	}
}

static void vive_controller_handle_analog_trigger(OuvrtViveController *self,
						  uint8_t squeeze)
{
	if (squeeze != self->priv->squeeze)
		self->priv->squeeze = squeeze;
}

static void vive_controller_handle_imu_sample(OuvrtViveController *self,
					      uint8_t *buf)
{
	/* Time in 48 MHz ticks, but we are missing the low byte */
	uint32_t timestamp = self->priv->timestamp | *buf;
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

	self->priv->timestamp = (message->timestamp_hi << 24) |
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
			if (type & 2) {
				vive_controller_handle_touch_position(self,
								      buf);
				buf += 4;
			}
			if (type & 4) {
				vive_controller_handle_analog_trigger(self,
								      *buf++);
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
		uint32_t ref = self->priv->timestamp;
		uint32_t timestamp;

		timestamp = (abs(ts1 - ref) < abs(ts2 - ref)) ? ts1 :
			    (abs(ts2 - ref) < abs(ts3 - ref)) ? ts2 :
								ts3;
	}
}

/*
 * Opens the Wireless Receiver HID device descriptor.
 */
static int vive_controller_start(OuvrtDevice *dev)
{
	int fd = dev->fd;

	if (fd == -1) {
		fd = open(dev->devnode, O_RDWR | O_NONBLOCK);
		if (fd == -1) {
			g_print("Vive Wireless Receiver %s: Failed to open '%s': %d\n",
				dev->serial, dev->devnode, errno);
			return -1;
		}
		dev->fd = fd;
	}

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

	ret = vive_controller_get_firmware_version(self);
	if (ret < 0 && errno == EPIPE) {
		g_print("Vive Wireless Receiver %s: No connected controller found\n",
			dev->serial);
	}
	if (!ret) {
		ret = vive_controller_get_config(self);
		if (!ret) {
			g_print("Vive Wireless Receiver %s: Controller %s connected\n",
				dev->serial, self->priv->serial);
			self->priv->connected = TRUE;
		}
	}

	while (dev->active) {
		fds.fd = dev->fd;
		fds.events = POLLIN;
		fds.revents = 0;

		ret = poll(&fds, 1, 1000);
		if (ret == -1) {
			g_print("Vive Wireless Receiver %s: Poll failure: %d\n",
				dev->serial, errno);
			continue;
		}

		if (fds.revents & (POLLERR | POLLHUP | POLLNVAL))
			break;

		if (!(fds.revents & POLLIN)) {
			if (self->priv->connected) {
				g_print("Vive Wireless Receiver %s: Poll timeout\n",
					dev->serial);
				continue;
			}
		}

		if (!self->priv->connected) {
			ret = vive_controller_get_firmware_version(self);
			if (ret < 0)
				continue;

			ret = vive_controller_get_config(self);
			if (ret < 0)
				continue;

			g_print("Vive Wireless Receiver %s: Controller %s connected\n",
				dev->serial, self->priv->serial);
			self->priv->connected = TRUE;
		}

		ret = read(dev->fd, buf, sizeof(buf));
		if (ret == -1) {
			g_print("Vive Controller %s: Read error: %d\n",
				self->priv->serial, errno);
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
			g_print("Vive Wireless Receiver %s: Controller %s disconnected\n",
				dev->serial, self->priv->serial);
			self->priv->connected = FALSE;
		} else {
			g_print("Vive Controller %s: Error, invalid %d-byte report 0x%02x\n",
				self->priv->serial, ret, buf[0]);
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
	self->priv = ouvrt_vive_controller_get_instance_private(self);

	self->priv->config = NULL;
	self->priv->connected = FALSE;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Vive Controller device.
 */
OuvrtDevice *vive_controller_new(const char *devnode)
{
	OuvrtViveController *vive;

	vive = g_object_new(OUVRT_TYPE_VIVE_CONTROLLER, NULL);
	if (vive == NULL)
		return NULL;

	vive->dev.devnode = g_strdup(devnode);

	return &vive->dev;
}
