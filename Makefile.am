bin_PROGRAMS = \
	ouvrtd

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	autogen.sh \
	m4/ax_pthread.m4

MAINTAINERCLEANFILES = \
	configure \
	aclocal.m4 \
	Makefile.in

ouvrtd_SOURCES = \
	src/camera-dk2.h \
	src/camera-dk2.c \
	src/camera-v4l2.h \
	src/camera-v4l2.c \
	src/debug-gst.h \
	src/debug-gst.c \
	src/device.h \
	src/device.c \
	src/rift-dk2.h \
	src/rift-dk2.c \
	src/ouvrtd.c

ouvrtd_CFLAGS = \
	$(UDEV_CFLAGS) \
	$(GST_CFLAGS)

ouvrtd_LDADD = \
	$(UDEV_LIBS) \
	$(PTHREAD_LIBS) \
	$(GST_LIBS)
