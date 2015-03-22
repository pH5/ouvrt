bin_PROGRAMS = \
	ouvrtd

noinst_PROGRAMS = \
	dump-eeprom

noinst_LIBRARIES = \
	libouvrt.a

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	autogen.sh

MAINTAINERCLEANFILES = \
	configure \
	aclocal.m4 \
	Makefile.in

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
	src/debug.h \
	src/debug.c \
	src/debug-gst.h \
	src/debug-gst.c \
	src/device.h \
	src/device.c \
	src/imu.h \
	src/leds.c \
	src/leds.h \
	src/rift-dk2.h \
	src/rift-dk2.c \
	src/tracker.c \
	src/tracker.h \
	src/ouvrtd.c

ouvrtd_CFLAGS = \
	$(UDEV_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GST_CFLAGS)

ouvrtd_LDADD = \
	libouvrt.a \
	$(UDEV_LIBS) \
	$(GLIB_LIBS) \
	$(GST_LIBS)

dump_eeprom_SOURCES = \
	tools/dump-eeprom.c

dump_eeprom_CFLAGS = \
	-I $(top_srcdir)/src

dump_eeprom_LDADD = \
	libouvrt.a
