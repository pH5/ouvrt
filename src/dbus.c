/*
 * D-Bus interface implementation
 * Copyright 2015 Philipp Zabel
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "camera.h"
#include "camera-dk2.h"
#include "device.h"
#include "gdbus-generated.h"
#include "ouvrtd.h"
#include "rift.h"

static GDBusObjectManagerServer *manager = NULL;

/*
 * Creates the object manager when the D-Bus connection is available.
 */
static void ouvrt_dbus_on_bus_acquired(GDBusConnection *connection,
				       const gchar *name,
				       G_GNUC_UNUSED gpointer user_data)
{
	g_print("ouvrtd: Acquired session bus, name: '%s'\n", name);

	if (!(g_dbus_connection_get_capabilities(connection) &
	      G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING)) {
		g_warning("ouvrtd: Unix FD passing not supported!\n");
	}

	/* org.freedesktop.DBus.ObjectManager */
	manager = g_dbus_object_manager_server_new("/de/phfuenf/ouvrt");
	g_dbus_object_manager_server_set_connection(manager, connection);
}

static void sender_vanished_handler(G_GNUC_UNUSED GDBusConnection *connection,
				    const gchar *name,
				    G_GNUC_UNUSED gpointer user_data)
{
	g_print("Watched name %s disappeared from the bus\n", name);
}

static gboolean ouvrt_tracker1_on_handle_acquire(OuvrtTracker1 *object,
						 GDBusMethodInvocation *invocation,
						 GUnixFDList *fd_list,
						 gpointer user_data)
{
	OuvrtDevice *dev = OUVRT_DEVICE(user_data);
	GError *error = NULL;
	const gchar *sender;
	int fd;

	if (fd_list != NULL) {
		g_warning("Tracker1.Acquire ignoring received fd list\n");
		g_object_unref(fd_list);
	}

	sender = g_dbus_method_invocation_get_sender(invocation);

	g_print("Tracker1 interface of device %s acquired by %s\n",
		dev->devnode, sender);

	/* Add a watch on sender */
	guint watcher_id = g_bus_watch_name(G_BUS_TYPE_SESSION, sender,
					    G_BUS_NAME_WATCHER_FLAGS_NONE,
					    NULL, /* name_appeared_handler */
					    sender_vanished_handler,
					    NULL, /* user_data */
					    NULL); /* user_data_free_func */

	(void)watcher_id;

	/* FIXME */
	fd = 1; /* stdout */

	fd_list = NULL;
	fd_list = g_unix_fd_list_new();
	g_unix_fd_list_append(fd_list, fd, &error);

	ouvrt_tracker1_complete_acquire(object, invocation, fd_list);

	return TRUE;
}

static gboolean ouvrt_tracker1_on_handle_release(OuvrtTracker1 *object,
						 GDBusMethodInvocation *invocation,
						 gpointer user_data)
{
	OuvrtDevice *dev = OUVRT_DEVICE(user_data);
	const gchar *sender;

	sender = g_dbus_method_invocation_get_sender(invocation);

	g_print("Tracker1 interface of device %s released by %s\n",
		dev->devnode, sender);

	ouvrt_tracker1_complete_release(object, invocation);

	return TRUE;
}

/*
 * Signal change notification for the Tracker1 tracking property.
 */
static void ouvrt_tracker1_on_tracking_changed(GObject *object,
					       GParamSpec *spec,
					       gpointer user_data)
{
	OuvrtTracker1 *tracker = OUVRT_TRACKER1(object);
	OuvrtDevice *dev = user_data;
	gboolean tracking;

	if (dev->type != DEVICE_TYPE_HMD)
		return;

	if (g_strcmp0(g_param_spec_get_name(spec), "tracking") != 0)
		return;

	tracking = ouvrt_tracker1_get_tracking(tracker);

	if (tracking) {
		g_print("Tracking enabled\n");
	} else {
		g_print("Tracking disabled\n");
	}
}

/*
 * Signal change notification for the Tracker1 flicker property.
 */
static void ouvrt_tracker1_on_flicker_changed(GObject *object,
					      GParamSpec *spec,
					      gpointer user_data)
{
	OuvrtTracker1 *tracker = OUVRT_TRACKER1(object);
	OuvrtDevice *dev = user_data;
	gboolean flicker;

	if (!OUVRT_IS_RIFT(dev))
		return;

	if (g_strcmp0(g_param_spec_get_name(spec), "flicker") != 0)
		return;

	flicker = ouvrt_tracker1_get_flicker(tracker);

	ouvrt_rift_set_flicker(OUVRT_RIFT(dev), flicker);
	if (flicker) {
		g_print("Flicker enabled\n");
	} else {
		g_print("Flicker disabled\n");
	}
}

/*
 * Exports a Tracker1 interface via D-Bus.
 */
static void ouvrt_dbus_export_tracker1_interface(OuvrtObjectSkeleton *object,
						 OuvrtDevice *dev)
{
	OuvrtTracker1 *tracker;

	g_print("Exporting Tracker1 interface for device %s\n", dev->devnode);

	tracker = ouvrt_tracker1_skeleton_new();
	ouvrt_tracker1_set_tracking(tracker, FALSE);
	ouvrt_tracker1_set_flicker(tracker, TRUE);

	g_signal_connect(tracker, "handle-acquire",
			 G_CALLBACK(ouvrt_tracker1_on_handle_acquire), dev);
	g_signal_connect(tracker, "handle-release",
			 G_CALLBACK(ouvrt_tracker1_on_handle_release), dev);
	g_signal_connect(tracker, "notify::tracking",
			 G_CALLBACK(ouvrt_tracker1_on_tracking_changed), dev);
	g_signal_connect(tracker, "notify::flicker",
			 G_CALLBACK(ouvrt_tracker1_on_flicker_changed), dev);

	ouvrt_object_skeleton_set_tracker1(object, tracker);
	g_object_unref(tracker);
}

/*
 * Signal change notification for the Camera1 sync-exposure property.
 */
static void ouvrt_camera1_on_sync_exposure_changed(GObject *object,
						   GParamSpec *spec,
						   gpointer user_data)
{
	OuvrtCamera1 *camera = OUVRT_CAMERA1(object);
	OuvrtDevice *dev = user_data;
	gboolean sync;

	if (!OUVRT_IS_CAMERA_DK2(dev))
		return;

	if (g_strcmp0(g_param_spec_get_name(spec), "sync-exposure") != 0)
		return;

	sync = ouvrt_camera1_get_sync_exposure(camera);

	ouvrt_camera_dk2_set_sync_exposure(OUVRT_CAMERA_DK2(dev), sync);
	if (sync) {
		g_print("Synchronised exposure enabled\n");
	} else {
		g_print("Synchronised exposure disabled\n");
	}
}

/*
 * Exports a Camera1 interface via D-Bus.
 */
static void ouvrt_dbus_export_camera1_interface(OuvrtObjectSkeleton *object,
						OuvrtDevice *dev)
{
	OuvrtCamera *camera = OUVRT_CAMERA(dev);
	double *A = camera->camera_matrix.m;
	OuvrtCamera1 *camera1;
	const gchar *caps;
	GVariant *variant;

	g_print("Exporting Camera1 interface for device %s\n", dev->devnode);

	camera1 = ouvrt_camera1_skeleton_new();

	variant = g_variant_new("(ddddddddd)", A[0], A[1], A[2],
					       A[3], A[4], A[5],
					       A[6], A[7], A[8]);
	ouvrt_camera1_set_camera_matrix(camera1, variant);
	variant = g_variant_new("(ddddd)", camera->dist_coeffs[0],
					   camera->dist_coeffs[1],
					   camera->dist_coeffs[2],
					   camera->dist_coeffs[3],
					   camera->dist_coeffs[4]);
	ouvrt_camera1_set_distortion_coefficients(camera1, variant);

	caps = "video/x-raw,format=GRAY8,width=752,height=480,framerate=60/1";
	ouvrt_camera1_set_gst_shm_caps(camera1, caps);
	ouvrt_camera1_set_gst_shm_socket(camera1, "/tmp/ouvrtd-gst");
	ouvrt_camera1_set_sync_exposure(camera1, FALSE);

	g_signal_connect(camera1, "notify::sync-exposure",
			 G_CALLBACK(ouvrt_camera1_on_sync_exposure_changed),
			 dev);

	ouvrt_object_skeleton_set_camera1(object, camera1);
	g_object_unref(camera1);
}

static gboolean
ouvrt_radio1_on_handle_start_discovery(OuvrtRadio1 *object,
				       GDBusMethodInvocation *invocation,
				       gpointer user_data)
{
	OuvrtDevice *dev = OUVRT_DEVICE(user_data);
	const gchar *sender;

	sender = g_dbus_method_invocation_get_sender(invocation);

	g_print("Radio1 interface of device %s set to discovery mode by %s\n",
		dev->devnode, sender);

	ouvrt_device_radio_start_discovery(dev);

	ouvrt_radio1_complete_start_discovery(object, invocation);

	return TRUE;
}

static gboolean
ouvrt_radio1_on_handle_stop_discovery(OuvrtRadio1 *object,
				      GDBusMethodInvocation *invocation,
				      gpointer user_data)
{
	OuvrtDevice *dev = OUVRT_DEVICE(user_data);
	const gchar *sender;

	sender = g_dbus_method_invocation_get_sender(invocation);

	g_print("Radio1 interface of device %s set to normal mode by %s\n",
		dev->devnode, sender);

	ouvrt_device_radio_stop_discovery(dev);

	ouvrt_radio1_complete_stop_discovery(object, invocation);

	return TRUE;
}

/*
 * Exports a Radio1 interface via D-Bus.
 */
static void ouvrt_dbus_export_radio1_interface(OuvrtObjectSkeleton *object,
					       OuvrtDevice *dev)
{
	OuvrtRadio1 *radio = ouvrt_radio1_skeleton_new();

	g_print("Exporting Radio1 interface for device %s\n", dev->devnode);


	ouvrt_radio1_set_discovering(radio, FALSE);
	ouvrt_radio1_set_address(radio, "00:00:00:00:00");

	g_signal_connect(radio, "handle-start-discovery",
			 G_CALLBACK(ouvrt_radio1_on_handle_start_discovery),
			 dev);
	g_signal_connect(radio, "handle-stop-discovery",
			 G_CALLBACK(ouvrt_radio1_on_handle_stop_discovery),
			 dev);

	ouvrt_object_skeleton_set_radio1(object, radio);
	g_object_unref(radio);
}

void ouvrt_dbus_export_device(OuvrtDevice *dev)
{
	gchar *object_path;
	OuvrtObjectSkeleton *object;

	if (!manager)
		return;

	object_path = g_strdup_printf("/de/phfuenf/ouvrt/dev_%lu", dev->id);
	object = ouvrt_object_skeleton_new(object_path);

	g_debug("TODO: register %s with DBus\n", dev->devnode);

	if (dev->type == DEVICE_TYPE_HMD) {
		/* Export a Tracker1 interface */
		ouvrt_dbus_export_tracker1_interface(object, dev);
	}

	if (dev->has_radio) {
		/* Export a Radio1 interface */
		ouvrt_dbus_export_radio1_interface(object, dev);
	}

	if (OUVRT_IS_CAMERA(dev)) {
		/* Export a Camera1 interface */
		ouvrt_dbus_export_camera1_interface(object, dev);
	}

	g_dbus_object_manager_server_export(manager,
					    G_DBUS_OBJECT_SKELETON(object));
	g_object_unref(object);
	g_free(object_path);
}

void ouvrt_dbus_unexport_device(OuvrtDevice *dev)
{
	gchar *object_path =
		g_strdup_printf("/de/phfuenf/ouvrt/dev_%lu", dev->id);

	if (manager) {
		g_print("D-Bus: Unexporting %s\n", object_path);
		g_dbus_object_manager_server_unexport(manager, object_path);
	}

	g_free(object_path);
}

static void __ouvrt_dbus_export_device(gpointer data,
				       gpointer user_data G_GNUC_UNUSED)
{
	ouvrt_dbus_export_device(OUVRT_DEVICE(data));
}

/*
 * Exports Tracker1 and Camera1 interfaces via D-Bus as soon as the
 * Ouvrtd name was acquired on the bus.
 */
static void
ouvrt_dbus_on_name_acquired(GDBusConnection *connection G_GNUC_UNUSED,
			    const gchar *name, gpointer user_data)
{
	g_print("ouvrtd: Acquired name \"%s\"\n", name);

	/* Now we are ready to serve our objects */
	g_list_foreach(device_list, __ouvrt_dbus_export_device, user_data);
}

/*
 * Reports when the Ouvrtd name on the bus was lost.
 */
static void ouvrt_dbus_on_name_lost(G_GNUC_UNUSED GDBusConnection *connection,
				    G_GNUC_UNUSED const gchar *name,
				    G_GNUC_UNUSED gpointer user_data)
{
	g_print("ouvrtd: Lost name %s\n", name);
}

/*
 * Requests ownership of the Ouvrtd name on the bus and installs
 * the callback functions.
 */
guint ouvrt_dbus_own_name(void)
{
	return g_bus_own_name(G_BUS_TYPE_SESSION,
			      "de.phfuenf.ouvrt.Ouvrtd",
			      G_BUS_NAME_OWNER_FLAGS_NONE,
			      ouvrt_dbus_on_bus_acquired,
			      ouvrt_dbus_on_name_acquired,
			      ouvrt_dbus_on_name_lost,
			      NULL,  /* user_data */
			      NULL); /* user_data_free_func */
}
