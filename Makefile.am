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
	src/leds.c \
	src/leds.h \
	src/math.h \
	src/math.c \
	src/rift-dk2.h \
	src/rift-dk2.c \
	src/tracker.c \
	src/tracker.h \
	src/ouvrtd.c \
	src/opencv.h \
	src/opencv.cpp \
	src/vive-headset-imu.h \
	src/vive-headset-imu.c \
	src/vive-headset-lighthouse.h \
	src/vive-headset-lighthouse.c

nodist_ouvrtd_SOURCES = \
	src/gdbus-generated.h \
	src/gdbus-generated.c

ouvrtd_CFLAGS = \
	$(UDEV_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GST_CFLAGS)

ouvrtd_LDADD = \
	libouvrt.a \
	$(UDEV_LIBS) \
	$(GLIB_LIBS) \
	$(GST_LIBS) \
	$(OPENCV_LIBS)

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
