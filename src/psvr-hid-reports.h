/*
 * Sony Playstation VR Headset USB HID reports
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#ifndef __PSVR_HID_REPORTS__
#define __PSVR_HID_REPORTS__

#include <asm/byteorder.h>

#define PSVR_CONTROL_MAGIC				0xaa

#define PSVR_ENABLE_VR_TRACKING_REPORT_ID		0x11
#define PSVR_ENABLE_VR_TRACKING_REPORT_SIZE		12

#define PSVR_ENABLE_VR_TRACKING_DATA_1			0xffffff00
#define PSVR_ENABLE_VR_TRACKING_DATA_2			0x00000000

struct psvr_enable_vr_tracking_report {
	__u8 id;
	__u8 unknown;
	__u8 magic;
	__u8 payload_length;
	__le32 payload[2];
} __attribute__((packed));

#define PSVR_HEADSET_POWER_REPORT_ID			0x17
#define PSVR_HEADSET_POWER_REPORT_SIZE			8

#define PSVR_HEADSET_POWER_ON				1
#define PSVR_HEADSET_POWER_OFF				0

struct psvr_headset_power_report {
	__u8 id;
	__u8 unknown;
	__u8 magic;
	__u8 payload_length;
	__le32 payload;
} __attribute__((packed));

#define PSVR_PROCESSING_BOX_POWER_REPORT_ID		0x13
#define PSVR_PROCESSING_BOX_POWER_REPORT_SIZE		8

#define PSVR_PROCESSING_BOX_POWER_ON			0
#define PSVR_PROCESSING_BOX_POWER_OFF			1

struct psvr_processing_box_power_report {
	__u8 id;
	__u8 unknown;
	__u8 magic;
	__u8 payload_length;
	__le32 payload;
} __attribute__((packed));

#define PSVR_SET_MODE_REPORT_ID				0x23
#define PSVR_SET_MODE_REPORT_SIZE			8

#define PSVR_MODE_VR					1
#define PSVR_MODE_CINEMATIC				0

struct psvr_set_mode_report {
	__u8 id;
	__u8 unknown;
	__u8 magic;
	__u8 payload_length;
	__le32 payload;
} __attribute__((packed));

#define PSVR_SENSOR_MESSAGE_SIZE			64

/*
 * pressed:   none down up  mute
 * button     0    2    4   8
 * button_raw 1023 579  452 323
 */
#define PSVR_BUTTON_VOLUME_UP	2
#define PSVR_BUTTON_VOLUME_DOWN	4
#define PSVR_BUTTON_MIC_MUTE	8

#define PSVR_STATE_POWER_BUTTON_PRESSED	1
#define PSVR_STATE_POWER_UP		2
#define PSVR_STATE_POWER_ON		3
#define PSVR_STATE_RUNNING		4
#define PSVR_STATE_POWER_DOWN		5
#define PSVR_STATE_POWER_OFF		6

#define PSVR_STATUS_WORN		0x01
#define PSVR_STATUS_DISPLAY_ACTIVE	0x02
#define PSVR_STATUS_MUTED		0x08
#define PSVR_STATUS_EARPHONES_CONNECTED	0x10

struct psvr_imu_sample {
	__le32 timestamp;
	__le16 gyro[3];
	__le16 accel[3];
} __attribute__((packed));

struct psvr_sensor_message {
	__u8 button;
	__u8 zero;
	__le16 volume;
	__u8 unknown1;
	__u8 state;
	__u8 unknown2[10];
	struct psvr_imu_sample sample[2];
	__u8 unknown3[5];
	__be16 button_raw;
	__le16 proximity;
	__u8 unknown4[6];
	__u8 sequence;
} __attribute__((packed));

#define ASSERT_SIZE(report,size) \
	_Static_assert((sizeof(struct report) == size), \
		       "incorrect size: " #report)

ASSERT_SIZE(psvr_enable_vr_tracking_report, PSVR_ENABLE_VR_TRACKING_REPORT_SIZE);
ASSERT_SIZE(psvr_headset_power_report, PSVR_HEADSET_POWER_REPORT_SIZE);
ASSERT_SIZE(psvr_processing_box_power_report, PSVR_PROCESSING_BOX_POWER_REPORT_SIZE);
ASSERT_SIZE(psvr_set_mode_report, PSVR_SET_MODE_REPORT_SIZE);
ASSERT_SIZE(psvr_sensor_message, PSVR_SENSOR_MESSAGE_SIZE);

#endif /* __PSVR_HID_REPORTS__ */
