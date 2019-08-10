/*
 * Microsoft HoloLens Sensors (Windows Mixed Reality) stereo camera
 * Copyright 2019 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <libusb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "hololens-camera2.h"
#include "debug.h"
#include "device.h"
#include "hidraw.h"
#include "usb-ids.h"

#define HOLOLENS_CAMERA2_WIDTH		1280
#define HOLOLENS_CAMERA2_HEIGHT		481
#define HOLOLENS_CAMERA2_FRAMERATE	90

#define HOLOLENS_INTERFACE_VIDEO	3
#define HOLOLENS_ENDPOINT_VIDEO		5

#define BULK_TRANSFER_SIZE		616538

struct _OuvrtHoloLensCamera2 {
	OuvrtDevice dev;

	libusb_device_handle *devh;
	int num_transfers;
	struct libusb_transfer **transfer;
	uint8_t endpoint;

	uint8_t last_seq;
	__u8 *frame;

	struct debug_stream *debug1;
	struct debug_stream *debug2;
};

G_DEFINE_TYPE(OuvrtHoloLensCamera2, ouvrt_hololens_camera2, \
	      OUVRT_TYPE_USB_DEVICE)

static int hololens_camera2_send(OuvrtHoloLensCamera2 *self, void *buf,
				 size_t len)
{
	struct libusb_transfer *transfer;
	uint8_t bEndpointAddress;
	void *data;

	transfer = libusb_alloc_transfer(0);
	if (!transfer)
		return -ENOMEM;

	data = g_memdup(buf, len);
	if (!data)
		return -ENOMEM;

	bEndpointAddress = self->endpoint | LIBUSB_ENDPOINT_OUT;
	libusb_fill_bulk_transfer(transfer, self->devh, bEndpointAddress,
				  data, len, NULL, NULL, 0);
	transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER |
			   LIBUSB_TRANSFER_FREE_TRANSFER;
	return libusb_submit_transfer(transfer);
}

static void hololens_camera2_handle_frame(OuvrtHoloLensCamera2 *self,
					  __u8 *buf, size_t len)
{
	uint16_t exposure;
	uint8_t seq;

	if (len != BULK_TRANSFER_SIZE) {
		g_print("%s: Wrong transfer size\n", self->dev.name);
		return;
	}

	/* Strip out packet headers */
	int j = 0;
	int n;
	for (unsigned int i = 0; i < len; i += 0x6000) {
		if (i + 0x20 >= len)
			break;
		if (i + 0x6000 >= len)
			n = len - i - 0x20;
		else
			n = 0x5fe0;
		memcpy(self->frame + j, buf + i + 0x20, n);
		j += n;
	}

        /* The first line contains metadata, possibly register values */
        exposure = __be16_to_cpup((__be16 *)(self->frame + 6));

	seq = self->frame[89];
	if ((int8_t)(seq - self->last_seq) != 1) {
		g_print("%s: Missing frame: %u -> %u\n", self->dev.name,
			self->last_seq, seq);
	}
	self->last_seq = seq;

	if (exposure == 300) {
		/* Bright frame, headset tracking */
		debug_stream_frame_push(self->debug1, self->frame,
					2 * 640 * 481 + 26,
					0, NULL, NULL, NULL, NULL);
	} else if (exposure == 0) {
		/* Dark frame, controller tracking */
		debug_stream_frame_push(self->debug2, self->frame,
					2 * 640 * 481 + 26,
					0, NULL, NULL, NULL, NULL);
	} else {
		g_print("%s: Unexpected exposure: %u\n", self->dev.name,
			exposure);
	}
}

static void hololens_camera2_transfer_callback(struct libusb_transfer *transfer)
{
	OuvrtHoloLensCamera2 *self = transfer->user_data;
	int ret;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
			g_print("%s: Device vanished\n", self->dev.name);
			self->dev.active = false;
		} else {
			g_print("%s: Sensor transfer error: %d (%s)\n",
				self->dev.name,
				transfer->status,
				libusb_error_name(transfer->status));
		}
		return;
	}

	if (transfer->actual_length != BULK_TRANSFER_SIZE)
		g_print("HoloLens camera: got %d bytes\n",
			transfer->actual_length);

	hololens_camera2_handle_frame(self, transfer->buffer,
				      transfer->actual_length);

	/* Resubmit transfer */
	ret = libusb_submit_transfer(transfer);
	if (ret < 0) {
		g_print("%s: Failed to resubmit bulk transfer: %d\n",
			self->dev.name, ret);
	}
}

#define HOLOLENS_CAMERA2_MAGIC	0x2b6f6c44

struct hololens_camera2_command {
	__le32 magic;
	__le32 len;
	__le32 command;
} __attribute__((packed));

static inline void hololens_camera2_set_active(OuvrtHoloLensCamera2 *self,
					       bool active)
{
	struct hololens_camera2_command command = {
		.magic = __cpu_to_le32(HOLOLENS_CAMERA2_MAGIC),
		.len = __cpu_to_le32(sizeof(command)),
		.command = __cpu_to_le32(active ? 0x81 : 0x82),
	};

	hololens_camera2_send(self, &command, sizeof(command));
}

struct hololens_camera2_unknown_command {
	__le32 magic;
	__le32 len;
	__le16 command;
	__le16 camera; /* 0 for left, 1 for right */
	__le16 unknown_6000;
	__le16 gain; /* observed 82 to 255 */
	__le16 second_camera; /* same as camera */
} __attribute__((packed));

/*
 * Sets gain for headset tracking frames of the left/right camera.
 *
 * Windows sends this repeatedly, usually in pairs with about three frames
 * between the left and right camera. Presumably this is part of a per-camera
 * auto-gain control loop. The set gain value is returned three times in the
 * metadata line for the corresponding camera. This seems to have no effect
 * on controller tracking frames.
 */
static inline void hololens_camera2_set_gain(OuvrtHoloLensCamera2 *self,
					     uint8_t camera, uint8_t gain)
{
	struct hololens_camera2_unknown_command command = {
		.magic = __cpu_to_le32(HOLOLENS_CAMERA2_MAGIC),
		.len = __cpu_to_le32(sizeof(command)),
		.command = __cpu_to_le16(0x80),
		.camera = __cpu_to_le16(camera),
		.unknown_6000 = __cpu_to_le16(6000),
		.gain = __cpu_to_le16(gain),
		.second_camera = __cpu_to_le16(camera),
	};

	hololens_camera2_send(self, &command, sizeof(command));
}

/*
 * Enables the stereo cameras.
 */
static int hololens_camera2_start(OuvrtDevice *dev)
{
	OuvrtHoloLensCamera2 *self = OUVRT_HOLOLENS_CAMERA2(dev);
	libusb_device_handle *devh;
	uint8_t bEndpointAddress;
	int ret;
	int i;

	devh = ouvrt_usb_device_get_handle(OUVRT_USB_DEVICE(dev));
	self->devh = devh;

	/* TODO: parse config descriptor */
	self->endpoint = HOLOLENS_ENDPOINT_VIDEO;

	ret = libusb_set_auto_detach_kernel_driver(devh, 1);
	if (ret) {
		g_print("%s: Failed to detach kernel drivers: %d\n", dev->name, ret);
		return ret;
	}

	ret = libusb_claim_interface(devh, HOLOLENS_INTERFACE_VIDEO);
	if (ret < 0) {
		g_print("%s: Failed to claim video interface: %d\n", dev->name, ret);
		return ret;
	}

	hololens_camera2_set_active(self, false);
	hololens_camera2_set_active(self, true);

	hololens_camera2_set_gain(self, 0, 0x20); /* left */
	hololens_camera2_set_gain(self, 1, 0x20); /* right */

	/* Submit two video frame transfers */
	self->num_transfers = 2;
	self->transfer = calloc(self->num_transfers, sizeof(*self->transfer));
	if (!self->transfer)
		return -ENOMEM;

	for (i = 0; i < self->num_transfers; i++) {
		self->transfer[i] = libusb_alloc_transfer(0);
		void *buf = calloc(1, BULK_TRANSFER_SIZE);
		bEndpointAddress = self->endpoint | LIBUSB_ENDPOINT_IN;
		libusb_fill_bulk_transfer(self->transfer[i], devh,
					  bEndpointAddress, buf, BULK_TRANSFER_SIZE,
					  hololens_camera2_transfer_callback,
					  self, 0);

		ret = libusb_submit_transfer(self->transfer[i]);
		if (ret < 0) {
			g_print("%s: Failed to submit bulk transfer %d\n",
				dev->name, i);
			return ret;
		}
	}

	static const struct debug_stream_desc desc1 = {
		.width = HOLOLENS_CAMERA2_WIDTH,
		.height = HOLOLENS_CAMERA2_HEIGHT,
		.format = FORMAT_GRAY,
		.framerate = { 30, 1 },
	};
	self->debug1 = debug_stream_new(&desc1);

	static const struct debug_stream_desc desc2 = {
		.width = HOLOLENS_CAMERA2_WIDTH,
		.height = HOLOLENS_CAMERA2_HEIGHT,
		.format = FORMAT_GRAY,
		.framerate = { 60, 1 },
	};
	self->debug2 = debug_stream_new(&desc2);

	return 0;
}

/*
 * Powers off the headset.
 */
static void hololens_camera2_stop(OuvrtDevice *dev)
{
	OuvrtHoloLensCamera2 *self = OUVRT_HOLOLENS_CAMERA2(dev);

	hololens_camera2_set_active(self, false);
	debug_stream_unref(self->debug2);
	debug_stream_unref(self->debug1);
	libusb_release_interface(self->devh, HOLOLENS_INTERFACE_VIDEO);
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_hololens_camera2_finalize(GObject *object)
{
	OuvrtHoloLensCamera2 *self = OUVRT_HOLOLENS_CAMERA2(object);

	free(self->frame);
	G_OBJECT_CLASS(ouvrt_hololens_camera2_parent_class)->finalize(object);
}

static void ouvrt_hololens_camera2_class_init(OuvrtHoloLensCamera2Class *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_hololens_camera2_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = hololens_camera2_start;
	OUVRT_DEVICE_CLASS(klass)->stop = hololens_camera2_stop;
}

static void ouvrt_hololens_camera2_init(OuvrtHoloLensCamera2 *self)
{
	ouvrt_usb_device_set_vid_pid(OUVRT_USB_DEVICE(self), VID_MICROSOFT,
				     PID_HOLOLENS_SENSORS);

	self->dev.type = DEVICE_TYPE_CAMERA;
	self->frame = malloc(615706); /* 2*640*481 + 26 */
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated HoloLens camera device.
 */
OuvrtDevice *hololens_camera2_new(G_GNUC_UNUSED const char *devnode)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_HOLOLENS_CAMERA2, NULL));
}
