/*
 * HTC Vive USB HID reports
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __VIVE_HID_REPORTS_H__
#define __VIVE_HID_REPORTS_H__

#include <asm/byteorder.h>

#define VIVE_CONTROLLER_BUTTON_REPORT_ID		0x01

#define VIVE_CONTROLLER_USB_BUTTON_TRIGGER		(1 << 0)
#define VIVE_CONTROLLER_USB_BUTTON_GRIP			(1 << 2)
#define VIVE_CONTROLLER_USB_BUTTON_MENU			(1 << 12)
#define VIVE_CONTROLLER_USB_BUTTON_SYSTEM		(1 << 13)
#define VIVE_CONTROLLER_USB_BUTTON_THUMB		(1 << 18)
#define VIVE_CONTROLLER_USB_BUTTON_TOUCH		(1 << 20)

struct vive_controller_button_report {
	__u8 id;
	__u8 unknown1;
	__le16 maybe_type;
	__le32 sequence;
	__le32 buttons;
	union {
		__le16 trigger;
		__le16 battery_voltage;
	};
	__u8 battery;
	__u8 unknown2;
	__le32 hardware_id;
	__le16 touch[2];
	__le16 unknown3;
	__le16 trigger_hires;
	__u8 unknown4[24];
	__le16 trigger_raw;
	__u8 unknown5[8];
	__u8 maybe_bitfield;
	__u8 unknown6;
} __attribute__((packed));

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

#define VIVE_IMU_REPORT_ID				0x20

struct vive_imu_sample {
	__le16 acc[3];
	__le16 gyro[3];
	__le32 time;
	__u8 seq;
} __attribute__((packed));

struct vive_imu_report {
	__u8 id;
	struct vive_imu_sample sample[3];
} __attribute__((packed));

#define VIVE_CONTROLLER_LIGHTHOUSE_PULSE_REPORT_ID	0x21

struct vive_controller_lighthouse_pulse {
	__le16 id;
	__le16 duration;
	__le32 timestamp;
} __attribute__((packed));

struct vive_controller_lighthouse_pulse_report {
	__u8 id;
	struct vive_controller_lighthouse_pulse pulse[7];
	__u8 reserved;
} __attribute__((packed));

#define VIVE_CONTROLLER_REPORT1_ID			0x23

#define VIVE_CONTROLLER_BATTERY_CHARGING		0x80
#define VIVE_CONTROLLER_BATTERY_CHARGE_MASK		0x7f

#define VIVE_CONTROLLER_BUTTON_TRIGGER			0x01
#define VIVE_CONTROLLER_BUTTON_TOUCH			0x02
#define VIVE_CONTROLLER_BUTTON_THUMB			0x04
#define VIVE_CONTROLLER_BUTTON_SYSTEM			0x08
#define VIVE_CONTROLLER_BUTTON_GRIP			0x10
#define VIVE_CONTROLLER_BUTTON_MENU			0x20

struct vive_controller_message {
	__u8 timestamp_hi;
	__u8 len;
	__u8 timestamp_lo;
	__u8 payload[26];
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

#define VIVE_HEADSET_LIGHTHOUSE_PULSE_REPORT_ID		0x25

struct vive_headset_lighthouse_pulse {
	__u8 id;
	__le16 duration;
	__le32 timestamp;
} __attribute__((packed));

struct vive_headset_lighthouse_pulse_report {
	__u8 id;
	struct vive_headset_lighthouse_pulse pulse[9];
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
