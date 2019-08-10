/*
 * UDP Telemetry
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdint.h>
#include <unistd.h>

#define TELEMETRY_DEFAULT_PORT			28532

#define TELEMETRY_PACKET_RAW_BUFFER		0
#define TELEMETRY_PACKET_RAW_IMU_SAMPLE		1
#define TELEMETRY_PACKET_IMU_SAMPLE		2
#define TELEMETRY_PACKET_POSE			3
#define TELEMETRY_PACKET_LIGHTHOUSE_FRAME	4
#define TELEMETRY_PACKET_BUTTONS		5
#define TELEMETRY_PACKET_AXIS			6

struct imu_sample;
struct raw_imu_sample;
struct lighthouse_frame;
struct dpose;

int telemetry_send_raw_buffer(uint8_t dev_id, const char *buf, size_t len);
int telemetry_send_raw_imu_sample(uint8_t dev_id, struct raw_imu_sample *raw);
int telemetry_send_imu_sample(uint8_t dev_id, struct imu_sample *sample);
int telemetry_send_lighthouse_frame(uint8_t dev_id,
				    struct lighthouse_frame *frame);
int telemetry_send_pose(uint8_t dev_id, struct dpose *pose);
int telemetry_send_buttons(uint8_t dev_id, uint8_t *buttons, int num_buttons);
int telemetry_send_axis(uint8_t dev_id, int index, float *axis, int num_axis);
int telemetry_init();
void telemetry_deinit();
