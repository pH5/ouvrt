/*
 * HTC Vive firmware and hardware version readout
 * Copyright 2016 Philipp Zabel
 * SPDX-License-Identifier:	LGPL-2.0+
 */
#include <stdint.h>

#include "vive-firmware.h"
#include "vive-hid-reports.h"
#include "device.h"
#include "hidraw.h"

/*
 * Retrieves the device firmware version and hardware revision
 */
int vive_get_firmware_version(OuvrtDevice *dev)
{
	struct vive_firmware_version_report report = {
		.id = VIVE_FIRMWARE_VERSION_REPORT_ID,
	};
	uint32_t firmware_version;
	int ret;

	ret = hid_get_feature_report_timeout(dev->fd, &report, sizeof(report),
					     100);
	if (ret < 0) {
		if (errno != EPIPE)
			g_print("%s: Read error 0x05: %d\n", dev->name, errno);
		return ret;
	}

	firmware_version = __le32_to_cpu(report.firmware_version);

	g_print("%s: Firmware version %u %s@%s FPGA %u.%u\n",
		dev->name, firmware_version, report.string1,
		report.string2, report.fpga_version_major,
		report.fpga_version_minor);
	g_print("%s: Hardware revision: %d rev %d.%d.%d\n", dev->name,
		report.hardware_revision, report.hardware_version_major,
		report.hardware_version_minor, report.hardware_version_micro);

	return 0;
}
