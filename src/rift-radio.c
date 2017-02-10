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
#include "hidraw.h"

static void rift_dump_message(const unsigned char *buf, size_t len)
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

static void rift_decode_remote_message(struct rift_remote *remote,
				       const struct rift_radio_message *message)
{
	int16_t buttons = __le16_to_cpu(message->remote.buttons);

	if (remote->buttons != buttons)
		remote->buttons = buttons;
}

static void rift_decode_touch_message(struct rift_touch_controller *touch,
				      const struct rift_radio_message *message)
{
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

	switch (message->touch.adc_channel) {
	case RIFT_TOUCH_CONTROLLER_ADC_A_X:
		touch->cap_a_x = adc_value;
		break;
	case RIFT_TOUCH_CONTROLLER_ADC_B_Y:
		touch->cap_b_y = adc_value;
		break;
	case RIFT_TOUCH_CONTROLLER_ADC_REST:
		touch->cap_rest = adc_value;
		break;
	case RIFT_TOUCH_CONTROLLER_ADC_STICK:
		touch->cap_stick = adc_value;
		break;
	case RIFT_TOUCH_CONTROLLER_ADC_TRIGGER:
		touch->cap_trigger = adc_value;
		break;
	}

	(void)accel;
	(void)gyro;
	(void)trigger;
	(void)grip;
	(void)stick;
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

	if (message->unknown[0] != 0x1a ||
	    message->unknown[1] != 0x00 ||
	    message->device_type != 0x03 ||
	    message->pairing.unknown_1 != 0x01 ||
	    message->pairing.unknown_0 != 0x00 ||
	    message->pairing.unknown[0] != 0x8c ||
	    message->pairing.unknown[1] != 0x00) {
		g_print("Rift: Unexpected pairing message!\n");
		return -EINVAL;
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

void rift_decode_radio_message(struct rift_radio *radio, int fd,
			       const unsigned char *buf, size_t len)
{
	const struct rift_radio_message *message = (const void *)buf;
	int ret;

	if (message->id == RIFT_RADIO_MESSAGE_ID) {
		if (radio->pairing) {
			ret = rift_decode_pairing_message(radio, fd, message);
			if (ret < 0)
				rift_dump_message(buf, len);
			return;
		}
		if (message->device_type == RIFT_REMOTE) {
			if (!radio->remote.base.present) {
				g_print("Rift: %s present\n",
					radio->remote.base.name);
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
			if (!radio->touch[0].base.active &&
			    message->touch.timestamp)
				rift_radio_activate(&radio->touch[0].base, fd);
			rift_decode_touch_message(&radio->touch[0], message);
		} else if (message->device_type == RIFT_TOUCH_CONTROLLER_RIGHT) {
			if (!radio->touch[1].base.present) {
				g_print("Rift: %s present (%sactive)\n",
					radio->touch[1].base.name,
					message->touch.timestamp ? "" : "in");
				radio->touch[1].base.present = true;
			}
			if (!radio->touch[1].base.active &&
			    message->touch.timestamp)
				rift_radio_activate(&radio->touch[1].base, fd);
			rift_decode_touch_message(&radio->touch[1], message);
		} else {
			g_print("%s: unknown device %02x:", radio->name,
				message->device_type);
			rift_dump_message(buf, len);
		}
	} else {
		unsigned int i;

		for (i = 1; i < len && !buf[i]; i++);
		if (i != len) {
			g_print("%s: unknown message:", radio->name);
			rift_dump_message(buf, len);
			return;
		}
	}
}

void rift_radio_init(struct rift_radio *radio)
{
	radio->remote.base.name = "Remote";
	radio->remote.base.id = RIFT_REMOTE;
	radio->touch[0].base.name = "Left Touch Controller";
	radio->touch[0].base.id = RIFT_TOUCH_CONTROLLER_LEFT;
	radio->touch[1].base.name = "Right Touch Controller";
	radio->touch[1].base.id = RIFT_TOUCH_CONTROLLER_RIGHT;
}
