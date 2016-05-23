/*
 * HTC Vive USB HID reports
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_HID_REPORTS_H__
#define __VIVE_HID_REPORTS_H__

#include <asm/byteorder.h>

#define VIVE_MAINBOARD_STATUS_REPORT_ID			0x03

struct vive_mainboard_status_report {
	__u8 id;
	__le16 unknown;
	__u8 len;
	__le16 lens_separation;
	__le16 reserved1;
	__u8 button;
	__u8 reserved2[3];
	__u8 proximity_change;
	__u8 reserved3;
	__le16 proximity;
	__le16 ipd;
	__u8 reserved4[46];
} __attribute__((packed));

#define VIVE_HEADSET_POWER_REPORT_ID			0x04

#define VIVE_HEADSET_POWER_REPORT_TYPE			0x2978

struct vive_headset_power_report {
	__u8 id;
	__le16 type;
	__u8 len;
	__u8 unknown1[9];
	__u8 reserved1[32];
	__u8 unknown2;
	__u8 reserved2[18];
} __attribute__((packed));

#define VIVE_HEADSET_MAINBOARD_DEVICE_INFO_REPORT_ID	0x04

#define VIVE_HEADSET_MAINBOARD_DEVICE_INFO_REPORT_TYPE	0x2987

struct vive_headset_mainboard_device_info_report {
	__u8 id;
	__le16 type;
	__u8 len;
	__be16 edid_vid;
	__le16 edid_pid;
	__u8 unknown1[4];
	__le32 display_firmware_version;
	__u8 unknown2[48];
} __attribute__((packed));

#define VIVE_FIRMWARE_VERSION_REPORT_ID			0x05

struct vive_firmware_version_report {
	__u8 id;
	__le32 firmware_version;
	__le32 unknown1;
	__u8 string1[16];
	__u8 string2[16];
	__u8 hardware_version_micro;
	__u8 hardware_version_minor;
	__u8 hardware_version_major;
	__u8 hardware_revision;
	__le32 unknown2;
	__u8 fpga_version_minor;
	__u8 fpga_version_major;
	__u8 reserved[13];
} __attribute__((packed));

#define VIVE_HEADSET_IMU_REPORT_ID			0x20

struct vive_headset_imu_sample {
	__le16 acc[3];
	__le16 gyro[3];
	__le32 time;
	__u8 seq;
} __attribute__((packed));

struct vive_headset_imu_report {
	__u8 id;
	struct vive_headset_imu_sample sample[3];
} __attribute__((packed));

#define VIVE_HEADSET_LIGHTHOUSE_PULSE_REPORT_ID		0x21

struct vive_headset_lighthouse_pulse {
	__le16 id;
	__le16 duration;
	__le32 timestamp;
} __attribute__((packed));

struct vive_headset_lighthouse_pulse_report {
	__u8 id;
	struct vive_headset_lighthouse_pulse pulse[7];
	__u8 reserved;
} __attribute__((packed));

#define VIVE_CONTROLLER_REPORT1_ID			0x23

struct vive_controller_analog_trigger_message {
	__u8 squeeze;
	__u8 unknown[4];
} __attribute__((packed));

#define VIVE_CONTROLLER_BUTTON_TRIGGER			0x01
#define VIVE_CONTROLLER_BUTTON_TOUCH			0x02
#define VIVE_CONTROLLER_BUTTON_THUMB			0x04
#define VIVE_CONTROLLER_BUTTON_SYSTEM			0x08
#define VIVE_CONTROLLER_BUTTON_GRIP			0x10
#define VIVE_CONTROLLER_BUTTON_MENU			0x20

struct vive_controller_button_message {
	__u8 buttons;
	__u8 unknown[4];
} __attribute__((packed));

struct vive_controller_touchpad_move_message {
	__le16 pos[2];
	__u8 unknown[4];
} __attribute__((packed));

struct vive_controller_touchpad_updown_message {
	__u8 buttons;
	__le16 pos[2];
	__u8 unknown[4];
} __attribute__((packed));

struct vive_controller_imu_message {
	__u8 timestamp_3;
	__le16 accel[3];
	__le16 gyro[3];
	__u8 unknown[4];
} __attribute__((packed));

struct vive_controller_ping_message {
	__u8 charge;
	__u8 unknown1[2];
	__le16 accel[3];
	__le16 gyro[3];
	__u8 unknown2[5];
} __attribute__((packed));

struct vive_controller_message {
	__u8 timestamp_hi;
	__u8 type_hi;
	__u8 timestamp_lo;
	__u8 type_lo;
	union {
		struct vive_controller_analog_trigger_message analog_trigger;
		struct vive_controller_button_message button;
		struct vive_controller_touchpad_move_message touchpad_move;
		struct vive_controller_touchpad_updown_message touchpad_updown;
		struct vive_controller_imu_message imu;
		struct vive_controller_ping_message ping;
		__u8 unknown[25];
	};
} __attribute__((packed));

struct vive_controller_report1 {
	__u8 id;
	struct vive_controller_message message;
} __attribute__((packed));

#define VIVE_CONTROLLER_REPORT2_ID			0x24

struct vive_controller_report2 {
	__u8 id;
	struct vive_controller_message message[2];
} __attribute__((packed));

#define VIVE_CONTROLLER_DISCONNECT_REPORT_ID		0x26

#define VIVE_CONTROLLER_COMMAND_REPORT_ID		0xff

#define VIVE_CONTROLLER_POWEROFF_COMMAND		0x9f

struct vive_controller_poweroff_report {
	__u8 id;
	__u8 command;
	__u8 len;
	__u8 magic[4];
} __attribute__((packed));

#endif /* __VIVE_HID_REPORTS_H__ */
