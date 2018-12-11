/*
 * Oculus Rift Sensor (CV1 external camera)
 * Copyright 2017-2018 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <libusb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib-object.h>

#include "rift-sensor.h"
#include "device.h"
#include "esp770u.h"
#include "ar0134.h"
#include "usb-ids.h"
#include "uvc.h"
#include "debug.h"

#define RIFT_SENSOR_WIDTH	1280
#define RIFT_SENSOR_HEIGHT	960
#define RIFT_SENSOR_FRAME_SIZE	(RIFT_SENSOR_WIDTH * RIFT_SENSOR_HEIGHT)
#define RIFT_SENSOR_FRAMERATE	52

#define RIFT_SENSOR_VS_PROBE_CONTROL_SIZE	26

#define UVC_INTERFACE_CONTROL	0
#define UVC_INTERFACE_DATA	1

struct _OuvrtRiftSensor {
	OuvrtDevice dev;

	libusb_device_handle *devh;
	int num_transfers;
	struct libusb_transfer **transfer;
	uint8_t endpoint;

	char *version;
	bool sync;

	unsigned char *frame;
	int frame_size;
	int payload_size;
	int frame_id;
	uint32_t pts;
	uint64_t time;
	int64_t dt;

	OuvrtTracker *tracker;
	uint32_t radio_id;
	struct debug_stream *debug;
};

G_DEFINE_TYPE(OuvrtRiftSensor, ouvrt_rift_sensor, OUVRT_TYPE_USB_DEVICE)

static int rift_sensor_get_calibration(OuvrtRiftSensor *self)
{
	uint8_t buf[128];
	double fx, fy, cx, cy;
	double k1, k2, k3, k4;
	int ret;

	/* Read a 128-byte block at EEPROM address 0x1d000 */
	ret = esp770u_flash_read(self->devh, 0x1d000, buf, sizeof buf);
	if (ret < 0)
		return ret;

	fx = fy = *(float *)(buf + 0x30);
	cx = *(float *)(buf + 0x34);
	cy = *(float *)(buf + 0x38);

	k1 = *(float *)(buf + 0x48);
	k2 = *(float *)(buf + 0x4c);
	k3 = *(float *)(buf + 0x50);
	k4 = *(float *)(buf + 0x54);

	g_print(" f = [ %7.3f %7.3f ], c = [ %7.3f %7.3f ]\n", fx, fy, cx, cy);
	g_print(" k = [ %9.6f %9.6f %9.6f %9.6f ]\n", k1, k2, k3 ,k4);

	return 0;
}

/*
 * Opens the USB device.
 */
static int rift_sensor_open(OuvrtDevice *dev)
{
	OuvrtRiftSensor *self = OUVRT_RIFT_SENSOR(dev);
	libusb_device_handle *devh;
	uint8_t firmware_version;
	int ret;

	ret = OUVRT_DEVICE_CLASS(ouvrt_rift_sensor_parent_class)->open(dev);
	if (ret < 0)
		return ret;

	devh = ouvrt_usb_device_get_handle(OUVRT_USB_DEVICE(dev));
	self->devh = devh;

	ret = libusb_set_auto_detach_kernel_driver(devh, 1);
	if (ret) {
		g_print("%s: Failed to detach kernel drivers: %d\n", dev->name,
			ret);
		return ret;
	}

	ret = libusb_claim_interface(devh, UVC_INTERFACE_CONTROL);
	if (ret < 0) {
		g_print("%s: Failed to claim control interface: %d\n",
			dev->name, ret);
		return ret;
	}

	ret = esp770u_query_firmware_version(devh, &firmware_version);
	if (ret < 0) {
		g_print("%s: Failed to query firmware version: %d\n",
			dev->name, errno);
		return ret;
	}
	g_print("%s: Firmware version %d\n", dev->name, firmware_version);

	ret = esp770u_init_unknown(devh);
	if (ret < 0) {
		g_print("%s: Failed to initialize: %d\n", dev->name, errno);
		return ret;
	}

	ret = esp770u_init_radio(devh);
	if (ret < 0) {
		g_print("%s: Failed to initialize radio: %d\n", dev->name,
			errno);
		return ret;
	}

	ret = rift_sensor_get_calibration(self);
	if (ret < 0) {
		g_print("%s: Failed to read calibration data: %d\n", dev->name,
			errno);
		return ret;
	}

	return 0;
}

static void default_frame_callback(OuvrtRiftSensor *self)
{
	struct timespec tp;
	double timestamps[4] = { 0 };

	clock_gettime(CLOCK_MONOTONIC, &tp);
	timestamps[1] = tp.tv_sec + 1e-9 * tp.tv_nsec;

	/*
	 * Find bright blobs in the camera image and identify individual LEDs
	 * using the estimated pose at time of exposure or, if that is not
	 * available, using the LED blinking pattern.
	 */
	struct blobservation *ob = NULL;
	if (self->tracker) {
		ouvrt_tracker_process_frame(self->tracker,
					    self->frame, RIFT_SENSOR_WIDTH,
					    RIFT_SENSOR_HEIGHT, self->time,
					    &ob);
	}

	clock_gettime(CLOCK_MONOTONIC, &tp);
	timestamps[2] = tp.tv_sec + 1e-9 * tp.tv_nsec;

	dquat rot = { 0 };
	dvec3 trans = { 0 };

	/* TODO: calculate pose */

	clock_gettime(CLOCK_MONOTONIC, &tp);
	timestamps[3] = tp.tv_sec + 1e-9 * tp.tv_nsec;

	debug_stream_frame_push(self->debug, self->frame,
				RIFT_SENSOR_WIDTH * RIFT_SENSOR_HEIGHT +
				sizeof(struct ouvrt_debug_attachment),
				RIFT_SENSOR_WIDTH * RIFT_SENSOR_HEIGHT,
				ob, &rot, &trans, timestamps);
}

enum process_payload_return {
	PAYLOAD_EMPTY,
	PAYLOAD_INVALID,
	PAYLOAD_OVERFLOW,
	PAYLOAD_FRAME_PARTIAL,
	PAYLOAD_FRAME_COMPLETE
};

enum process_payload_return process_payload(OuvrtRiftSensor *self,
					    unsigned char *payload, size_t len)
{
	struct uvc_payload_header *h = (struct uvc_payload_header *)payload;
	int payload_len;
	int frame_id;
	uint32_t pts;
	bool error;

	if (len == 0 || len == sizeof(struct uvc_payload_header))
		return PAYLOAD_EMPTY;

	if (h->bHeaderLength == 0) {
		/* This happens when unplugging the camera */
		return PAYLOAD_INVALID;
	}

	if (h->bHeaderLength != 12) {
		g_print("%s: Invalid header length: %u (%ld)\n", self->dev.name,
			h->bHeaderLength, len);
		return PAYLOAD_INVALID;
	}

	payload += h->bHeaderLength;
	payload_len = len - h->bHeaderLength;
	frame_id = h->bmHeaderInfo & 0x01;
	error = h->bmHeaderInfo & 0x40;

	if (error) {
		g_print("%s: Frame error\n", self->dev.name);
		return PAYLOAD_INVALID;
	}

	pts = __le32_to_cpu(h->dwPresentationTime);
	if (self->payload_size == 0)
		self->pts = pts;

	if (frame_id != self->frame_id) {
		struct timespec ts;
		uint64_t time;

		if (self->payload_size != self->frame_size) {
			g_print("%s: Dropping short frame: %u\n",
				self->dev.name, self->payload_size);
		}

		/* Start of new frame */
		clock_gettime(CLOCK_MONOTONIC, &ts);
		time = ts.tv_sec * 1000000000 + ts.tv_nsec;
		self->dt = time - self->time;

		self->frame_id = frame_id;
		self->pts = pts;
		self->time = time;
		self->payload_size = 0;
	} else {
		if (pts != self->pts) {
			g_print("%s: PTS changed in-frame at %u!\n",
				self->dev.name, self->payload_size);
			self->pts = pts;
		}
	}

	if (self->payload_size + payload_len > self->frame_size) {
		g_print("%s: Frame buffer overflow: %u %u %u\n", self->dev.name,
		       self->payload_size, payload_len, self->frame_size);
		return PAYLOAD_OVERFLOW;
	}

	memcpy(self->frame + self->payload_size, payload, payload_len);
	self->payload_size += payload_len;

	return (self->payload_size == self->frame_size) ?
	       PAYLOAD_FRAME_COMPLETE : PAYLOAD_FRAME_PARTIAL;
}

static void iso_transfer_cb(struct libusb_transfer *transfer)
{
	OuvrtRiftSensor *self = transfer->user_data;
	OuvrtDevice *dev = OUVRT_DEVICE(self);
	int ret;
	int i;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
			if (dev->active)
				g_print("%s: Device vanished\n", dev->name);
			dev->active = false;
		} else {
			g_print("%s: Transfer error: %d (%s)\n",
				dev->name, transfer->status,
				libusb_error_name(transfer->status));
		}
		return;
	}

	/* Handle contained isochronous packets */
	for (i = 0; i < transfer->num_iso_packets; i++) {
		enum process_payload_return ret;
		unsigned char *payload;
		size_t payload_len;

		payload = libusb_get_iso_packet_buffer_simple(transfer, i);
		payload_len = transfer->iso_packet_desc[i].actual_length;
		ret = process_payload(self, payload, payload_len);

		if (ret == PAYLOAD_FRAME_COMPLETE)
			default_frame_callback(self);
	}


	/* Resubmit transfer */
	ret = libusb_submit_transfer(transfer);
	if (ret < 0) {
		g_print("%s: Failed to resubmit: %d\n", dev->name, ret);
		dev->active = false;
	}
}

/*
 * Starts streaming and sets up the sensor for the exposure synchronization
 * signal from the Rift CV1.
 *
 * Returns 0 on success, negative values on error.
 */
static int rift_sensor_start(OuvrtDevice *dev)
{
	OuvrtRiftSensor *self = OUVRT_RIFT_SENSOR(dev);
	libusb_device_handle *devh = self->devh;
	const int num_packets = 24;
	const int packet_size = 16384;
	const int alt_setting = 2;
	struct uvc_probe_commit_control probe = {
		.bFormatIndex = 1,
		.bFrameIndex = 4,
		.dwFrameInterval = __cpu_to_le32(192000),
		.dwMaxVideoFrameSize = __cpu_to_le32(RIFT_SENSOR_FRAME_SIZE),
		.dwMaxPayloadTransferSize = __cpu_to_le16(3072),
	};
	struct uvc_probe_commit_control expect = {
		.bFormatIndex = 1,
		.bFrameIndex = 4,
		.dwFrameInterval = __cpu_to_le32(200000),
		.dwMaxVideoFrameSize = __cpu_to_le32(RIFT_SENSOR_FRAME_SIZE),
		.dwMaxPayloadTransferSize = __cpu_to_le16(8192),
	};
	struct uvc_probe_commit_control commit;
	int ret;

	ret = libusb_claim_interface(devh, 1);
	if (ret < 0) {
		g_print("%s: Failed to claim data interface: %d\n", dev->name,
			ret);
		return ret;
	}

	uint16_t len = RIFT_SENSOR_VS_PROBE_CONTROL_SIZE;

	ret = uvc_set_cur(devh, 1, 0, VS_PROBE_CONTROL, &probe, len);
	if (ret < 0) {
		g_print("%s: Failed to set PROBE: %d\n", dev->name, ret);
		return ret;
	}

	ret = uvc_get_cur(devh, 1, 0, VS_PROBE_CONTROL, &commit, len);
	if (ret < 0) {
		g_print("%s: Failed to get PROBE: %d\n", dev->name, ret);
		return ret;
	}

	if (memcmp(&expect, &commit, len) != 0) {
		g_print("%s: PROBE result differs\n"
			"\tbmHint = %u\n"
			"\tbFormatIndex = %u\n"
			"\tbFrameIndex = %u\n"
			"\tdwFrameInterval = %u\n"
			"\twCompQuality = %u\n"
			"\tdwMaxVideoFrameSize = %u\n"
			"\tdwMaxPayloadTransferSize = %u\n",
			dev->name,
			commit.bmHint,
			commit.bFormatIndex,
			commit.bFrameIndex,
			commit.dwFrameInterval,
			commit.wCompQuality,
			commit.dwMaxVideoFrameSize,
			commit.dwMaxPayloadTransferSize);
	}

	ret = uvc_set_cur(devh, 1, 0, VS_COMMIT_CONTROL, &commit, len);
	if (ret < 0) {
		g_print("%s: Failed to set COMMIT\n", dev->name);
		return ret;
	}

	ret = libusb_set_interface_alt_setting(devh, 1, alt_setting);
	if (ret) {
		g_print("%s: Failed to set interface alt setting\n", dev->name);
		return ret;
	}

	self->frame_size = RIFT_SENSOR_FRAME_SIZE;
	self->frame = calloc(1, self->frame_size +
			     sizeof(struct ouvrt_debug_attachment));
	if (!self->frame)
		return -ENOMEM;

	self->num_transfers = 7; /* enough for a single frame */
	self->transfer = calloc(self->num_transfers, sizeof(*self->transfer));
	if (!self->transfer)
		return -ENOMEM;

	for (int i = 0; i < self->num_transfers; i++) {
		self->transfer[i] = libusb_alloc_transfer(32);
		if (!self->transfer[i])
			return -ENOMEM;

		uint8_t bEndpointAddress = 1 | LIBUSB_ENDPOINT_IN;

		int transfer_size = num_packets * packet_size;
		void *buf = malloc(transfer_size);
		libusb_fill_iso_transfer(self->transfer[i], devh,
					 bEndpointAddress, buf, transfer_size,
					 num_packets, iso_transfer_cb, self,
					 1000);
		libusb_set_iso_packet_lengths(self->transfer[i], packet_size);

		ret = libusb_submit_transfer(self->transfer[i]);
		if (ret < 0) {
			g_print("%s: Failed to submit iso transfer %d\n",
				dev->name, i);
			return ret;
		}
	}

	self->debug = debug_stream_new(RIFT_SENSOR_WIDTH, RIFT_SENSOR_HEIGHT,
				       RIFT_SENSOR_FRAMERATE);

	return 0;
}

/*
 * Initializes the sensors and handles USB transfers.
 */
static void rift_sensor_thread(OuvrtDevice *dev)
{
	OuvrtRiftSensor *self = OUVRT_RIFT_SENSOR(dev);
	int ret;

	usleep(1000000);

	ret = ar0134_init(self->devh);
	if (ret < 0) {
		g_print("%s: Failed to initialize AR0134 sensor\n", dev->name);
		return;
	}

	/* Initialize the AR0134 sensor for CV1 tracking */
	if (self->tracker) {
		g_print("%s: Synchronised exposure\n", dev->name);
		/* Enable synchronised exposure by default */
		ret = ar0134_set_sync(self->devh, true);
		if (ret < 0)
			return;

		self->radio_id = ouvrt_tracker_get_radio_address(self->tracker);
		if (self->radio_id) {
			ret = esp770u_setup_radio(self->devh,
						  self->radio_id);
			if (ret < 0)
				return;
		}
	} else {
		g_print("%s: Automatic exposure\n", dev->name);
		ret = ar0134_set_ae(self->devh, true);
		if (ret < 0)
			return;
	}

	OUVRT_DEVICE_CLASS(ouvrt_rift_sensor_parent_class)->thread(dev);
}

static void rift_sensor_stop(OuvrtDevice *dev)
{
	OuvrtRiftSensor *self = OUVRT_RIFT_SENSOR(dev);

	g_print("%s: Stop\n", dev->name);

	debug_stream_unref(self->debug);
	libusb_release_interface(self->devh, UVC_INTERFACE_CONTROL);
}

/*
 * Frees common fields of the device structure. To be called from the device
 * specific free operation.
 */
static void ouvrt_rift_sensor_finalize(GObject *object)
{
	OuvrtRiftSensor *self = OUVRT_RIFT_SENSOR(object);

	g_object_unref(self->tracker);
}

static void ouvrt_rift_sensor_class_init(OuvrtRiftSensorClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_rift_sensor_finalize;

	OUVRT_DEVICE_CLASS(klass)->open = rift_sensor_open;
	OUVRT_DEVICE_CLASS(klass)->start = rift_sensor_start;
	OUVRT_DEVICE_CLASS(klass)->thread = rift_sensor_thread;
	OUVRT_DEVICE_CLASS(klass)->stop = rift_sensor_stop;
}

static void ouvrt_rift_sensor_init(OuvrtRiftSensor *self)
{
	ouvrt_usb_device_set_vid_pid(OUVRT_USB_DEVICE(self), VID_OCULUSVR,
				     PID_RIFT_SENSOR);
	self->sync = false;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated Oculus Sensor device.
 */
OuvrtDevice *rift_sensor_new(G_GNUC_UNUSED const char *devnode)
{
	OuvrtRiftSensor *camera;

	camera = g_object_new(OUVRT_TYPE_RIFT_SENSOR, NULL);
	if (!camera)
		return NULL;

	return OUVRT_DEVICE(camera);
}

void ouvrt_rift_sensor_set_sync_exposure(OuvrtRiftSensor *self, gboolean sync)
{
	int ret;

	if (sync == self->sync)
		return;

	self->sync = sync;

	if (!self->dev.active)
		return;

	if (sync) {
		ret = ar0134_set_ae(self->devh, false);
		if (ret < 0)
			return;

		ret = ar0134_set_sync(self->devh, true);
		if (ret < 0)
			return;
	} else {
		ret = ar0134_set_sync(self->devh, false);
		if (ret < 0)
			return;

		ret = ar0134_set_ae(self->devh, true);
		if (ret < 0)
			return;
	}
}

void ouvrt_rift_sensor_set_tracker(OuvrtRiftSensor *self, OuvrtTracker *tracker)
{
	int ret;

	if (self->devh) {
		if (tracker && !self->tracker) {
			g_print("%s: Synchronised exposure\n", self->dev.name);
			ouvrt_rift_sensor_set_sync_exposure(self, true);

			self->radio_id = ouvrt_tracker_get_radio_address(tracker);
			if (self->radio_id) {
				ret = esp770u_setup_radio(self->devh,
							  self->radio_id);
				if (ret < 0)
					return;
			}
		} else if (!tracker && self->tracker) {
			g_print("%s: Automatic exposure\n", self->dev.name);
			ouvrt_rift_sensor_set_sync_exposure(self, false);
		}
	}
	g_set_object(&self->tracker, tracker);
}
