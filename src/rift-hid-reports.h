/*
 * Oculus Rift HMD USB HID reports
 * Copyright 2015-2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+ or BSL-1.0
 */
#ifndef __RIFT_HID_REPORTS__
#define __RIFT_HID_REPORTS__

#include <asm/byteorder.h>

#define RIFT_CONFIG_REPORT_ID			0x02

#define RIFT_CONFIG_USE_CALIBRATION		0x04
#define RIFT_CONFIG_AUTO_CALIBRATION		0x08
#define RIFT_CONFIG_SENSOR_COORDINATES		0x40

struct rift_config_report {
	__u8 id;
	__u16 echo;
	__u8 flags;
	__u8 packet_interval;
	__le16 sample_rate;
} __attribute__((packed));

#define RIFT_UNKNOWN_REPORT_6_ID		0x06

struct rift_unknown_report_6 {
	__u8 id;
	__u16 echo;
	__u8 unknown;
} __attribute__((packed));

#define RIFT_TRACKING_REPORT_ID			0x0c

#define RIFT_TRACKING_ENABLE			0x01
#define RIFT_TRACKING_AUTO_INCREMENT		0x02
#define RIFT_TRACKING_USE_CARRIER		0x04
#define RIFT_TRACKING_SYNC_INPUT		0x08
#define RIFT_TRACKING_VSYNC_LOCK		0x10
#define RIFT_TRACKING_CUSTOM_PATTERN		0x20

#define RIFT_TRACKING_EXPOSURE_US		350
#define RIFT_TRACKING_PERIOD_US			16666
#define RIFT_TRACKING_VSYNC_OFFSET		0
#define RIFT_TRACKING_DUTY_CYCLE		0x7f

struct rift_tracking_report {
	__u8 id;
	__u16 echo;
	__u8 pattern;
	__u8 flags;
	__u8 reserved;
	__le16 exposure_us;
	__le16 period_us;
	__le16 vsync_offset;
	__u8 duty_cycle;
} __attribute__((packed));

#define RIFT_DISPLAY_REPORT_ID			0x0d

#define RIFT_DISPLAY_READ_PIXEL			0x04
#define RIFT_DISPLAY_DIRECT_PENTILE		0x08

struct rift_display_report {
	__u8 id;
	__u16 echo;
	__u8 brightness;
	__u8 flags1;
	__u8 flags2;
	__le16 unknown_6;
	__le16 persistence;
	__le16 lighting_offset;
	__le16 pixel_settle;
	__le16 total_rows;
} __attribute__((packed));

#define RIFT_POSITION_REPORT_ID			0x0f

struct rift_position_report {
	__u8 id;
	__u16 echo;
	__u8 reserved_1;
	__le32 pos[3];
	__le16 dir[3];
	__le16 reserved_2;
	__le16 index;
	__le16 num;
	__le16 type;
} __attribute__((packed));

#define RIFT_LED_PATTERN_REPORT_ID		0x10

struct rift_led_pattern_report {
	__u8 id;
	__u16 echo;
	__u8 pattern_length;
	__le32 pattern;
	__le16 index;
	__le16 num;
} __attribute__((packed));

#define RIFT_KEEPALIVE_REPORT_ID		0x11

#define RIFT_KEEPALIVE_TYPE			0x0b
#define RIFT_KEEPALIVE_TIMEOUT_MS		10000

struct rift_keepalive_report {
	__u8 id;
	__le16 echo;
	__u8 type;
	__le16 timeout_ms;
} __attribute__((packed));

#define RIFT_RADIO_CONTROL_REPORT_ID		0x1a

#define RIFT_RADIO_SERIAL_NUMBER_CONTROL	0x88
#define RIFT_RADIO_FIRMWARE_VERSION_CONTROL	0x82

struct rift_radio_control_report {
	__u8 id;
	__u16 echo;
	__u8 unknown[3];
} __attribute__((packed));

#define RIFT_RADIO_DATA_REPORT_ID		0x1b

struct rift_radio_serial_number_report {
	__u8 unknown[9];
	__u8 number[14];
	__u8 padding[5];
} __attribute__((packed));

struct rift_radio_firmware_version_report {
	__u8 unknown[3];
	__u8 date[11];
	__u8 version[10];
	__u8 padding[4];
} __attribute__((packed));

struct rift_radio_data_report {
	__u8 id;
	__u16 echo;
	union {
		struct rift_radio_serial_number_report serial;
		struct rift_radio_firmware_version_report firmware;
		__u8 payload[28];
	};
} __attribute__((packed));

#define RIFT_CV1_POWER_REPORT_ID		0x1d

#define RIFT_CV1_POWER_DISPLAY			0x01
#define RIFT_CV1_POWER_AUDIO			0x02
#define RIFT_CV1_POWER_LEDS			0x04

struct rift_cv1_power_report {
	__u8 id;
	__u16 echo;
	__u8 components;
} __attribute__((packed));

#define RIFT_CV1_SENSOR_REPORT_ID		0x1f

struct rift_cv1_sensor_report {
	__u8 id;
	__u16 echo;
	__u8 zero_padding;
	__le16 proximity;
	__le16 lens_separation;
	__le16 unknown[4];
	__u8 unknown_zeros[4];
};

#define RIFT_CV1_LIFETIME_REPORT_ID		0x22

struct rift_cv1_lifetime_report {
	__u8 id;
	__u16 echo;
	__u8 zero_padding;
	__le32 runtime_s;
	__le16 proximity_count;
	__le16 poweron_count;
	__le32 display_runtime_s;
	__le32 display_poweron_count;
	__le32 lens_separation_travel;
	__le16 lens_separation_change_count;
	__u8 unknown_zeros[38];
} __attribute__((packed));

#define RIFT_SENSOR_MESSAGE_ID			0x0b

struct rift_imu_sample {
	__be64 accel;				/* packed, 10⁻⁴ m/s² */
	__be64 gyro;				/* packed, 10⁻⁴ rad/s */
} __attribute__((packed));

struct rift_sensor_message {
	__u8 id;
	__le16 echo;
	__u8 num_samples;
	__le16 sample_count;
	__le16 temperature;			/* 10⁻² °C */
	__le32 timestamp;			/* µs, wraps every ~72 min */
	struct rift_imu_sample sample[2];
	__le16 mag[3];
	__le16 frame_count;			/* HDMI input frame count */
	__le32 frame_timestamp;			/* HDMI vsync timestamp */
	__u8 frame_id;				/* frame id pixel readback */
	__u8 led_pattern_phase;
	__le16 exposure_count;
	__le16 exposure_timestamp;
	__le16 reserved;
} __attribute__((packed));

#define RIFT_RADIO_MESSAGE_ID			0x0c

#define RIFT_REMOTE_BUTTON_UP			0x001
#define RIFT_REMOTE_BUTTON_DOWN			0x002
#define RIFT_REMOTE_BUTTON_LEFT			0x004
#define RIFT_REMOTE_BUTTON_RIGHT		0x008
#define RIFT_REMOTE_BUTTON_OK			0x010
#define RIFT_REMOTE_BUTTON_PLUS			0x020
#define RIFT_REMOTE_BUTTON_MINUS		0x040
#define RIFT_REMOTE_BUTTON_OCULUS		0x080
#define RIFT_REMOTE_BUTTON_BACK			0x100

struct rift_remote_message {
	__le16 buttons;
	__u8 unknown[56];
} __attribute__((packed));

#define RIFT_TOUCH_CONTROLLER_BUTTON_A		0x01
#define RIFT_TOUCH_CONTROLLER_BUTTON_X		0x01
#define RIFT_TOUCH_CONTROLLER_BUTTON_B		0x02
#define RIFT_TOUCH_CONTROLLER_BUTTON_Y		0x02
#define RIFT_TOUCH_CONTROLLER_BUTTON_MENU	0x04
#define RIFT_TOUCH_CONTROLLER_BUTTON_OCULUS	0x04
#define RIFT_TOUCH_CONTROLLER_BUTTON_STICK	0x08

#define RIFT_TOUCH_CONTROLLER_ADC_STICK		0x01
#define RIFT_TOUCH_CONTROLLER_ADC_B_Y		0x02
#define RIFT_TOUCH_CONTROLLER_ADC_TRIGGER	0x03
#define RIFT_TOUCH_CONTROLLER_ADC_A_X		0x04
#define RIFT_TOUCH_CONTROLLER_ADC_REST		0x08

struct rift_touch_message {
	__le32 timestamp;
	__le16 accel[3];
	__le16 gyro[3];
	__u8 buttons;
	__u8 trigger_grip_stick[5];
	__u8 adc_channel;
	__le16 adc_value;
	__u8 unknown[33];
} __attribute__((packed));

#define RIFT_REMOTE				1
#define RIFT_TOUCH_CONTROLLER_LEFT		2
#define RIFT_TOUCH_CONTROLLER_RIGHT		3

struct rift_radio_message {
	__u8 id;
	__u16 echo;
	__u8 unknown[2];
	__u8 device_type;
	union {
		struct rift_remote_message remote;
		struct rift_touch_message touch;
	};
} __attribute__((packed));

/* This message is sent in a 250 ms interval and contains all zeros */
#define RIFT_RADIO_UNKNOWN_MESSAGE_ID		0x0d

#endif /* __RIFT_HID_REPORTS__ */
