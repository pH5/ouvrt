/*
 * Etron Technology eSP570 webcam controller specific UVC functionality
 * Copyright 2014 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/uvcvideo.h>
#include <linux/usb/video.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#define ESP570_EXTENSION_UNIT_ID	4

#define ESP570_SELECTOR_I2C		2
#define ESP570_SELECTOR_UNKNOWN_3	3
#define ESP570_SELECTOR_EEPROM		5

/*
 * Calls SET_CUR and then GET_CUR on a given selector of the DK2 camera UVC
 * extension unit.
 */
static int uvc_xu_set_get_cur(int fd, int selector, unsigned char *buf,
			      uint8_t len)
{
	int ret;
	struct uvc_xu_control_query xu = {
		.unit = ESP570_EXTENSION_UNIT_ID,
		.selector = selector,
		.size = len,
		.data = buf,
	};

	xu.query = UVC_SET_CUR;
	ret = ioctl(fd, UVCIOC_CTRL_QUERY, &xu);
	if (ret == -1) {
		printf("uvc: SET_CUR error: %d\n", errno);
		return ret;
	}

	xu.query = UVC_GET_CUR;
	ret = ioctl(fd, UVCIOC_CTRL_QUERY, &xu);
	if (ret == -1) {
		printf("uvc: GET_CUR error: %d\n", errno);
		return ret;
	}

	return 0;
}

/*
 * Reads a buffer from the Microchip 24AA128 EEPROM.
 */
int esp570_eeprom_read(int fd, uint16_t addr, uint8_t len, char *buf_out)
{
	unsigned char buf[59];
	int ret;

	if (len > 32)
		return -1;

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x82;
	buf[1] = 0xa0;
	buf[2] = (addr >> 8) & 0xff;
	buf[3] = addr & 0xff;
	buf[4] = len;

	ret = uvc_xu_set_get_cur(fd, ESP570_SELECTOR_EEPROM, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	if (buf[0] != 0x82 || buf[1] != len) {
		printf("read_buf: error: 0x%02x 0x%02x\n", buf[0], buf[1]);
		return -1;
	}

	memcpy(buf_out, buf + 2, len);
	return len;
}

static void print_hex(const char *prefix, unsigned char *buf, size_t len)
{
	unsigned int i;

	printf("%s:", prefix);
	for (i = 0; i < len; i++)
		printf(" %02x", buf[i]);
	putchar('\n');
}

/*
 * Performs a 16-bit read operation on the I2C bus.
 */
int esp570_i2c_read(int fd, uint8_t addr, uint8_t reg, uint16_t *val)
{
	unsigned char buf[6];
	int ret;

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x84;
	buf[1] = addr;
	buf[2] = reg;

	ret = uvc_xu_set_get_cur(fd, ESP570_SELECTOR_I2C, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	if (buf[0] != 0x84 || buf[4] != 0x00 || buf[5] != 0x00) {
		print_hex("eSP570: i2c_read error", buf, 6);
		return -1;
	}

	*val = (buf[1] << 8) | buf[2];
	return 0;
}

/*
 * Performs a 16-bit write operation on the I2C bus.
 */
int esp570_i2c_write(int fd, uint8_t addr, uint8_t reg, uint16_t val)
{
	unsigned char buf[6];
	int ret;

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x04;
	buf[1] = addr;
	buf[2] = reg;
	buf[3] = (val >> 8) & 0xff;
	buf[4] = val & 0xff;

	ret = uvc_xu_set_get_cur(fd, ESP570_SELECTOR_I2C, buf, sizeof(buf));
	if (ret < 0)
		return ret;

	if (buf[0] != 0x04 || buf[1] != addr || buf[2] != reg || buf[5] != 0) {
		print_hex("eSP570: i2c_write error", buf, 6);
		return -1;
	}

	if (buf[3] != ((val >> 8) & 0xff) || buf[4] != (val & 0xff)) {
		printf("eSP570: i2c_write wrote 0x%04x, read back 0x%04x\n",
		       val, (buf[3] << 8) | buf[4]);
	}

	return 0;
}

/*
 * Calls SET_CUR and GET_CUR on the extension unit's selector 3 with values
 * captured from the Oculus VR Windows drivers. I have no idea what these mean.
 */
int esp570_setup_unknown_3(int fd)
{
	unsigned char buf[3];
	int ret;

	buf[0] = 0x80;
	buf[1] = 0x14;
	buf[2] = 0x00;
	ret = uvc_xu_set_get_cur(fd, ESP570_SELECTOR_UNKNOWN_3, buf, 3);
	if (ret < 0)
		return ret;
	if (buf[0] != 0x80 || buf[1] != 0xdc || buf[2] != 0x00)
		print_hex("eSP570: set: 80 14 00, got", buf, 3);

	buf[0] = 0xa0;
	buf[1] = 0xf0;
	buf[2] = 0x00;
	ret = uvc_xu_set_get_cur(fd, ESP570_SELECTOR_UNKNOWN_3, buf, 3);
	if (ret < 0)
		return ret;
	if (buf[0] != 0xa0 || buf[1] != 0x98 || buf[2] != 0x00)
		print_hex("eSP570: set: a0 f0 00, got", buf, 3);

	return 0;
}
