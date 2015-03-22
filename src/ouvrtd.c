#include <errno.h>
#include <glib.h>
#include <gst/gst.h>
#include <libudev.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/fcntl.h>

#include "dbus.h"
#include "device.h"
#include "gdbus-generated.h"
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
	OuvrtDevice *(*new)(const char *devnode);
};

static const struct device_match device_matches[NUM_MATCHES] = {
	{ VID_OCULUSVR, PID_RIFT_DK2,    "Rift DK2",    rift_dk2_new    },
	{ VID_OCULUSVR, PID_CAMERA_DK2,  "Camera DK2",  camera_dk2_new  },
};

GList *device_list = NULL;
static int num_devices;

/*
 * Check if an added device matches the table of known hardware, if yes create
 * a new device structure and start the device.
 */
void ouvrtd_device_add(struct udev_device *dev)
{
	const char *devnode, *vid, *pid, *serial;
	struct udev_device *parent;
	OuvrtDevice *d;
	int i;

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
	g_print("udev: Found %s: %s\n", device_matches[i].name, devnode);

	d = device_matches[i].new(devnode);
	if (d == NULL)
		return;
	if (serial && d->serial == NULL)
		d->serial = strdup(serial);
	if (d->serial)
		g_print("%s: Serial %s\n", device_matches[i].name, d->serial);

	device_list = g_list_append(device_list, d);
	ouvrt_device_start(d);
}

/*
 * Compares the device's devnode against a given device node.
 */
static gint ouvrt_device_cmp_devnode(OuvrtDevice *dev, const char *devnode)
{
	return g_strcmp0(dev->devnode, devnode);
}

/*
 * Check if a removed device matches a registered device structure. If yes,
 * dereference the device to stop it and free the device structure.
 */
int ouvrtd_device_remove(struct udev_device *dev)
{
	const char *devnode;
	GList *link;

	devnode = udev_device_get_devnode(dev);
	link = g_list_find_custom(device_list, devnode,
				  (GCompareFunc)ouvrt_device_cmp_devnode);
	if (link == NULL)
		return 0;

	g_print("Removing device: %s\n", devnode);
	device_list = g_list_remove_link(device_list, link);
	g_object_unref(OUVRT_DEVICE(link->data));
	g_list_free_1(link);
	num_devices--;

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

struct udev_source {
	GSource base;
	gpointer tag;
};

gboolean udev_source_prepare(GSource *source, gint *timeout)
{
	(void)source;

	*timeout = -1;
	return FALSE;
}

gboolean udev_source_check(GSource *source)
{
	struct udev_source *usrc = (struct udev_source *)source;

	return (g_source_query_unix_fd(source, usrc->tag) > 0);
}

gboolean udev_source_dispatch(GSource *source, GSourceFunc callback,
			      gpointer user_data)
{
	struct udev_source *usrc = (struct udev_source *)source;
	GIOCondition revents = g_source_query_unix_fd(source, usrc->tag);

	if (revents & G_IO_IN) {
		if (callback == NULL)
			return FALSE;
		return callback(user_data);
	}

	return TRUE;
}

GSourceFuncs udev_source_funcs = {
	.prepare = udev_source_prepare,
	.check = udev_source_check,
	.dispatch = udev_source_dispatch,
};

gboolean udev_source_callback(gpointer user_data)
{
	struct udev_monitor *monitor = user_data;
	struct udev_device *dev;
	const char *action;

	dev = udev_monitor_receive_device(monitor);
	if (!dev) {
		g_print("udev: Monitor receive_device error\n");
		return TRUE;
	}

	action = udev_device_get_action(dev);
	if (strcmp(action, "add") == 0)
		ouvrtd_device_add(dev);
	else if (strcmp(action, "remove") == 0)
		ouvrtd_device_remove(dev);

	return TRUE;
}

/*
 * Set up a udev event monitor, call device enumeration, and then monitor
 * for appearing and disappearing known hardware.
 */
void ouvrtd_startup(struct udev *udev)
{
	struct udev_monitor *monitor;
	struct udev_source *source;
	int fd;

	/* Set up monitoring udev events for hidraw and video4linux devices */
	monitor = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(monitor, "hidraw",
							NULL);
	udev_monitor_filter_add_match_subsystem_devtype(monitor, "video4linux",
							NULL);
	udev_monitor_enable_receiving(monitor);
	fd = udev_monitor_get_fd(monitor);

	/* Enumerate presently available hidraw and video4linux devices */
	ouvrtd_enumerate(udev);

	/* Watch udev events for hidraw and video4linux devices */
	source = (struct udev_source *)g_source_new(&udev_source_funcs,
						    sizeof(*source));
	g_source_set_callback(&source->base, udev_source_callback, monitor,
			      NULL); /* destroy_notify */
	source->tag = g_source_add_unix_fd(&source->base, fd, G_IO_IN);
	g_source_attach(&source->base, g_main_context_default());
	g_source_unref(&source->base);
}

static void ouvrtd_signal_handler(int sig)
{
	signal(sig, SIG_IGN);
	g_print(" - stopping all devices\n");

	g_list_foreach(device_list, (GFunc)ouvrt_device_stop,
		       NULL); /* user_data */

	exit(0);
}

/*
 * Main function. Initialize GStreamer for debugging purposes and udev for
 * device detection.
 */
int main(int argc, char *argv[])
{
	struct udev *udev;
	GMainLoop *loop;
	guint owner_id;

	setlocale(LC_CTYPE, "");

	gst_init(&argc, &argv);

	signal(SIGINT, ouvrtd_signal_handler);

	udev = udev_new();
	if (!udev)
		return -1;

	loop = g_main_loop_new(NULL, TRUE);
	owner_id = ouvrt_dbus_own_name();

	ouvrtd_startup(udev);
	g_main_loop_run(loop);

	g_bus_unown_name(owner_id);
	udev_unref(udev);
	g_object_unref(loop);

	return 0;
}
