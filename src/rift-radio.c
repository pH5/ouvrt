/*
 * Oculus Rift CV1 Radio
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>

#include "rift-hid-reports.h"
#include "rift-radio.h"
#include "buttons.h"
#include "hidraw.h"
#include "imu.h"
#include "json.h"
#include "telemetry.h"
#include "tracking-model.h"

static void rift_dump_report(const unsigned char *buf, size_t len)
{
	unsigned int i;

	for (i = 0; i < len; i++)
		g_print(" %02x", buf[i]);
	g_print("\n");
}

static int rift_radio_transfer(int fd, uint8_t a, uint8_t b, uint8_t c)
{
	struct rift_radio_control_report report = {
		.id = RIFT_RADIO_CONTROL_REPORT_ID,
		.unknown = { a, b, c },
	};
	int ret;

	ret = hid_send_feature_report(fd, &report, sizeof(report));
	if (ret < 0)
		return ret;

	do {
		ret = hid_get_feature_report(fd, &report, sizeof(report));
		if (ret < 0)
			return ret;
	} while (report.unknown[0] & 0x80);

	if (report.unknown[0] & 0x08)
		return -EIO;

	return 0;
}

static int rift_radio_read(int fd, uint8_t a, uint8_t b, uint8_t c,
			   struct rift_radio_data_report *report)
{
	int ret;

	if (report->id != RIFT_RADIO_DATA_REPORT_ID)
		return -EINVAL;

	ret = rift_radio_transfer(fd, a, b, c);
	if (ret < 0)
		return ret;

	return hid_get_feature_report(fd, report, sizeof(*report));
}

static int rift_radio_write(int fd, uint8_t a, uint8_t b, uint8_t c,
			    struct rift_radio_data_report *report)
{
	int ret;

	if (report->id != RIFT_RADIO_DATA_REPORT_ID)
		return -EINVAL;

	ret = hid_send_feature_report(fd, report, sizeof(*report));
	if (ret < 0)
		return ret;

	return rift_radio_transfer(fd, a, b, c);
}

static int rift_radio_read_flash(int fd, uint8_t device_type,
				 struct rift_radio_data_report *report)
{
	int ret;

	ret = hid_send_feature_report(fd, report, sizeof(*report));
	if (ret < 0)
		return ret;

	ret = rift_radio_transfer(fd, 0x03, RIFT_RADIO_READ_FLASH_CONTROL,
				  device_type);
	if (ret < 0)
		return ret;

	return hid_get_feature_report(fd, report, sizeof(*report));
}

static int rift_radio_read_calibration_hash(int fd, uint8_t device_type,
					    uint8_t hash[16])
{
	struct rift_radio_data_report report = {
		.id = RIFT_RADIO_DATA_REPORT_ID,
		.flash.offset = __cpu_to_le16(0x1bf0),
		.flash.length = 16,
	};
	int ret;

	ret = rift_radio_read_flash(fd, device_type, &report);
	if (ret < 0)
		return ret;

	memcpy(hash, report.flash.data, 16);

	return 0;
}

static int rift_radio_read_calibration(int fd, uint8_t device_type, char **json,
				       uint16_t *length)
{
	struct rift_radio_data_report report = {
		.id = RIFT_RADIO_DATA_REPORT_ID,
	};
	uint16_t offset;
	uint16_t size;
	char *tmp;
	int ret;

	report.flash.offset = __cpu_to_le16(0);
	report.flash.length = __cpu_to_le16(20);
	ret = rift_radio_read_flash(fd, device_type, &report);
	if (ret < 0)
		return ret;

	if (__le16_to_cpup((__le16 *)&report.flash.data[0]) != 1)
		return -EINVAL;

	size = __le16_to_cpup((__le16 *)&report.flash.data[2]);

	tmp = g_malloc(size + 1);

	memcpy(tmp, report.flash.data + 4, 16);

	for (offset = 20; offset < size + 4; offset += 20) {
		report.flash.offset = __cpu_to_le16(offset);
		report.flash.length = __cpu_to_le16(20);
		ret = rift_radio_read_flash(fd, device_type, &report);
		if (ret < 0) {
			g_free(tmp);
			return ret;
		}

		memcpy(tmp + offset - 4, report.flash.data,
		       (offset - 4 + 20 <= size) ? 20 :
		       (size - (offset - 4)));
	}
	tmp[size] = 0;

	*json = tmp;
	*length = size;

	return 0;
}

int rift_radio_get_address(int fd, uint32_t *address)
{
	struct rift_radio_data_report report = {
		.id = RIFT_RADIO_DATA_REPORT_ID,
	};
	int ret;

	ret = rift_radio_read(fd, 0x05, 0x03, 0x05, &report);
	if (ret < 0)
		return ret;

	*address = __le32_to_cpup((__le32 *)report.payload);

	return 0;
}

int rift_get_firmware_version(int fd)
{
	struct rift_radio_data_report report = {
		.id = RIFT_RADIO_DATA_REPORT_ID,
	};
	int ret;
	int i;

	ret = rift_radio_read(fd, 0x05, RIFT_RADIO_FIRMWARE_VERSION_CONTROL,
			      0x05, &report);
	if (ret < 0)
		return ret;

	g_print("Rift: Firmware version ");
	for (i = 14; i < 24 && g_ascii_isalnum(report.payload[i]); i++)
		g_print("%c", report.payload[i]);
	g_print("\n");

	return 0;
}

static int rift_radio_get_serial(int fd, int device_type,
				 uint32_t *address, char *serial)
{
	struct rift_radio_data_report report = {
		.id = RIFT_RADIO_DATA_REPORT_ID,
	};
	int ret;
	int i;

	ret = rift_radio_read(fd, 0x03, RIFT_RADIO_SERIAL_NUMBER_CONTROL,
			      device_type, &report);
	if (ret < 0)
		return ret;

	*address = __le32_to_cpu(report.serial.address);

	for (i = 0; i < 14 && g_ascii_isalnum(report.serial.number[i]); i++)
		serial[i] = report.serial.number[i];

	return 0;
}

static int rift_radio_get_firmware_version(int fd, int device_type,
					   char *firmware_date,
					   char *firmware_version)
{
	struct rift_radio_data_report report = {
		.id = RIFT_RADIO_DATA_REPORT_ID,
	};
	int ret;
	int i;

	ret = rift_radio_read(fd, 0x03, RIFT_RADIO_FIRMWARE_VERSION_CONTROL,
			      device_type, &report);
	if (ret < 0)
		return ret;

	for (i = 0; i < 11 && g_ascii_isprint(report.firmware.date[i]); i++)
		firmware_date[i] = report.firmware.date[i];

	for (i = 0; i < 10 && g_ascii_isalnum(report.firmware.version[i]); i++)
		firmware_version[i] = report.firmware.version[i];

	return 0;
}

static const struct button_map remote_button_map[9] = {
	{ RIFT_REMOTE_BUTTON_UP, OUVRT_BUTTON_UP },
	{ RIFT_REMOTE_BUTTON_DOWN, OUVRT_BUTTON_DOWN },
	{ RIFT_REMOTE_BUTTON_LEFT, OUVRT_BUTTON_LEFT },
	{ RIFT_REMOTE_BUTTON_RIGHT, OUVRT_BUTTON_RIGHT },
	{ RIFT_REMOTE_BUTTON_OK, OUVRT_BUTTON_THUMB },
	{ RIFT_REMOTE_BUTTON_PLUS, OUVRT_BUTTON_PLUS },
	{ RIFT_REMOTE_BUTTON_MINUS, OUVRT_BUTTON_MINUS },
	{ RIFT_REMOTE_BUTTON_OCULUS, OUVRT_BUTTON_SYSTEM },
	{ RIFT_REMOTE_BUTTON_BACK, OUVRT_BUTTON_BACK },
};

static void rift_decode_remote_message(struct rift_remote *remote,
				       const struct rift_radio_message *message)
{
	int16_t buttons = __le16_to_cpu(message->remote.buttons);

	if (remote->buttons != buttons) {
		ouvrt_handle_buttons(remote->base.dev_id, buttons,
				     remote->buttons, 9, remote_button_map);
		remote->buttons = buttons;
	}
}

static const struct button_map touch_left_button_map[4] = {
	{ RIFT_TOUCH_CONTROLLER_BUTTON_X, OUVRT_BUTTON_X },
	{ RIFT_TOUCH_CONTROLLER_BUTTON_Y, OUVRT_BUTTON_Y },
	{ RIFT_TOUCH_CONTROLLER_BUTTON_MENU, OUVRT_BUTTON_MENU },
	{ RIFT_TOUCH_CONTROLLER_BUTTON_STICK, OUVRT_BUTTON_JOYSTICK },
};

static const struct button_map touch_right_button_map[4] = {
	{ RIFT_TOUCH_CONTROLLER_BUTTON_A, OUVRT_BUTTON_A },
	{ RIFT_TOUCH_CONTROLLER_BUTTON_B, OUVRT_BUTTON_B },
	{ RIFT_TOUCH_CONTROLLER_BUTTON_OCULUS, OUVRT_BUTTON_SYSTEM },
	{ RIFT_TOUCH_CONTROLLER_BUTTON_STICK, OUVRT_BUTTON_JOYSTICK },
};

static void rift_decode_touch_message(struct rift_touch_controller *touch,
				      const struct rift_radio_message *message)
{
	uint32_t timestamp = __le32_to_cpu(message->touch.timestamp);
	int16_t accel[3] = {
		__le16_to_cpu(message->touch.accel[0]),
		__le16_to_cpu(message->touch.accel[1]),
		__le16_to_cpu(message->touch.accel[2]),
	};
	int16_t gyro[3] = {
		__le16_to_cpu(message->touch.gyro[0]),
		__le16_to_cpu(message->touch.gyro[1]),
		__le16_to_cpu(message->touch.gyro[2]),
	};
	const uint8_t *tgs = message->touch.trigger_grip_stick;
	uint16_t trigger = tgs[0] | ((tgs[1] & 0x03) << 8);
	uint16_t grip = ((tgs[1] & 0xfc) >> 2) | ((tgs[2] & 0x0f) << 6);
	uint16_t stick[2] = {
		((tgs[2] & 0xf0) >> 4) | ((tgs[3] & 0x3f) << 4),
		((tgs[3] & 0xc0) >> 6) | ((tgs[4] & 0xff) << 2),
	};
	uint16_t adc_value = __le16_to_cpu(message->touch.adc_value);
	int32_t dt = timestamp - touch->last_timestamp;

	if (dt > 1000 - 25 && dt < 1000 + 25) {
		/* 1 ms */
	} else if (dt > 2000 - 25 && dt < 2000 + 25) {
		/* 2 ms */
	} else if (dt > 3000 - 25 || dt < 3000 + 25) {
		/* 3 ms */
	} else {
		g_print("%s: %d µs since last IMU sample\n", touch->base.name,
			dt);
	}
	touch->last_timestamp = timestamp;

	if (!(timestamp ||
	    accel[0] || accel[1] || accel[2] ||
	    gyro[0] || gyro[1] || gyro[2]))
		return;

	struct imu_sample *sample = &touch->imu.sample;
	struct rift_touch_calibration *c = &touch->calibration;
	const double a[3] = {
		9.81 / 2048 * accel[0],
		9.81 / 2048 * accel[1],
		9.81 / 2048 * accel[2],
	};
	const double g[3] = {
		2.0 / 2048 * gyro[0],
		2.0 / 2048 * gyro[1],
		2.0 / 2048 * gyro[2],
	};
	const double ax = c->acc_calibration[0] * a[0] +
			  c->acc_calibration[1] * a[1] +
			  c->acc_calibration[2] * a[2];
	const double ay = c->acc_calibration[3] * a[0] +
			  c->acc_calibration[4] * a[1] +
			  c->acc_calibration[5] * a[2];
	const double az = c->acc_calibration[6] * a[0] +
			  c->acc_calibration[7] * a[1] +
			  c->acc_calibration[8] * a[2];
	const double gx = c->gyro_calibration[0] * g[0] +
			  c->gyro_calibration[1] * g[1] +
			  c->gyro_calibration[2] * g[2];
	const double gy = c->gyro_calibration[3] * g[0] +
			  c->gyro_calibration[4] * g[1] +
			  c->gyro_calibration[5] * g[2];
	const double gz = c->gyro_calibration[6] * g[0] +
			  c->gyro_calibration[7] * g[1] +
			  c->gyro_calibration[8] * g[2];

	sample->time = timestamp;
	sample->acceleration.x = ax;
	sample->acceleration.y = ay;
	sample->acceleration.z = az;
	sample->angular_velocity.x = gx;
	sample->angular_velocity.y = gy;
	sample->angular_velocity.z = gz;

	telemetry_send_imu_sample(touch->base.dev_id, sample);

	const double dt_s = 1e-6 * dt;

	pose_update(dt_s, &touch->imu.pose, sample);

	telemetry_send_pose(touch->base.dev_id, &touch->imu.pose);

	float t;
	if (trigger < c->trigger_mid_range) {
		t = 1.0f - ((float)trigger - c->trigger_min_range) /
		    (c->trigger_mid_range - c->trigger_min_range) * 0.5f;
	} else {
		t = 0.5f - ((float)trigger - c->trigger_mid_range) /
		    (c->trigger_max_range - c->trigger_mid_range) * 0.5f;
	}
	if (t != touch->trigger) {
		touch->trigger = t;
		telemetry_send_axis(touch->base.dev_id, 1, &touch->trigger, 1);
	}

	float gr;
	if (grip < c->middle_mid_range) {
		gr = 1.0f - ((float)grip - c->middle_min_range) /
		     (c->middle_mid_range - c->middle_min_range) * 0.5f;
	} else {
		gr = 0.5f - ((float)grip - c->middle_mid_range) /
		     (c->middle_max_range - c->middle_mid_range) * 0.5f;
	}
	if (gr != touch->grip) {
		touch->grip = gr;
		telemetry_send_axis(touch->base.dev_id, 2, &touch->grip, 1);
	}

	float joy[2];
	if (stick[0] >= c->joy_x_dead_min && stick[0] <= c->joy_x_dead_max &&
	    stick[1] >= c->joy_y_dead_min && stick[1] <= c->joy_y_dead_max) {
		joy[0] = 0.0f;
		joy[1] = 0.0f;
	} else {
		joy[0] = ((float)stick[0] - c->joy_x_range_min) /
			 (c->joy_x_range_max - c->joy_x_range_min) * 2.0f - 1.0f;
		joy[1] = ((float)stick[1] - c->joy_y_range_min) /
			 (c->joy_y_range_max - c->joy_y_range_min) * 2.0f - 1.0f;
	}
	if (joy[0] != touch->stick[0] || joy[1] != touch->stick[1]) {
		touch->stick[0] = joy[0];
		touch->stick[1] = joy[1];

		telemetry_send_axis(touch->base.dev_id, 0, touch->stick, 2);
	}

	switch (message->touch.adc_channel) {
	case RIFT_TOUCH_CONTROLLER_HAPTIC_COUNTER:
		/*
		 * The haptic counter seems to be used as read pointer into a
		 * 256-byte ringbuffer. It is incremented 320 times per second:
		 *
		 * https://developer.oculus.com/documentation/pcsdk/latest/concepts/dg-input-touch-haptic/
		 */
		touch->haptic_counter = adc_value;
		break;
	case RIFT_TOUCH_CONTROLLER_ADC_STICK:
		touch->cap_stick = ((float)adc_value - c->cap_sense_min[0]) /
				   (c->cap_sense_touch[0] - c->cap_sense_min[0]);
		telemetry_send_axis(touch->base.dev_id, 3, &touch->cap_stick, 1);
		break;
	case RIFT_TOUCH_CONTROLLER_ADC_B_Y:
		touch->cap_b_y = ((float)adc_value - c->cap_sense_min[1]) /
				 (c->cap_sense_touch[1] - c->cap_sense_min[1]);
		telemetry_send_axis(touch->base.dev_id, 4, &touch->cap_b_y, 1);
		break;
	case RIFT_TOUCH_CONTROLLER_ADC_TRIGGER:
		touch->cap_trigger = ((float)adc_value - c->cap_sense_min[2]) /
				     (c->cap_sense_touch[2] - c->cap_sense_min[2]);
		telemetry_send_axis(touch->base.dev_id, 5, &touch->cap_trigger, 1);
		break;
	case RIFT_TOUCH_CONTROLLER_ADC_A_X:
		touch->cap_a_x = ((float)adc_value - c->cap_sense_min[3]) /
				 (c->cap_sense_touch[3] - c->cap_sense_min[3]);
		telemetry_send_axis(touch->base.dev_id, 6, &touch->cap_a_x, 1);
		break;
	case RIFT_TOUCH_CONTROLLER_ADC_REST:
		touch->cap_rest = ((float)adc_value - c->cap_sense_min[7]) /
				  (c->cap_sense_touch[7] - c->cap_sense_min[7]);
		telemetry_send_axis(touch->base.dev_id, 7, &touch->cap_a_x, 1);
		break;
	}

	uint8_t buttons = message->touch.buttons;
	if (buttons != touch->buttons) {
		const struct button_map *map;

		map = (touch->base.id == RIFT_TOUCH_CONTROLLER_LEFT) ?
		      touch_left_button_map : touch_right_button_map;

		ouvrt_handle_buttons(touch->base.id, buttons, touch->buttons,
				     4, map);
		touch->buttons = buttons;
	}
}

static int rift_touch_parse_calibration(struct rift_touch_controller *touch,
					const char *json,
					struct rift_touch_calibration *c)
{
	JsonNode *node = json_from_string(json, NULL);
	JsonObject *object = json_node_get_object(node);
	JsonArray *array;
	int version;
	int i, j;

	object = json_object_get_object_member(object, "TrackedObject");
	if (!object) {
		json_node_unref(node);
		return -EINVAL;
	}

	version = json_object_get_int_member(object, "JsonVersion");
	if (version != 2) {
		json_node_unref(node);
		return -EINVAL;
	}

	json_object_get_vec3_member(object, "ImuPosition", &c->imu_position);

	c->joy_x_range_min = json_object_get_int_member(object, "JoyXRangeMin");
	c->joy_x_range_max = json_object_get_int_member(object, "JoyXRangeMax");
	c->joy_x_dead_min = json_object_get_int_member(object, "JoyXDeadMin");
	c->joy_x_dead_max = json_object_get_int_member(object, "JoyXDeadMax");
	c->joy_y_range_min = json_object_get_int_member(object, "JoyYRangeMin");
	c->joy_y_range_max = json_object_get_int_member(object, "JoyYRangeMax");
	c->joy_y_dead_min = json_object_get_int_member(object, "JoyYDeadMin");
	c->joy_y_dead_max = json_object_get_int_member(object, "JoyYDeadMax");

	c->trigger_min_range = json_object_get_int_member(object, "TriggerMinRange");
	c->trigger_mid_range = json_object_get_int_member(object, "TriggerMidRange");
	c->trigger_max_range = json_object_get_int_member(object, "TriggerMaxRange");

	array = json_object_get_array_member(object, "GyroCalibration");
	for (i = 0; i < 12; i++)
		c->gyro_calibration[i] = json_array_get_double_element(array, i);

	c->middle_min_range = json_object_get_int_member(object, "MiddleMinRange");
	c->middle_mid_range = json_object_get_int_member(object, "MiddleMidRange");
	c->middle_max_range = json_object_get_int_member(object, "MiddleMaxRange");

	c->middle_flipped = json_object_get_boolean_member(object, "MiddleFlipped");

	array = json_object_get_array_member(object, "AccCalibration");
	for (i = 0; i < 12; i++)
		c->acc_calibration[i] = json_array_get_double_element(array, i);

	array = json_object_get_array_member(object, "CapSenseMin");
	for (i = 0; i < 8; i++)
		c->cap_sense_min[i] = json_array_get_int_element(array, i);

	array = json_object_get_array_member(object, "CapSenseTouch");
	for (i = 0; i < 8; i++)
		c->cap_sense_touch[i] = json_array_get_int_element(array, i);

	JsonObject *model = json_object_get_object_member(object, "ModelPoints");

	tracking_model_init(&touch->model, json_object_get_size(model));

	for (i = 0; i < touch->model.num_points; i++) {
		char name[8];

		g_snprintf(name, 8, "Point%d", i);
		array = json_object_get_array_member(model, name);

		double point[6];

		for (j = 0; j < 6; j++)
			point[j] = json_array_get_double_element(array, j);

		touch->model.points[i].x = point[0];
		touch->model.points[i].y = point[1];
		touch->model.points[i].z = point[2];
		touch->model.normals[i].x = point[3];
		touch->model.normals[i].y = point[4];
		touch->model.normals[i].z = point[5];
	}

	json_node_unref(node);

	return 0;
}

static int rift_touch_get_calibration(struct rift_touch_controller *touch,
				      int fd)
{
	struct rift_wireless_device *dev = &touch->base;
	uint8_t hash[16];
	char hash_string[33];
	gboolean success;
	char *path;
	char *filename;
	char *json;
	int ret;
	int i;

	ret = rift_radio_read_calibration_hash(fd, dev->id, hash);
	if (ret < 0)
		return ret;

	for (i = 0; i < 32; i++) {
		uint8_t nibble = (i % 2) ? (hash[i / 2] & 0xf) :
					   (hash[i / 2] >> 4);
		hash_string[i] = (nibble < 10) ? ('0' + nibble) :
						 ('a' + nibble - 10);
	}
	hash_string[32] = 0;

	g_print("Rift: %s: calibration hash: %s\n", dev->name, hash_string);

	path = g_strdup_printf("%s/ouvrt", g_get_user_cache_dir());

	filename = g_strdup_printf("%s/%.14s_%s.%ctouch", path, dev->serial,
				   hash_string,
				   (dev->id == RIFT_TOUCH_CONTROLLER_LEFT) ? 'l' : 'r');

	success = g_file_get_contents(filename, &json, NULL, NULL);
	if (success) {
		g_print("Rift: %s: read cached calibration data\n", dev->name);
	} else {
		uint16_t length;

		g_print("Rift: %s: reading calibration data\n", dev->name);

		ret = rift_radio_read_calibration(fd, dev->id, &json, &length);
		if (ret < 0) {
			g_free(filename);
			g_free(path);
			return ret;
		}

		g_mkdir_with_parents(path, 0755);
		g_file_set_contents(filename, json, length, NULL);

		g_print("Rift: %s: wrote calibration data cache\n", dev->name);
	}

	g_free(filename);
	g_free(path);

	rift_touch_parse_calibration(touch, json, &touch->calibration);

	g_free(json);

	return 0;
}

static int rift_radio_activate(struct rift_wireless_device *dev, int fd)
{
	int ret;

	ret = rift_radio_get_serial(fd, dev->id, &dev->address, dev->serial);
	if (ret < 0) {
		g_print("Rift: Failed to read %s serial number\n", dev->name);
		return ret;
	}

	g_print("Rift: %s: Serial %.14s\n", dev->name, dev->serial);

	ret = rift_radio_get_firmware_version(fd, dev->id, dev->firmware_date,
					      dev->firmware_version);
	if (ret < 0) {
		g_print("Rift: Failed to read firmware version\n");
		return ret;
	}

	g_print("Rift: %s: Firmware version %.10s\n", dev->name,
		dev->firmware_version);

	if (dev->id != RIFT_REMOTE) {
		struct rift_touch_controller *touch;

		touch = (struct rift_touch_controller *)dev;
		ret = rift_touch_get_calibration(touch, fd);
		if (ret < 0)
			return ret;
	}

	dev->active = true;

	return 0;
}

int rift_decode_pairing_message(struct rift_radio *radio, int fd,
				const struct rift_radio_message *message)
{
	struct rift_radio_data_report report = {
		.id = RIFT_RADIO_DATA_REPORT_ID,
	};
	uint8_t device_type = message->pairing.device_type;
	uint32_t device_address = __le32_to_cpu(message->pairing.id[0]);
	uint32_t radio_address = __le32_to_cpu(message->pairing.id[1]);
	struct rift_wireless_device *dev;
	uint16_t maybe_channel;
	int ret;
	int i;

	for (i = 0; i < sizeof *message; i++) {
		if (((char *)message)[i])
			break;
	}
	if (i == sizeof *message)
		return 0;

	if (message->unknown[0] != 0x1a ||
	    message->unknown[1] != 0x00 ||
	    message->device_type != 0x03 ||
	    message->pairing.unknown_1 != 0x01) {
		g_print("Rift: Unexpected pairing message!\n");
		return -EINVAL;
	}

	if (message->pairing.buttons & ~(RIFT_TOUCH_CONTROLLER_BUTTON_Y |
					 RIFT_TOUCH_CONTROLLER_BUTTON_STICK)) {
		g_print("Rift: Unexpected buttons in pairing message: 0x%02x\n",
			message->pairing.buttons);
	}

	if ((message->pairing.unknown[0] != 0x8c &&
	     message->pairing.unknown[0] != 0x00) ||
	    message->pairing.unknown[1] != 0x00) {
		g_print("Rift: Unexpected field in pairing message: unknown = { 0x%02x, 0x%02x }\n",
			message->pairing.unknown[0],
			message->pairing.unknown[1]);
	}

	switch (device_type) {
	case RIFT_REMOTE:
		dev = &radio->remote.base;
		maybe_channel = 750;
		break;
	case RIFT_TOUCH_CONTROLLER_LEFT:
		dev = &radio->touch[0].base;
		maybe_channel = 1000;
		break;
	case RIFT_TOUCH_CONTROLLER_RIGHT:
		dev = &radio->touch[1].base;
		maybe_channel = 1250;
		break;
	default:
		g_print("Rift: Unknown device type: 0x%02x\n", device_type);
		return -EINVAL;
	}

	g_print("Rift: Detected %s %08x: %s paired to %08x, firmware %s, rssi(?) %u\n",
		dev->name, device_address,
		(radio_address == radio->address) ? "already" : "currently",
		radio_address, message->pairing.firmware,
		message->pairing.maybe_rssi);

	if (dev->address == device_address)
		return 0;

	g_print("Rift: Pairing %s %08x to headset radio %08x, channel(?) %u ...\n",
		dev->name, device_address, radio->address, maybe_channel);

	/* Step 1: set device address */
	memset(report.payload, 0, sizeof(report.payload));
	*(__le32 *)report.payload = __cpu_to_le32(device_address);
	ret = rift_radio_write(fd, 0x04, 0x07, 0x05, &report);
	if (ret < 0)
		return ret;

	/* Step 2: configure device target address and channel(?) */
	memset(report.payload, 0, sizeof(report.payload));
	report.payload[0] = 0x11;
	report.payload[1] = 0x05;
	report.payload[2] = device_type;
	*(__le32 *)(report.payload + 3) = __cpu_to_le32(radio->address);
	*(__le32 *)(report.payload + 7) = __cpu_to_le32(radio->address);
	report.payload[11] = 0x8c;
	*(__le16 *)(report.payload + 12) = __cpu_to_le16(maybe_channel);
	*(__le16 *)(report.payload + 16) = __cpu_to_le16(2000);
	ret = rift_radio_write(fd, 0x04, 0x09, 0x05, &report);
	if (ret < 0)
		return ret;

	/* Step 3: tell device to stop pairing */
	memset(report.payload, 0, sizeof(report.payload));
	report.payload[0] = 0x21;
	ret = rift_radio_write(fd, 0x04, 0x09, 0x05, &report);
	if (ret < 0)
		return ret;

	/* Step 4: finish pairing */
	memset(report.payload, 0, sizeof(report.payload));
	ret = rift_radio_write(fd, 0x04, 0x08, 0x05, &report);
	if (ret < 0)
		return ret;

	dev->address = device_address;

	g_print("Rift: Pairing %s %08x finished\n", dev->name, dev->address);

	return 0;
}

int rift_decode_radio_message(struct rift_radio *radio, int fd,
			       const struct rift_radio_message *message)
{
	if (radio->pairing)
		return rift_decode_pairing_message(radio, fd, message);

	if (message->unknown[0] == 0)
		return 0;

	if (message->device_type == RIFT_REMOTE) {
		if (!radio->remote.base.present) {
			g_print("Rift: %s present\n", radio->remote.base.name);
			radio->remote.base.present = true;
		}
		rift_decode_remote_message(&radio->remote, message);
	} else if (message->device_type == RIFT_TOUCH_CONTROLLER_LEFT) {
		if (!radio->touch[0].base.present) {
			g_print("Rift: %s present (%sactive)\n",
				radio->touch[0].base.name,
				message->touch.timestamp ? "" : "in");
			radio->touch[0].base.present = true;
		}
		if (!radio->touch[0].base.active && message->touch.timestamp)
			rift_radio_activate(&radio->touch[0].base, fd);
		rift_decode_touch_message(&radio->touch[0], message);
	} else if (message->device_type == RIFT_TOUCH_CONTROLLER_RIGHT) {
		if (!radio->touch[1].base.present) {
			g_print("Rift: %s present (%sactive)\n",
				radio->touch[1].base.name,
				message->touch.timestamp ? "" : "in");
			radio->touch[1].base.present = true;
		}
		if (!radio->touch[1].base.active && message->touch.timestamp)
			rift_radio_activate(&radio->touch[1].base, fd);
		rift_decode_touch_message(&radio->touch[1], message);
	} else {
		g_print("%s: unknown device %02x:", radio->name,
			message->device_type);
		return -ENODEV;
	}

	return 0;
}

void rift_decode_radio_report(struct rift_radio *radio, int fd,
			      const unsigned char *buf, size_t len)
{
	const struct rift_radio_report *report = (const void *)buf;
	int ret;
	int i;

	if (report->id == RIFT_RADIO_REPORT_ID) {
		for (i = 0; i < 2; i++) {
			ret = rift_decode_radio_message(radio, fd,
							&report->message[i]);
			if (ret < 0) {
				rift_dump_report(buf, len);
				return;
			}
		}
	} else {
		unsigned int i;

		for (i = 1; i < len && !buf[i]; i++);
		if (i != len) {
			g_print("%s: unknown message:", radio->name);
			rift_dump_report(buf, len);
		}
	}
}

void rift_radio_init(struct rift_radio *radio)
{
	radio->remote.base.name = "Remote";
	radio->remote.base.id = RIFT_REMOTE;
	radio->touch[0].base.name = "Touch Controller L";
	radio->touch[0].base.id = RIFT_TOUCH_CONTROLLER_LEFT;
	radio->touch[0].imu.pose.rotation.w = 1.0;
	radio->touch[1].base.name = "Touch Controller R";
	radio->touch[1].base.id = RIFT_TOUCH_CONTROLLER_RIGHT;
	radio->touch[1].imu.pose.rotation.w = 1.0;
}
