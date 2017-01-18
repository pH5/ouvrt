bin_PROGRAMS = \
	ouvrtd

noinst_PROGRAMS = \
	dump-eeprom

noinst_LIBRARIES = \
	libouvrt.a

ACLOCAL_AMFLAGS = -I m4

ARFLAGS = crD

CLEANFILES = \
	src/gdbus-generated.h \
	src/gdbus-generated.c

EXTRA_DIST = \
	autogen.sh \
	xml/de.phfuenf.ouvrt.Camera1.xml \
	xml/de.phfuenf.ouvrt.Tracker1.xml

MAINTAINERCLEANFILES = \
	configure \
	aclocal.m4 \
	Makefile.in

BUILT_SOURCES = \
	src/gdbus-generated.h

libouvrt_a_SOURCES = \
	src/blobwatch.h \
	src/blobwatch.c \
	src/esp570.h \
	src/esp570.c \
	src/flicker.h \
	src/flicker.c \
	src/mt9v034.h \
	src/mt9v034.c

ouvrtd_SOURCES = \
	src/camera.h \
	src/camera.c \
	src/camera-dk2.h \
	src/camera-dk2.c \
	src/camera-v4l2.h \
	src/camera-v4l2.c \
	src/dbus.c \
	src/dbus.h \
	src/debug.h \
	src/debug.c \
	src/debug-gst.h \
	src/debug-gst.c \
	src/device.h \
	src/device.c \
	src/imu.h \
	src/json.c \
	src/json.h \
	src/leds.c \
	src/leds.h \
	src/lighthouse.c \
	src/lighthouse.h \
	src/math.h \
	src/math.c \
	src/psvr.h \
	src/psvr-hid-reports.h \
	src/psvr.c \
	src/rift.h \
	src/rift-hid-reports.h \
	src/rift.c \
	src/rift-radio.h \
	src/rift-radio.c \
	src/tracker.c \
	src/tracker.h \
	src/tracking-model.c \
	src/tracking-model.h \
	src/ouvrtd.c \
	src/opencv.h \
	src/vive-controller.h \
	src/vive-controller.c \
	src/vive-controller-usb.h \
	src/vive-controller-usb.c \
	src/vive-firmware.h \
	src/vive-firmware.c \
	src/vive-imu.h \
	src/vive-imu.c \
	src/vive-headset.h \
	src/vive-headset.c \
	src/vive-headset-mainboard.h \
	src/vive-headset-mainboard.c \
	src/vive-config.h \
	src/vive-config.c

if HAVE_OPENCV
ouvrtd_SOURCES += \
	src/opencv.cpp
endif

nodist_ouvrtd_SOURCES = \
	src/gdbus-generated.h \
	src/gdbus-generated.c

ouvrtd_CFLAGS = \
	$(UDEV_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GST_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(ZLIB_CFLAGS)

ouvrtd_LDADD = \
	libouvrt.a \
	$(UDEV_LIBS) \
	$(GLIB_LIBS) \
	$(GST_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(OPENCV_LIBS) \
	$(ZLIB_LIBS)

dump_eeprom_SOURCES = \
	tools/dump-eeprom.c

dump_eeprom_CFLAGS = \
	-I $(top_srcdir)/src

dump_eeprom_LDADD = \
	libouvrt.a

src/gdbus-generated.c: src/gdbus-generated.h
src/gdbus-generated.h: xml/de.phfuenf.ouvrt.Tracker1.xml \
		       xml/de.phfuenf.ouvrt.Camera1.xml
	@gdbus-codegen --interface-prefix de.phfuenf.ouvrt. \
		--generate-c-code src/gdbus-generated \
		--c-namespace Ouvrt \
		--c-generate-object-manager \
		$^
	@sed -i '/^ouvrt_tracker1_complete_/{N;s/object,/object G_GNUC_UNUSED,/}' \
		src/gdbus-generated.c
