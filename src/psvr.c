/*
 * Sony PlayStation VR Headset
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <errno.h>
#include <libusb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "psvr.h"
#include "psvr-hid-reports.h"
#include "device.h"
#include "hidraw.h"
#include "imu.h"
#include "telemetry.h"
#include "usb-ids.h"

#define PSVR_CONFIG_DESCRIPTOR		1

#define PSVR_INTERFACE_SENSOR		4
#define PSVR_INTERFACE_CONTROL		5

#define PSVR_ENDPOINT_SENSOR		3
#define PSVR_ENDPOINT_CONTROL		4

struct _OuvrtPSVR {
	OuvrtDevice dev;

	libusb_device_handle *devh;
	int num_transfers;
	struct libusb_transfer **transfer;
	uint8_t sensor_endpoint;
	uint8_t control_endpoint;

	bool power;
	bool vrmode;
	uint8_t button;
	uint8_t state;
	uint8_t last_seq;
	uint32_t last_timestamp;
	struct imu_state imu;
	vec3 acc_bias;
	vec3 acc_scale;

	uint8_t status_flags;
	uint8_t volume;

	uint8_t *calibration;
};

G_DEFINE_TYPE(OuvrtPSVR, ouvrt_psvr, OUVRT_TYPE_USB_DEVICE)

static int psvr_control_send(OuvrtPSVR *psvr, void *buf, size_t len)
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

	bEndpointAddress = psvr->control_endpoint | LIBUSB_ENDPOINT_OUT;
	libusb_fill_bulk_transfer(transfer, psvr->devh, bEndpointAddress,
				  data, len, NULL, NULL, 0);
	transfer->flags |= LIBUSB_TRANSFER_FREE_BUFFER |
			   LIBUSB_TRANSFER_FREE_TRANSFER;
	return libusb_submit_transfer(transfer);
}

static void psvr_set_processing_box_power(OuvrtPSVR *psvr, bool power)
{
	struct psvr_processing_box_power_report report = {
		.id = PSVR_PROCESSING_BOX_POWER_REPORT_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 4,
		.payload = __cpu_to_le32(power ? PSVR_PROCESSING_BOX_POWER_ON :
						 PSVR_PROCESSING_BOX_POWER_OFF),
	};
	int ret;

	ret = psvr_control_send(psvr, &report, sizeof(report));
	if (ret < 0)
		g_print("PSVR: Failed to set processing box power: %d\n", ret);
}

static void psvr_set_headset_power(OuvrtPSVR *psvr, bool power)
{
	struct psvr_headset_power_report report = {
		.id = PSVR_HEADSET_POWER_REPORT_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 4,
		.payload = __cpu_to_le32(power ? PSVR_HEADSET_POWER_ON :
						 PSVR_HEADSET_POWER_OFF),
	};
	int ret;

	ret = psvr_control_send(psvr, &report, sizeof(report));
	if (ret < 0)
		g_print("PSVR: Failed to set headset power: %d\n", ret);
}

/*
 * Switches into VR mode enables the tracking LEDs.
 */
static void psvr_enable_vr_tracking(OuvrtPSVR *psvr)
{
	struct psvr_enable_vr_tracking_report report = {
		.id = PSVR_ENABLE_VR_TRACKING_REPORT_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 8,
		.payload = {
			__cpu_to_le32(PSVR_ENABLE_VR_TRACKING_DATA_1),
			__cpu_to_le32(PSVR_ENABLE_VR_TRACKING_DATA_2),
		},
	};
	int ret;

	ret = psvr_control_send(psvr, &report, sizeof(report));
	if (ret < 0)
		g_print("PSVR: Failed to enable VR tracking: %d\n", ret);
}

static void psvr_set_mode(OuvrtPSVR *psvr, int mode)
{
	struct psvr_set_mode_report report = {
		.id = PSVR_SET_MODE_REPORT_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 4,
		.payload = __cpu_to_le32(mode ? PSVR_MODE_VR :
						PSVR_MODE_CINEMATIC),
	};
	int ret;

	ret = psvr_control_send(psvr, &report, sizeof(report));
	if (ret < 0) {
		g_print("PSVR: Failed to set %s mode: %d\n",
			mode ? "VR" : "cinematic", ret);
	}
}

static void psvr_get_calibration(OuvrtPSVR *psvr, uint8_t index)
{
	struct psvr_device_info_request report = {
		.id = PSVR_DEVICE_INFO_REQUEST_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 8,
		.request = PSVR_CALIBRATION_REPORT_ID,
		.index = index,
	};
	int ret;

	ret = psvr_control_send(psvr, &report, sizeof(report));
	if (ret < 0) {
		g_print("Failed to request calibration data %d: %d\n", index,
			ret);
	}
}

static void psvr_decode_sensor_message(OuvrtPSVR *self,
				       const unsigned char *buf,
				       G_GNUC_UNUSED size_t len)
{
	const struct psvr_sensor_message *message = (void *)buf;
	uint16_t volume = __le16_to_cpu(message->volume);
	uint16_t button_raw = __be16_to_cpu(message->button_raw);
	uint16_t proximity = __le16_to_cpu(message->proximity);
	struct raw_imu_sample raw;
	struct imu_sample imu;
	int32_t dt;
	int i;

	if (message->button != self->button) {
		int i;
		int num_buttons = 0;
		uint8_t btns[4];
		for (i = 0; i < 4; i++) {
			if ((self->button ^ message->button) & (1 << i)) {
				btns[num_buttons++] = i |
					((message->button & (1 << i)) ? 0x80 : 0);
			}
		}
		telemetry_send_buttons(self->dev.id, btns, num_buttons);

		self->button = message->button;
	}

	if (message->state != self->state) {
		self->state = message->state;

		if (self->state == PSVR_STATE_RUNNING &&
		    !self->calibration) {
			g_print("PSVR: Requesting calibration data\n");
			for (int i = 0; i < 5; i++)
				psvr_get_calibration(self, i);
		}

		if (self->state == PSVR_STATE_RUNNING &&
		    !self->vrmode) {
			g_print("PSVR: Switch to VR mode\n");
			psvr_set_mode(self, PSVR_MODE_VR);
			psvr_enable_vr_tracking(self);

			self->vrmode = true;
		}
		if (self->state != PSVR_STATE_RUNNING &&
		    self->vrmode)
			self->vrmode = false;
	}

	memset(&imu, 0, sizeof(imu));

	for (i = 0; i < 2; i++) {
		const struct psvr_imu_sample *sample = &message->sample[i];

		raw.time = __le32_to_cpu(sample->timestamp);
		raw.acc[0] = (int16_t)__le16_to_cpu(sample->accel[0]);
		raw.acc[1] = (int16_t)__le16_to_cpu(sample->accel[1]);
		raw.acc[2] = (int16_t)__le16_to_cpu(sample->accel[2]);
		raw.gyro[0] = (int16_t)__le16_to_cpu(sample->gyro[0]);
		raw.gyro[1] = (int16_t)__le16_to_cpu(sample->gyro[1]);
		raw.gyro[2] = (int16_t)__le16_to_cpu(sample->gyro[2]);

		telemetry_send_raw_imu_sample(self->dev.id, &raw);

		dt = raw.time - self->last_timestamp;
		if (dt < 0)
			dt += (1 << 24);

		if (dt < 440 || dt > 560) {
			if (self->last_timestamp == 0) {
				self->last_timestamp = raw.time;
				break;
			}
		}

		/*
		 * Transform from IMU coordinate system into common coordinate
		 * system:
		 *
		 *    x                                y
		 *    |          ⎡ 0  1  0 ⎤ ⎡x⎤       |
		 *    +-- y  ->  ⎢ 1  0  0 ⎥ ⎢y⎥  ->   +-- x
		 *   /           ⎣ 0  0 -1 ⎦ ⎣z⎦      /
		 * -z                                z
		 *
		 * Apply accelerometer scale and bias from the calibration data.
		 */
		imu.acceleration.x = raw.acc[1] * self->acc_scale.x -
				     self->acc_bias.x;
		imu.acceleration.y = raw.acc[0] * self->acc_scale.y -
				     self->acc_bias.y;
		imu.acceleration.z = raw.acc[2] * self->acc_scale.z -
				     self->acc_bias.z;
		imu.angular_velocity.x = raw.gyro[1] *  (16.0 / 16384);
		imu.angular_velocity.y = raw.gyro[0] *  (16.0 / 16384);
		imu.angular_velocity.z = raw.gyro[2] * -(16.0 / 16384);
		imu.time = 1e-6 * raw.time;

		telemetry_send_imu_sample(self->dev.id, &imu);

		pose_update(1e-6 * dt, &self->imu.pose, &imu);

		telemetry_send_pose(self->dev.id, &self->imu.pose);

		self->last_timestamp = raw.time;
	}

	self->last_seq = message->sequence;

	(void)volume;
	(void)button_raw;
	(void)proximity;
}

void psvr_dump_reply(unsigned char *buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		g_print("%02x ", buf[i]);
		if (i == 3 || i % 16 == 3)
			g_print("\n");
	}
	if (i % 16 != 4)
		g_print("\n");
}

void psvr_handle_serial_report(OuvrtPSVR *psvr,
			       struct psvr_serial_report *report)
{
	psvr->dev.serial = g_strndup((const gchar *)report->serial,
				     sizeof(report->serial));
	g_print("Serial number: %s\n", psvr->dev.serial);
	g_print("Firmware version: %u.%u\n",
		report->firmware_version_major,
		report->firmware_version_minor);
}

static void
psvr_handle_calibration_report(OuvrtPSVR *psvr,
			       struct psvr_calibration_report *report)
{
	if (!psvr->calibration)
		psvr->calibration = malloc(290);

	unsigned char *calibration = psvr->calibration;
	memcpy(calibration + 58 * report->index, report->payload, 56);

	if (report->index != 4)
		return;

	uint32_t version = __le32_to_cpup((__le32 *)calibration);
	if (version != 2) {
		g_print("Calibration data version != 2: %u\n", version);
		return;
	}

	/*
	 * The accelerometer calibration values look like measurements at rest
	 * in the six major orientations, in units of 1 g.
	 */
	float *top_up = (float *)(calibration + 16 + 4 * 16);
	float *left_up = (float *)(calibration + 16 + 5 * 16);
	float *bottom_up = (float *)(calibration + 16 + 6 * 16);
	float *right_up = (float *)(calibration + 16 + 7 * 16);
	float *back_up = (float *)(calibration + 16 + 8 * 16);
	float *front_up  = (float *)(calibration + 16 + 9 * 16);

	const double acc_scale_factor = STANDARD_GRAVITY * 2.0 * 2.0 / 32767.0;
	psvr->acc_scale.x = acc_scale_factor * (right_up[0] - left_up[0]);
	psvr->acc_scale.y = acc_scale_factor * (top_up[1] - bottom_up[1]);
	psvr->acc_scale.z = acc_scale_factor * -(back_up[2] - front_up[2]);

	const double acc_bias_factor = STANDARD_GRAVITY * 0.5;
	psvr->acc_bias.x = acc_bias_factor * (right_up[0] + left_up[0]);
	psvr->acc_bias.y = acc_bias_factor * (top_up[1] + bottom_up[1]);
	psvr->acc_bias.z = acc_bias_factor * (back_up[2] + front_up[2]);
}

static void psvr_handle_status_report(OuvrtPSVR *psvr,
				      struct psvr_status_report *report)
{
	uint8_t changed = psvr->status_flags ^ report->flags;
	bool mic_mute = report->flags & PSVR_STATUS_FLAG_MIC_MUTE;
	bool headphones = report->flags & PSVR_STATUS_FLAG_HEADPHONES_CONNECTED;
	bool cinematic = report->flags & PSVR_STATUS_FLAG_CINEMATIC_MODE;
	bool worn = report->flags & PSVR_STATUS_FLAG_WORN;
	bool display_on = report->flags & PSVR_STATUS_FLAG_DISPLAY_ON;

	psvr->status_flags = report->flags;

	if (changed & PSVR_STATUS_FLAG_MIC_MUTE)
		g_print("PSVR: Mic %s\n", mic_mute ? "muted" : "enabled");
	if (changed & PSVR_STATUS_FLAG_HEADPHONES_CONNECTED)
		g_print("PSVR: Headphones %sconnected\n", headphones ? "" : "dis");
	if (changed & PSVR_STATUS_FLAG_CINEMATIC_MODE)
		g_print("PSVR: %s mode\n", cinematic ? "Cinematic" : "VR");
	if (changed & PSVR_STATUS_FLAG_WORN)
		g_print("PSVR: Headset %s\n", worn ? "worn" : "removed");
	if (changed & PSVR_STATUS_FLAG_DISPLAY_ON)
		g_print("PSVR: Display powered %s\n", display_on ? "on" : "off");
	if (psvr->volume != report->volume) {
		psvr->volume = report->volume;
		g_print("PSVR: Volume:%d\n", report->volume);
	}
}

void psvr_handle_command_reply(G_GNUC_UNUSED OuvrtPSVR *psvr,
			       struct psvr_command_reply *reply)
{
	if (reply->error) {
		g_print("PSVR: Command %02x error: %02x \"%s\"\n",
			reply->command, reply->error, reply->message);
	}
}

static void psvr_handle_control_reply(OuvrtPSVR *psvr, unsigned char *buf,
				      int len)
{
	if (buf[2] != PSVR_CONTROL_MAGIC) {
		g_print("PSVR: Unexpected magic byte at offset 2:\n");
		psvr_dump_reply(buf, len);
	} else if (buf[3] != len - 4) {
		g_print("PSVR: Unexpected length byte at offset 3:\n");
		psvr_dump_reply(buf, len);
	} else if (buf[0] == PSVR_SERIAL_REPORT_ID &&
		   len == PSVR_SERIAL_REPORT_SIZE) {
		psvr_handle_serial_report(psvr, (void *)buf);
	} else if (buf[0] == PSVR_CALIBRATION_REPORT_ID &&
		   len == PSVR_CALIBRATION_REPORT_SIZE) {
		psvr_handle_calibration_report(psvr, (void *)buf);
	} else if (buf[0] == PSVR_STATUS_REPORT_ID &&
		   len == PSVR_STATUS_REPORT_SIZE) {
		psvr_handle_status_report(psvr, (void *)buf);
	} else if (buf[0] == PSVR_COMMAND_REPLY_ID &&
		   len == PSVR_COMMAND_REPLY_SIZE) {
		psvr_handle_command_reply(psvr, (void *)buf);
	} else {
		g_print("PSVR: Unknown report:\n");
		psvr_dump_reply(buf, len);
	}
}

static void psvr_control_transfer_callback(struct libusb_transfer *transfer)
{
	OuvrtPSVR *psvr = transfer->user_data;
	int ret;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
			g_print("PSVR: Device vanished\n");
			psvr->dev.active = false;
		} else {
			g_print("PSVR: Control transfer error: %d (%s)\n",
				transfer->status,
				libusb_error_name(transfer->status));
		}
		return;
	}

	if (transfer->buffer[2] != 0xaa) {
		g_print("PSVR: Missing magic\n");
	}
	if (transfer->buffer[3] + 4 != transfer->actual_length) {
		g_print("PSVR: Invalid length\n");
	}

	psvr_handle_control_reply(psvr, transfer->buffer,
				  transfer->actual_length);

	/* Resubmit transfer */
	ret = libusb_submit_transfer(transfer);
	if (ret < 0) {
		g_print("PSVR: Failed to resubmit control transfer: %d\n", ret);
	}
}

static void psvr_sensor_transfer_callback(struct libusb_transfer *transfer)
{
	OuvrtPSVR *psvr = transfer->user_data;
	int ret;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
			g_print("PSVR: Device vanished\n");
			psvr->dev.active = false;
		} else {
			g_print("PSVR: Sensor transfer error: %d (%s)\n",
				transfer->status,
				libusb_error_name(transfer->status));
		}
		return;
	}

	psvr_decode_sensor_message(psvr, transfer->buffer,
				   transfer->actual_length);

	/* Resubmit transfer */
	ret = libusb_submit_transfer(transfer);
	if (ret < 0) {
		g_print("PSVR: Failed to resubmit sensor transfer: %d\n", ret);
	}
}

static int psvr_parse_config_descriptor(OuvrtPSVR *psvr)
{
	struct libusb_config_descriptor *config;
	const struct libusb_interface *interface;
	libusb_device *dev;
	int ret;

	dev = libusb_get_device(psvr->devh);
	if (!dev)
		return -ENODEV;

	ret = libusb_get_config_descriptor_by_value(dev,
						    PSVR_CONFIG_DESCRIPTOR,
						    &config);
	if (ret < 0) {
		g_print("PSVR: Failed to get config descriptor: %d\n", ret);
		return ret;
	}

	if (config->bNumInterfaces < 6)
		return -EINVAL;

	interface = &config->interface[PSVR_INTERFACE_CONTROL];
	if (interface->num_altsetting < 1 ||
	    interface->altsetting[0].bNumEndpoints < 2) {
		libusb_free_config_descriptor(config);
		return -EINVAL;
	}
	psvr->control_endpoint =
		interface->altsetting[0].endpoint[1].bEndpointAddress;

	interface = &config->interface[PSVR_INTERFACE_SENSOR];
	if (interface->num_altsetting < 1 ||
	    interface->altsetting[0].bNumEndpoints < 1) {
		libusb_free_config_descriptor(config);
		return -EINVAL;
	}
	psvr->sensor_endpoint =
		interface->altsetting[0].endpoint[0].bEndpointAddress;

	libusb_free_config_descriptor(config);
	return 0;
}

/*
 * Enables the headset.
 */
static int psvr_start(OuvrtDevice *dev)
{
	OuvrtPSVR *psvr = OUVRT_PSVR(dev);
	libusb_device_handle *devh;
	uint8_t bEndpointAddress;
	unsigned char buf[64];
	int ret;
	int i;

	devh = ouvrt_usb_device_get_handle(OUVRT_USB_DEVICE(dev));
	psvr->devh = devh;

	ret = psvr_parse_config_descriptor(psvr);
	if (ret) {
		g_print("PSVR: Failed to parse USB config descriptor: %d\n", ret);
		return ret;
	}

	ret = libusb_set_auto_detach_kernel_driver(devh, 1);
	if (ret) {
		g_print("PSVR: Failed to detach kernel drivers: %d\n", ret);
		return ret;
	}

	ret = libusb_claim_interface(devh, PSVR_INTERFACE_CONTROL);
	if (ret < 0) {
		g_print("PSVR: Failed to claim control interface: %d\n", ret);
		return ret;
	}

	int actual_length;
	struct psvr_device_info_request request = {
		.id = PSVR_DEVICE_INFO_REQUEST_ID,
		.magic = PSVR_CONTROL_MAGIC,
		.payload_length = 8,
		.request = PSVR_SERIAL_REPORT_ID,
	};

	bEndpointAddress = psvr->control_endpoint | LIBUSB_ENDPOINT_OUT;
	ret = libusb_bulk_transfer(devh, bEndpointAddress, (void *)&request,
				   sizeof(request), &actual_length, 0);
	if (ret < 0) {
		g_print("PSVR: Failed to request serial number: %d (%s)\n",
			ret, libusb_strerror(ret));
		return ret;
	}

	do {
		memset(buf, 0, sizeof(buf));
		bEndpointAddress = psvr->control_endpoint | LIBUSB_ENDPOINT_IN;
		ret = libusb_bulk_transfer(devh, bEndpointAddress, buf,
					   sizeof(buf), &actual_length, 0);
		if (ret) {
			g_print("Failed to read: %d\n", errno);
			return ret;
		}

		int len = buf[3] + 4;
		if (len > 64 || len != actual_length) {
			g_print("Invalid %d-byte report 0x%02x\n",
				actual_length, buf[0]);
			continue;
		}

		psvr_handle_control_reply(psvr, buf, len);
	} while (buf[0] != PSVR_SERIAL_REPORT_ID);

	ret = libusb_claim_interface(devh, PSVR_INTERFACE_SENSOR);
	if (ret < 0) {
		g_print("PSVR: Failed to claim sensor interface: %d\n", ret);
		return ret;
	}

	psvr_set_processing_box_power(psvr, true);
	psvr_set_headset_power(psvr, true);
	g_print("PSVR: Sent power on message\n");

	/* Submit one sensor transfer and one control transfer */
	psvr->num_transfers = 2;
	psvr->transfer = calloc(psvr->num_transfers, sizeof(*psvr->transfer));
	if (!psvr->transfer)
		return -ENOMEM;

	for (i = 0; i < psvr->num_transfers; i++) {
		psvr->transfer[i] = libusb_alloc_transfer(0);
		void *buf = calloc(1, 64);
		bEndpointAddress = (i ? psvr->control_endpoint :
					psvr->sensor_endpoint) |
				   LIBUSB_ENDPOINT_IN;
		libusb_fill_bulk_transfer(psvr->transfer[i], devh,
					  bEndpointAddress, buf, 64,
					  (i ? psvr_control_transfer_callback :
					       psvr_sensor_transfer_callback),
					  psvr, 0);

		ret = libusb_submit_transfer(psvr->transfer[i]);
		if (ret < 0) {
			g_print("PSVR: Failed to submit bulk transfer %d\n", i);
			return ret;
		}
	}

	return 0;
}

/*
 * Powers off the headset.
 */
static void psvr_stop(OuvrtDevice *dev)
{
	OuvrtPSVR *psvr = OUVRT_PSVR(dev);

	psvr_set_headset_power(psvr, false);
	g_print("PSVR: Sent power off message\n");

	libusb_release_interface(psvr->devh, PSVR_INTERFACE_SENSOR);
	libusb_release_interface(psvr->devh, PSVR_INTERFACE_CONTROL);
}

/*
 * Frees the device structure and its contents.
 */
static void ouvrt_psvr_finalize(GObject *object)
{
	OuvrtPSVR *psvr = OUVRT_PSVR(object);

	free(psvr->calibration);
	G_OBJECT_CLASS(ouvrt_psvr_parent_class)->finalize(object);
}

static void ouvrt_psvr_class_init(OuvrtPSVRClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = ouvrt_psvr_finalize;
	OUVRT_DEVICE_CLASS(klass)->start = psvr_start;
	OUVRT_DEVICE_CLASS(klass)->stop = psvr_stop;
}

static void ouvrt_psvr_init(OuvrtPSVR *self)
{
	ouvrt_usb_device_set_vid_pid(OUVRT_USB_DEVICE(self), VID_SONY, PID_PSVR);

	self->dev.type = DEVICE_TYPE_HMD;
	self->power = false;
	self->vrmode = false;
	self->state = PSVR_STATE_POWER_OFF;
	self->imu.pose.rotation.w = 1.0;

	/* ±2g range */
	self->acc_scale.x = STANDARD_GRAVITY * 2.0 / 32767.0;
	self->acc_scale.y = STANDARD_GRAVITY * 2.0 / 32767.0;
	self->acc_scale.z = STANDARD_GRAVITY * 2.0 / -32767.0;

	self->status_flags = 0;
	self->volume = 0;
}

/*
 * Allocates and initializes the device structure.
 *
 * Returns the newly allocated PlayStation VR device.
 */
OuvrtDevice *psvr_new(G_GNUC_UNUSED const char *devnode)
{
	return OUVRT_DEVICE(g_object_new(OUVRT_TYPE_PSVR, NULL));
}
