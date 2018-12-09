/*
 * Etron Technology eSP770U webcam controller specific UVC functionality
 * Copyright 2017-2018 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <errno.h>
#include <libusb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "uvc.h"

#define ESP770U_EXTENSION_UNIT		4

#define ESP770U_SELECTOR_I2C		2
#define ESP770U_SELECTOR_REG		3
#define ESP770U_SELECTOR_COUNTER	10
#define ESP770U_SELECTOR_CONTROL	11
#define ESP770U_SELECTOR_DATA		12

/*
 * Calls SET_CUR and then GET_CUR on a given selector of the eSP770U UVC
 * extension unit.
 */
static int esp770u_set_get_cur(libusb_device_handle *devh, int selector,
			       uint8_t *buf, uint8_t len)
{
	int ret;

	ret = uvc_set_cur(devh, 0, ESP770U_EXTENSION_UNIT, selector, buf, len);
	if (ret < 0)
		return ret;

	return uvc_get_cur(devh, 0, ESP770U_EXTENSION_UNIT, selector, buf, len);
}

/*
 * Reads an eSP770u register.
 */
static int esp770u_read_reg(libusb_device_handle *devh, uint16_t reg,
			    uint8_t *val)
{
	uint8_t buf[4] = { 0x82, reg >> 8, reg & 0xff, 0x00 };
	int ret;

	ret = esp770u_set_get_cur(devh, ESP770U_SELECTOR_REG, buf, sizeof buf);
	if (ret < 0)
		return ret;
	if (buf[0] != 0x82 || buf[2] != 0x00) {
		printf("%s(%04x): %02x %02x %02x %02x\n", __func__, reg,
		       buf[0], buf[1], buf[2], buf[3]);
	}
	*val = buf[1];
	return ret;
}

/*
 * Writes to an eSP770u register.
 */
static int esp770u_write_reg(libusb_device_handle *devh, uint16_t reg,
			     uint8_t val)
{
	uint8_t buf[4] = { 0x02, reg >> 8, reg & 0xff, val };
	int ret;

	ret = esp770u_set_get_cur(devh, ESP770U_SELECTOR_REG, buf, sizeof buf);
	if (ret < 0)
		return ret;
	if (buf[0] != 0x02 || buf[1] != (reg >> 8) || buf[2] != (reg & 0xff) ||
	    buf[3] != val) {
		printf("%s(%04x): %02x %02x %02x %02x\n", __func__, reg,
		       buf[0], buf[1], buf[2], buf[3]);
	}
	return ret;
}

/*
 * Query firmware version.
 */
int esp770u_query_firmware_version(libusb_device_handle *devh, uint8_t *val)
{
	uint8_t buf[4] = { 0xa0, 0x03, 0x00, 0x00 };
	int ret;

	ret = esp770u_set_get_cur(devh, ESP770U_SELECTOR_REG, buf, sizeof buf);
	if (ret < 0)
		return ret;
	*val = buf[1];
	if (buf[0] != 0xa0 || buf[2] != 0x00 || buf[3] != 0x00) {
		printf("%s: %02x %02x %02x %02x\n", __func__,
		       buf[0], buf[1], buf[2], buf[3]);
	}
	return ret;
}

/*
 * Read self-incrementing counter.
 */
static int esp770u_get_counter(libusb_device_handle *devh, uint8_t *count)
{
	return uvc_get_cur(devh, 0, ESP770U_EXTENSION_UNIT,
			   ESP770U_SELECTOR_COUNTER, count, 1);
}

/*
 * Write back self-incrementing counter.
 */
static int esp770u_set_counter(libusb_device_handle *devh, uint8_t count)
{
	return uvc_set_cur(devh, 0, ESP770U_EXTENSION_UNIT,
			   ESP770U_SELECTOR_COUNTER, &count, 1);
}

/*
 * Reads a buffer from the flash storage.
 */
int esp770u_flash_read(libusb_device_handle *devh, uint32_t addr,
		       uint8_t *data, uint16_t len)
{
	uint8_t control[16];
	uint8_t count;
	int ret;

	ret = esp770u_get_counter(devh, &count);
	if (ret < 0)
		return ret;

	memset(control, 0, sizeof control);
	control[0] = count;
	control[1] = 0x41;
	control[2] = 0x03;
	control[3] = 0x01;

	control[5] = (addr >> 16) & 0xff;
	control[6] = (addr >> 8) & 0xff;
	control[7] = addr & 0xff;

	control[8] = len >> 8;
	control[9] = len & 0xff;

	ret = uvc_set_cur(devh, 0, ESP770U_EXTENSION_UNIT,
			  ESP770U_SELECTOR_CONTROL, control, sizeof control);
	if (ret < 0)
		return ret;

	memset(data, 0, len);
	ret = uvc_get_cur(devh, 0, ESP770U_EXTENSION_UNIT,
			  ESP770U_SELECTOR_DATA, data, len);
	if (ret < 0)
		return ret;

	return esp770u_set_counter(devh, count);
}

static int esp770u_spi_set_control(libusb_device_handle *devh, uint8_t a,
				   size_t len)
{
	/* a is alternating, 0x81 or 0x41 */
	uint8_t control[16] = { 0x00, a, 0x80, 0x01, [9] = len };

	return uvc_set_cur(devh, 0, ESP770U_EXTENSION_UNIT,
			   ESP770U_SELECTOR_CONTROL, control, sizeof control);
}

static int esp770u_spi_set_data(libusb_device_handle *devh, uint8_t *data,
				size_t len)
{
	return uvc_set_cur(devh, 0, ESP770U_EXTENSION_UNIT,
			   ESP770U_SELECTOR_DATA, data, len);
}

static int esp770u_spi_get_data(libusb_device_handle *devh, uint8_t *data,
				size_t len)
{
	return uvc_get_cur(devh, 0, ESP770U_EXTENSION_UNIT,
			   ESP770U_SELECTOR_DATA, data, len);
}

/*
 * Writes a command buffer to the nRF51288 radio.
 */
static int esp770u_radio_write(libusb_device_handle *devh, const uint8_t *buf,
			       size_t len)
{
	uint8_t data[127];
	unsigned int i;
	int ret;

	if (len > 126)
		return -EINVAL;

	memset(data, 0, sizeof data);
	for (i = 0; i < len; i++) {
		data[i] = buf[i];
		data[126] -= buf[i]; /* calculate checksum */
	}

	ret = esp770u_spi_set_control(devh, 0x81, sizeof data);
	if (ret < 0)
		return ret;

	/* Send data */
	ret = esp770u_spi_set_data(devh, data, sizeof data);
	if (ret < 0)
		return ret;

	ret = esp770u_spi_set_control(devh, 0x41, sizeof data);
	if (ret < 0)
		return ret;

	/* Expect all zeros */
	ret = esp770u_spi_get_data(devh, data, sizeof data);
	if (ret < 0)
		return ret;

	ret = esp770u_spi_set_control(devh, 0x81, sizeof data);
	if (ret < 0)
		return ret;

	/* Clear */
	memset(data, 0, sizeof data);
	ret = esp770u_spi_set_data(devh, data, sizeof data);
	if (ret < 0)
		return ret;

	ret = esp770u_spi_set_control(devh, 0x41, sizeof data);
	if (ret < 0)
		return ret;

	ret = esp770u_spi_get_data(devh, data, sizeof data);
	if (ret < 0)
		return ret;
	for (i = 2; i < 126; i++)
		if (data[i])
			break;
	if (data[0] != buf[0] || data[1] != buf[1]) {
		printf("eSP770U: Unexpected read (%02x %02x):\n", buf[0], buf[1]);
		for (i = 0; i < 127; i++)
			printf("%02x ", data[i]);
		printf("\n");
	}
	uint8_t chksum = 0;
	for (i = 0; i < 127; i++)
		chksum += data[i];
	if (chksum) {
		printf("eSP770U: Checksum mismatch: %02x\n", chksum);
		for (i = 0; i < 127; i++)
			printf("%02x ", data[i]);
		printf("\n");
	}

	return 0;
}

/*
 * Unknown nRF51288 radio initialization.
 */
int esp770u_init_radio(libusb_device_handle *devh)
{
	uint8_t val;
	int ret;

	/* Wait for the nRF51288 to boot up */
	usleep(50000);

	static const uint8_t buf0[2] = { 0x01, 0x01 };
	ret = esp770u_radio_write(devh, buf0, sizeof buf0);
	if (ret < 0)
		return ret;

	static const uint8_t buf1[2] = { 0x11, 0x01 };
	ret = esp770u_radio_write(devh, buf1, sizeof buf1);
	if (ret < 0)
		return ret;

	ret = esp770u_read_reg(devh, 0xf014, &val);
	if (ret < 0)
		return ret;
	if (val != 0x1a)
		printf("unexpected read(0xf014) = 0x%04x\n", val);

	static const uint8_t buf2[2] = { 0x21, 0x01 };
	ret = esp770u_radio_write(devh, buf2, sizeof buf2);
	if (ret < 0)
		return ret;

	static const uint8_t buf3[2] = { 0x31, 0x01 };
	return esp770u_radio_write(devh, buf3, sizeof buf3);
}

/*
 * Setup nRF51288 to receive exposure synchronisation signals from the Rift HMD
 * with the given radio id.
 */
int esp770u_setup_radio(libusb_device_handle *devh, uint8_t radio_id[5])
{
	int ret;

	const uint8_t buf0[7] = {
		0x40, 0x10,
		radio_id[0],
		radio_id[1],
		radio_id[2],
		radio_id[3],
		radio_id[4],
	};
	ret = esp770u_radio_write(devh, buf0, sizeof buf0);
	if (ret < 0)
		return ret;

	static const uint8_t buf1[10] = {
		0x50, 0x11,
		0xf4, 0x01,
		0x00, 0x00, 0x67, 0xff, 0xff, 0xff
	};
	ret = esp770u_radio_write(devh, buf1, sizeof buf1);
	if (ret < 0)
		return ret;

	static const uint8_t buf2[2] = { 0x61, 0x12 };
	ret = esp770u_radio_write(devh, buf2, sizeof buf2);
	if (ret < 0)
		return ret;

	static const uint8_t buf3[2] = { 0x71, 0x85 };
	ret = esp770u_radio_write(devh, buf3, sizeof buf3);
	if (ret < 0)
		return ret;

	static const uint8_t buf4[2] = { 0x81, 0x86 };
	return esp770u_radio_write(devh, buf4, sizeof buf4);
}

/*
 * Performs a 16-bit read operation on the I2C bus.
 */
int esp770u_i2c_read(libusb_device_handle *devh, uint8_t addr, uint16_t reg,
		     uint16_t *val)
{
	uint8_t buf[6] = {
		0x86, addr,
		reg >> 8, reg & 0xff,
	};
	int ret;

	ret = esp770u_set_get_cur(devh, ESP770U_SELECTOR_I2C, buf, sizeof buf);
	if (ret < 0)
		return ret;

	if (buf[0] != 0x86 || buf[4] != 0x00 || buf[5] != 0x00) {
		printf("%s(%04x): %02x %02x %02x %02x %02x %02x\n", __func__,
		       reg, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
		return -1;
	}

	*val = (buf[2] << 8) | buf[1];

	return 0;
}

/*
 * Performs a 16-bit write operation on the I2C bus.
 */
int esp770u_i2c_write(libusb_device_handle *devh, uint8_t addr, uint16_t reg,
		      uint16_t val)
{
	uint8_t buf[6] = {
		0x06, addr,
		reg >> 8, reg & 0xff,
		val >> 8, val & 0xff,
	};
	int ret;

	ret = esp770u_set_get_cur(devh, ESP770U_SELECTOR_I2C, buf, sizeof buf);
	if (ret < 0)
		return ret;

	if (buf[0] != 0x06 || buf[1] != addr || buf[2] != (reg >> 8) ||
	    buf[3] != (reg & 0xff)) {
		printf("%s(%04x, %04x): %02x %02x %02x %02x %02x %02x\n",
		       __func__, reg, val, buf[0], buf[1], buf[2], buf[3],
		       buf[4], buf[5]);
		return -1;
	}

	if (buf[4] != (val >> 8) || buf[5] != (val & 0xff)) {
		printf("%s(%04x, %04x): read back 0x%04x\n", __func__, reg,
		       val, (buf[4] << 8) | buf[5]);
	}

	return 0;
}

/*
 * Calls SET_CUR and GET_CUR on the extension unit's selector 3 with values
 * captured from the Oculus Windows drivers. This could be some kind of reset
 * sequence.
 */
int esp770u_init_unknown(libusb_device_handle *devh)
{
	uint8_t val;
	int ret;

	ret = esp770u_read_reg(devh, 0xf05a, &val);
	if (ret < 0)
		return ret;
	if (val != 0x03)
		printf("unexpected f05a value: %02x\n", val);
	val = 0x01; /* val &= ~(1 << 1)? */
	ret = esp770u_write_reg(devh, 0xf05a, val);
	if (ret < 0)
		return ret;

	ret = esp770u_read_reg(devh, 0xf018, &val);
	if (ret < 0)
		return ret;
	if (val != 0x0e)
		printf("unexpected f018 value: %02x\n", val);
	val |= 1 << 0;
	ret = esp770u_write_reg(devh, 0xf018, val);
	if (ret < 0)
		return ret;

	ret = esp770u_read_reg(devh, 0xf017, &val);
	if (ret < 0)
		return ret;
	if (val != 0xec && val != 0xed)
		printf("unexpected f017 value: %02x\n", val);
	val |= 1 << 0;
	ret = esp770u_write_reg(devh, 0xf017, val);
	if (ret < 0)
		return ret;
	val &= ~(1 << 0);
	ret = esp770u_write_reg(devh, 0xf017, val);
	if (ret < 0)
		return ret;

	val = 0x0e; /* &= ~(1 << 0)? */
	return esp770u_write_reg(devh, 0xf018, val);
}
