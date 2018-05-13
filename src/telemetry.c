/*
 * UDP Telemetry
 * Copyright 2017 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "imu.h"
#include "lighthouse.h"
#include "telemetry.h"

#define TELEMETRY_ADDRESS			INADDR_LOOPBACK

static struct sockaddr_in telemetry_addr;
static int telemetry_fd;

int telemetry_send_raw_buffer(uint8_t dev_id, const char *buf, size_t len)
{
	char packet[256];

	if (telemetry_fd <= 0)
		return 0;

	if (len > sizeof(buf) - 2)
		return -ENOSPC;

	packet[0] = TELEMETRY_PACKET_RAW_BUFFER;
	packet[1] = dev_id;
	memcpy(packet + 2, buf, len);

	return sendto(telemetry_fd, packet, len, 0,
		      (struct sockaddr *)&telemetry_addr,
		      sizeof(telemetry_addr));
}

int telemetry_send_raw_imu_sample(uint8_t dev_id, struct raw_imu_sample *raw)
{
	char packet[2 + sizeof(*raw)];
	const size_t len = sizeof(packet);

	if (telemetry_fd <= 0)
		return 0;

	packet[0] = TELEMETRY_PACKET_RAW_IMU_SAMPLE;
	packet[1] = dev_id;
	memcpy(packet + 2, raw, sizeof(*raw));

	return sendto(telemetry_fd, packet, len, 0,
		      (struct sockaddr *)&telemetry_addr,
		      sizeof(telemetry_addr));
}

int telemetry_send_imu_sample(uint8_t dev_id, struct imu_sample *sample)
{
	char packet[2 + sizeof(*sample)];
	const size_t len = sizeof(packet);

	if (telemetry_fd <= 0)
		return 0;

	packet[0] = TELEMETRY_PACKET_IMU_SAMPLE;
	packet[1] = dev_id;
	memcpy(packet + 2, sample, sizeof(*sample));

	return sendto(telemetry_fd, packet, len, 0,
		      (struct sockaddr *)&telemetry_addr,
		      sizeof(telemetry_addr));
}

int telemetry_send_lighthouse_frame(uint8_t dev_id,
				    struct lighthouse_frame *frame)
{
	char packet[2 + sizeof(*frame)];
	const size_t len = sizeof(packet);

	if (telemetry_fd <= 0)
		return 0;

	packet[0] = TELEMETRY_PACKET_LIGHTHOUSE_FRAME;
	packet[1] = dev_id;
	memcpy(packet + 2, frame, sizeof(*frame));

	return sendto(telemetry_fd, packet, len, 0,
		      (struct sockaddr *)&telemetry_addr,
		      sizeof(telemetry_addr));
}

int telemetry_send_pose(uint8_t dev_id, struct dpose *pose)
{
	char packet[2 + sizeof(*pose)];
	const size_t len = sizeof(packet);

	if (telemetry_fd <= 0)
		return 0;

	packet[0] = TELEMETRY_PACKET_POSE;
	packet[1] = dev_id;
	memcpy(packet + 2, pose, sizeof(*pose));

	return sendto(telemetry_fd, packet, len, 0,
		      (struct sockaddr *)&telemetry_addr,
		      sizeof(telemetry_addr));
}

int telemetry_send_axis(uint8_t dev_id, int index, float *axis, int num_axis)
{
	char packet[2 + 1 + num_axis * sizeof(float)];
	const size_t len = sizeof(packet);

	if (telemetry_fd <= 0)
		return 0;

	if (num_axis == 0)
		return 0;

	packet[0] = TELEMETRY_PACKET_AXIS;
	packet[1] = dev_id;
	packet[2] = index;
	memcpy(packet + 3, axis, num_axis * sizeof(float));

	return sendto(telemetry_fd, packet, len, 0,
		      (struct sockaddr *)&telemetry_addr,
		      sizeof(telemetry_addr));
}


int telemetry_send_buttons(uint8_t dev_id, uint8_t *buttons, int num_buttons)
{
	char packet[2 + num_buttons];
	const size_t len = sizeof(packet);

	if (telemetry_fd <= 0)
		return 0;

	if (num_buttons == 0)
		return 0;

	packet[0] = TELEMETRY_PACKET_BUTTONS;
	packet[1] = dev_id;
	memcpy(packet + 2, buttons, num_buttons);

	return sendto(telemetry_fd, packet, len, 0,
		      (struct sockaddr *)&telemetry_addr,
		      sizeof(telemetry_addr));
}

/*
 * Initializes the telemetry UDP socket and target address.
 */
int telemetry_init(int *argc, char **argv[])
{
	struct sockaddr_in local_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(0),
		.sin_addr.s_addr = htonl(INADDR_ANY),
	};
	int fd, ret;

	(void)argc;
	(void)argv;

	if (telemetry_fd > 0)
		return -EBUSY;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return fd;

	ret = bind(fd, (struct sockaddr *)&local_addr, sizeof(local_addr));
	if (ret < 0) {
		close(fd);
		return ret;
	}

	telemetry_addr.sin_family = AF_INET;
	telemetry_addr.sin_port = htons(TELEMETRY_DEFAULT_PORT);
	telemetry_addr.sin_addr.s_addr = htonl(TELEMETRY_ADDRESS);

	telemetry_fd = fd;

	return 0;
}

/*
 * Closes the telemetry UDP socket.
 */
void telemetry_deinit()
{
	if (telemetry_fd > 0) {
		close(telemetry_fd);
		telemetry_fd = 0;
	}
}
