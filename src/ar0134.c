/*
 * Aptina AR0134 Image Sensor initialization
 * Copyright 2017-2018 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <errno.h>
#include <libusb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp770u.h"

#define AR0134_CHIP_VERSION_REG			0x3000
#define AR0134_Y_ADDR_START			0x3002
#define AR0134_X_ADDR_START			0x3004
#define AR0134_Y_ADDR_END			0x3006
#define AR0134_X_ADDR_END			0x3008
#define AR0134_FRAME_LENGTH_LINES		0x300a
#define AR0134_LINE_LENGTH_PCK			0x300c
#define AR0134_REVISION_NUMBER			0x300e
#define AR0134_COARSE_INTEGRATION_TIME		0x3012
#define AR0134_FINE_INTEGRATION_TIME		0x3014
#define AR0134_RESET_REGISTER			0x301a
#define		AR0134_FORCED_PLL_ON			(1 << 11)
#define		AR0134_GPI_EN				(1 << 8)
#define		AR0134_STREAM				(1 << 2)
#define AR0134_GLOBAL_GAIN			0x305e
#define AR0134_EMBEDDED_DATA_CTRL		0x3064
#define		AR0134_EMBEDDED_DATA			(1 << 8)
#define		AR0134_EMBEDDED_STATS_EN		(1 << 7)
#define AR0134_DIGITAL_TEST			0x30b0
#define		AR0134_PLL_COMPLETE_BYPASS		(1 << 14)
#define		AR0134_ENABLE_SHORT_LLPCK_BIT		(1 << 10)
#define		AR0134_MONO_CHROME			(1 << 7)
#define AR0134_AE_CTRL_REG			0x3100
#define		AR0134_AE_ENABLE			(1 << 0)

#define AR0134_I2C_ADDR		0x20

static inline int ar0134_read_reg(libusb_device_handle *devh, uint16_t reg,
				  uint16_t *val)
{
        return esp770u_i2c_read(devh, AR0134_I2C_ADDR, reg, val);
}

static inline int ar0134_write_reg(libusb_device_handle *devh, uint16_t reg,
				   uint16_t val)
{
        return esp770u_i2c_write(devh, AR0134_I2C_ADDR, reg, val);
}

int ar0134_init(libusb_device_handle *devh)
{
	uint16_t version, revision;
	uint16_t val;
	int ret;

	ret = ar0134_read_reg(devh, AR0134_CHIP_VERSION_REG, &version);
	if (ret < 0)
		return ret;

	ret = ar0134_read_reg(devh, AR0134_REVISION_NUMBER, &revision);
	if (ret < 0)
		return ret;

	if (version != 0x2406 || revision != 0x1300) {
		printf("AR0134: Unknown sensor %04x:%04x\n", version, revision);
		return -ENODEV;
	}

	ret = ar0134_read_reg(devh, AR0134_DIGITAL_TEST, &val);
	if (ret < 0)
		return ret;
	if (val != AR0134_MONO_CHROME) {
		printf("AR0134: Unexpected mode: 0x%04x\n", val);
		return -EINVAL;
	}

	/* Enable embedded register data and statistics. */
	ret = ar0134_read_reg(devh, AR0134_EMBEDDED_DATA_CTRL, &val);
	if (ret < 0)
		return ret;
	return ar0134_write_reg(devh, AR0134_EMBEDDED_DATA_CTRL, val |
				AR0134_EMBEDDED_DATA |
				AR0134_EMBEDDED_STATS_EN);
}

int ar0134_set_ae(libusb_device_handle *devh, bool enabled)
{
	uint16_t val;
	int ret;

	ret = ar0134_read_reg(devh, AR0134_AE_CTRL_REG, &val);
	if (ret < 0)
		return ret;
	return ar0134_write_reg(devh, AR0134_AE_CTRL_REG, val |
				AR0134_AE_ENABLE);
}

int ar0134_set_gain(libusb_device_handle *devh, uint16_t gain)
{
	return ar0134_write_reg(devh, AR0134_GLOBAL_GAIN, gain);
}

static int ar0134_set_window(libusb_device_handle *devh, uint16_t x_start,
			     uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
	const uint16_t regs[] = {
		AR0134_Y_ADDR_START, y_start,
		AR0134_X_ADDR_START, x_start,
		AR0134_Y_ADDR_END, y_end,
		AR0134_X_ADDR_END, x_end,
	};
	int ret;
	int i;

	for (i = 0; i < 4; i++) {
		ret = ar0134_write_reg(devh, regs[2 * i], regs[2 * i + 1]);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int ar0134_set_timings(libusb_device_handle *devh, bool tight)
{
	uint16_t val;
	int ret;

	ret = ar0134_read_reg(devh, AR0134_LINE_LENGTH_PCK, &val);
	if (ret < 0)
		return ret;

	ret = ar0134_set_window(devh, 0, 0, 1279, 959);
	if (ret < 0)
		return ret;

	/* Set minimum supported pixel clocks per line */
	ret = ar0134_write_reg(devh, AR0134_LINE_LENGTH_PCK,
			       tight ? 1388 : 1498);
	if (ret < 0)
		return ret;
	ret = ar0134_read_reg(devh, AR0134_DIGITAL_TEST, &val);
	if (ret < 0)
		return ret;
	if (val != AR0134_MONO_CHROME)
		printf("AR0134: Unexpected digital test value: 0x%04x\n", val);
	if (tight)
		val |= AR0134_ENABLE_SHORT_LLPCK_BIT;
	else
		val &= AR0134_ENABLE_SHORT_LLPCK_BIT;
	ret = ar0134_write_reg(devh, AR0134_DIGITAL_TEST, val);
	if (ret < 0)
		return ret;

	/* Set minimum total number of lines, 23 lines vertical blanking */
	ret = ar0134_write_reg(devh, AR0134_FRAME_LENGTH_LINES, 997);
	if (ret < 0)
		return ret;

	/*
	 * Set coarse integration time (in multiples of line_length_pck) and
	 * fine integration time (in multiples of the pixel clock).
	 * At 74.25 MHz pixel clock and 1388 pclk per line, exposure time would
	 * be (1388 * 26 + 646) / 74.25e6 = ~495 µs.
	 */
	ret = ar0134_write_reg(devh, AR0134_COARSE_INTEGRATION_TIME,
			       tight ? 26 : 100);
	if (ret < 0)
		return ret;
	return ar0134_write_reg(devh, AR0134_FINE_INTEGRATION_TIME,
				tight ? 646 : 0);
}

/*
 * Switches between streaming mode and externally triggered exposure from
 * nRF51288.
 */
int ar0134_set_sync(libusb_device_handle *devh, bool enabled)
{
	uint16_t val;
	int ret;

	printf("%sabling synchronisation\n", enabled ? "En" : "Dis");

	ret = ar0134_set_timings(devh, true);
	if (ret < 0)
		return ret;

	ret = ar0134_read_reg(devh, AR0134_RESET_REGISTER, &val);
	if (ret < 0)
		return ret;
	val &= ~(AR0134_FORCED_PLL_ON | AR0134_GPI_EN | AR0134_STREAM);
	val |= enabled ? (AR0134_FORCED_PLL_ON | AR0134_GPI_EN) : AR0134_STREAM;
	return ar0134_write_reg(devh, AR0134_RESET_REGISTER, val);
}
