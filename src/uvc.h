/*
 * UVC Controls
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <asm/byteorder.h>
#include <libusb.h>
#include <stdbool.h>
#include <stdint.h>

#define VS_PROBE_CONTROL	1
#define VS_COMMIT_CONTROL	2

struct uvc_probe_commit_control {
	__le16 bmHint;
	__u8 bFormatIndex;
	__u8 bFrameIndex;
	__le32 dwFrameInterval;
	__le16 wKeyFrameRate;
	__le16 wPFrameRate;
	__le16 wCompQuality;
	__le16 wCompWindowSize;
	__le16 wDelay;
	__le32 dwMaxVideoFrameSize;
	__le32 dwMaxPayloadTransferSize;
	__le32 dwClockFrequency;
	__u8 bmFramingInfo;
	__u8 bPreferedVersion;
	__u8 bMinVersion;
	__u8 bMaxVersion;
} __attribute__((packed));

struct uvc_payload_header {
	__u8 bHeaderLength;
	__u8 bmHeaderInfo;
	__le32 dwPresentationTime;
	__u8 scrSourceClock[6];
} __attribute__((packed));

#define ASSERT_SIZE(report,size) \
        _Static_assert((sizeof(struct report) == size), \
                       "incorrect size: " #report)

ASSERT_SIZE(uvc_probe_commit_control, 34);
ASSERT_SIZE(uvc_payload_header, 12);

int uvc_set_cur(libusb_device_handle *dev, uint8_t interface, uint8_t entity,
		uint8_t selector, void *data, uint16_t wLength);
int uvc_get_cur(libusb_device_handle *dev, uint8_t interface, uint8_t entity,
		uint8_t selector, void *data, uint16_t wLength);
int uvc_get_len(libusb_device_handle *dev, uint8_t interface, uint8_t entity,
		uint8_t selector, uint16_t *wLength);
