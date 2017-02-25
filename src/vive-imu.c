/*
 * HTC Vive IMU
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <asm/byteorder.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>

#include "vive-imu.h"
#include "vive-hid-reports.h"
#include "hidraw.h"
#include "imu.h"
#include "telemetry.h"

static inline int oldest_sequence_index(uint8_t a, uint8_t b, uint8_t c)
{
	if (a == (uint8_t)(b + 2))
		return 1;
	else if (b == (uint8_t)(c + 2))
		return 2;
	else
		return 0;
}

int vive_imu_get_range_modes(OuvrtDevice *dev, struct vive_imu *imu)
{
	struct vive_imu_range_modes_report report = {
		.id = VIVE_IMU_RANGE_MODES_REPORT_ID,
	};
	int ret;
	int i;

	ret = hid_get_feature_report(dev->fd, &report, sizeof(report));
	if (ret < 0)
		return ret;

	if (!report.gyro_range || !report.accel_range) {
		ret = hid_get_feature_report(dev->fd, &report, sizeof(report));
		if (ret < 0)
			return ret;

		if (!report.gyro_range || !report.accel_range) {
			g_print("%s: unexpected range mode report: %02x %02x %02x",
				dev->name, report.id, report.gyro_range,
				report.accel_range);
			for (i = 0; i < 61; i++)
				g_print(" %02x", report.unknown[i]);
			g_print("\n");
		}
	}

	if (report.gyro_range > 4 || report.accel_range > 4)
		return -EINVAL;

	/*
	 * Convert MPU-6500 gyro full scale range (+/-250°/s, +/-500°/s,
	 * +/-1000°/s, or +/-2000°/s) into rad/s, accel full scale range
	 * (+/-2g, +/-4g, +/-8g, or +/-16g) into m/s².
	 */
	imu->gyro_range = M_PI / 180.0 * (250 << report.gyro_range);
	imu->accel_range = 9.80665 * (2 << report.accel_range);

	return 0;
}

/*
 * Decodes the periodic IMU sensor message sent by the Vive headset and wired
 * controllers.
 */
void vive_imu_decode_message(OuvrtDevice *dev, struct vive_imu *imu,
			     const void *buf, size_t len)
{
	const struct vive_imu_report *report = buf;
	const struct vive_imu_sample *sample = report->sample;
	uint8_t last_seq = imu->sequence;
	int i, j;

	(void)len;

	/*
	 * The three samples are updated round-robin. New messages
	 * can contain already seen samples in any place, but the
	 * sequence numbers should always be consecutive.
	 * Start at the sample with the oldest sequence number.
	 */
	i = oldest_sequence_index(sample[0].seq, sample[1].seq, sample[2].seq);

	/* From there, handle all new samples */
	for (j = 3; j; --j, i = (i + 1) % 3) {
		struct raw_imu_sample raw;
		struct imu_sample s;
		uint32_t time;
		double scale;
		uint8_t seq;
		int32_t dt;

		sample = report->sample + i;
		seq = sample->seq;

		/* Skip already seen samples */
		if (seq == last_seq ||
		    seq == (uint8_t)(last_seq - 1) ||
		    seq == (uint8_t)(last_seq - 2))
			continue;

		raw.acc[0] = (int16_t)__le16_to_cpu(sample->acc[0]);
		raw.acc[1] = (int16_t)__le16_to_cpu(sample->acc[1]);
		raw.acc[2] = (int16_t)__le16_to_cpu(sample->acc[2]);
		raw.gyro[0] = (int16_t)__le16_to_cpu(sample->gyro[0]);
		raw.gyro[1] = (int16_t)__le16_to_cpu(sample->gyro[1]);
		raw.gyro[2] = (int16_t)__le16_to_cpu(sample->gyro[2]);

		time = __le32_to_cpu(sample->time);
		dt = time - (uint32_t)imu->time;
		raw.time = imu->time + dt;

		telemetry_send_raw_imu_sample(dev->id, &raw);

		scale = imu->accel_range / 32768.0;
		s.acceleration.x = -scale * imu->acc_scale.x * raw.acc[0] -
				   imu->acc_bias.x;
		s.acceleration.z = -scale * imu->acc_scale.y * raw.acc[1] -
				   imu->acc_bias.y;
		s.acceleration.y = -scale * imu->acc_scale.z * raw.acc[2] -
				   imu->acc_bias.z;

		scale = imu->gyro_range / 32768.0;
		s.angular_velocity.x = -scale * imu->gyro_scale.x * raw.gyro[0] -
				       imu->gyro_bias.x;
		s.angular_velocity.z = -scale * imu->gyro_scale.y * raw.gyro[1] -
				       imu->gyro_bias.y;
		s.angular_velocity.y = -scale * imu->gyro_scale.z * raw.gyro[2] -
				       imu->gyro_bias.z;

		s.time = (double)raw.time / 48e6;

		telemetry_send_imu_sample(dev->id, &s);

		if ((dt > 47950 && dt < 48050) ||
		    (dt > 190000 && dt < 194000)) {
			pose_update(dt / 48e6, &imu->state.pose, &s);

			telemetry_send_pose(dev->id, &imu->state.pose);
		}

		imu->sequence = seq;
		imu->time = raw.time;
	}
}
