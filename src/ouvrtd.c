#include <errno.h>
#include <gst/gst.h>
#include <libudev.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/fcntl.h>

#include "device.h"
#include "rift-dk2.h"
#include "camera-dk2.h"

#define VID_OCULUSVR	"2833"
#define PID_RIFT_DK2	"0021"
#define PID_CAMERA_DK2	"0201"

#define NUM_MATCHES	2

struct device_match {
	const char *vid;
	const char *pid;
	const char *name;
	struct device *(*new)(const char *devnode);
};

static const struct device_match device_matches[NUM_MATCHES] = {
	{ VID_OCULUSVR, PID_RIFT_DK2,    "Rift DK2",    rift_dk2_new    },
	{ VID_OCULUSVR, PID_CAMERA_DK2,  "Camera DK2",  camera_dk2_new  },
};

#define MAX_DEVICES 2

static struct device *devices[MAX_DEVICES];
static int num_devices;

/*
 * Check if an added device matches the table of known hardware, if yes create
 * a new device structure and start the device.
 */
void ouvrtd_device_add(struct udev_device *dev)
{
	const char *devnode, *vid, *pid, *serial;
	struct udev_device *parent;
	struct device *d;
	int i;

	if (num_devices >= MAX_DEVICES)
		return;

	parent = udev_device_get_parent_with_subsystem_devtype(dev,
						"usb", "usb_device");
	if (!parent)
		return;

	vid = udev_device_get_sysattr_value(parent, "idVendor");
	if (!vid)
		return;

	pid = udev_device_get_sysattr_value(parent, "idProduct");
	if (!pid)
		return;

	for (i = 0; i < NUM_MATCHES; i++) {
		if (strcmp(vid, device_matches[i].vid) == 0 &&
		    strcmp(pid, device_matches[i].pid) == 0)
			break;
	}
	if (i == NUM_MATCHES)
		return;

	devnode = udev_device_get_devnode(dev);
	serial = udev_device_get_sysattr_value(parent, "serial");
	printf("udev: Found %s: %s\n",
	       device_matches[i].name, devnode);

	d = device_matches[i].new(devnode);
	if (d == NULL)
		return;
	if (serial && d->serial == NULL)
		d->serial = strdup(serial);
	if (d->serial) {
		printf("%s: Serial %s\n",
		       device_matches[i].name, d->serial);
	}
	devices[num_devices++] = d;

	device_start(d);
}

/*
 * Check if a removed device matches a registered device structure. If yes,
 * dereference the device to stop it and free the device structure.
 */
int ouvrtd_device_remove(struct udev_device *dev)
{
	const char *devnode;
	int i;

	devnode = udev_device_get_devnode(dev);
	for (i = 0; i < num_devices; i++) {
		if (device_match_devnode(devices[i], devnode)) {
			printf("Removing device: %s\n", devnode);
			devices[i] = device_unref(devices[i]);
			memcpy(devices + i, devices + i + 1,
			       (MAX_DEVICES - i - 1) * sizeof(devices[0]));
			num_devices--;
			return 0;
		}
	}

	return 0;
}

/*
 * Enumerate currently present USB devices to find known hardware.
 */
int ouvrtd_enumerate(struct udev *udev)
{
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;

	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "hidraw");
	udev_enumerate_add_match_subsystem(enumerate, "video4linux");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;
		struct udev_device *dev;

		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);

		ouvrtd_device_add(dev);

		udev_device_unref(dev);
	}

	udev_enumerate_unref(enumerate);

	return 0;
}

/*
 * Set up a udev event monitor, call device enumeration, and then monitor
 * for appearing and disappearing known hardware.
 */
int ouvrtd_startup(struct udev *udev)
{
	struct udev_monitor *monitor;
	struct udev_device *dev;
	const char *action;
	struct pollfd pfd;
	int ret;

	/* Set up monitoring udev events for hidraw and video4linux devices */
	monitor = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(monitor, "hidraw",
							NULL);
	udev_monitor_filter_add_match_subsystem_devtype(monitor, "video4linux",
							NULL);
	udev_monitor_enable_receiving(monitor);

	/* Enumerate presently available hidraw and video4linux devices */
	ouvrtd_enumerate(udev);

	pfd.fd = udev_monitor_get_fd(monitor);
	pfd.events = POLLIN;

	/* Watch udev events for hidraw and video4linux devices */
	while (1) {
		ret = poll(&pfd, 1, -1);
		if (ret <= 0 || !(pfd.revents & POLLIN))
			continue;

		dev = udev_monitor_receive_device(monitor);
		if (!dev) {
			printf("udev: Monitor receive_device error\n");
			continue;
		}

		action = udev_device_get_action(dev);
		if (strcmp(action, "add") == 0)
			ouvrtd_device_add(dev);
		else if (strcmp(action, "remove") == 0)
			ouvrtd_device_remove(dev);
	}
}

static void ouvrtd_signal_handler(int sig)
{
	int i;

	signal(sig, SIG_IGN);
	printf(" - stopping all devices\n");

	for (i = 0; i < num_devices; i++)
		device_stop(devices[i]);

	exit(0);
}

/*
 * Main function. Initialize GStreamer for debugging purposes and udev for
 * device detection.
 */
int main(int argc, char *argv[])
{
	struct udev *udev;

	signal(SIGINT, ouvrtd_signal_handler);

	gst_init(&argc, &argv);

	udev = udev_new();
	if (!udev)
		return -1;

	ouvrtd_startup(udev);

	udev_unref(udev);

	return 0;
}
