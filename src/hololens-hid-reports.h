/*
 * Microsoft HoloLens Sensors USB HID reports
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __HOLOLENS_HID_REPORTS__
#define __HOLOLENS_HID_REPORTS__

#include <asm/byteorder.h>

#define HOLOLENS_IMU_REPORT_ID				0x01
#define HOLOLENS_IMU_REPORT_SIZE			497
#define HOLOLENS_IMU_REPORT_SIZE_V2			381

struct hololens_imu_message {
	__u8 code;
	__u8 text[57];
} __attribute__((packed));

struct hololens_imu_report {
	__u8 id;
	__le16 temperature[4];
	__le64 gyro_timestamp[4];
	__le16 gyro[3][32];
	__le64 accel_timestamp[4];
	__le32 accel[3][4];
	__le64 video_timestamp[4];
	__u8 video_metadata[36];
	struct hololens_imu_message message[2];
} __attribute__((packed));

#define HOLOLENS_COMMAND_CONFIG_START			0x0b
#define HOLOLENS_COMMAND_CONFIG_META			0x06
#define HOLOLENS_COMMAND_CONFIG_DATA			0x04
#define HOLOLENS_COMMAND_CONFIG_READ			0x08
#define HOLOLENS_COMMAND_START_IMU			0x07

#define HOLOLENS_CONTROL_REPORT_ID			0x02
#define HOLOLENS_CONTROL_REPORT_SIZE			33

struct hololens_control_report {
	__u8 id;
	__u8 code;
	__u8 len;
	__u8 data[30];
} __attribute__((packed));

#define HOLOLENS_DEBUG_REPORT_ID			0x03
#define HOLOLENS_DEBUG_REPORT_SIZE			497

struct hololens_debug_message {
	__le32 unknown1;
	__le16 unknown2;
	__u8 code;
	__u8 text[56];
} __attribute__((packed));

struct hololens_debug_report {
	__u8 id;
	__le32 unknown;
	struct hololens_debug_message message[3];
	__u8 zeros[303];
} __attribute__((packed));

#define ASSERT_SIZE(report,size) \
	_Static_assert((sizeof(struct report) == size), \
		       "incorrect size: " #report)

ASSERT_SIZE(hololens_imu_report, HOLOLENS_IMU_REPORT_SIZE);
ASSERT_SIZE(hololens_control_report, HOLOLENS_CONTROL_REPORT_SIZE);
ASSERT_SIZE(hololens_debug_report, HOLOLENS_DEBUG_REPORT_SIZE);

#endif /* __HOLOLENS_HID_REPORTS__ */
