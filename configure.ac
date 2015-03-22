AC_PREREQ([2.69])

AC_INIT([ouvrtd],[0.0.1],[philipp.zabel@gmail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AM_MAINTAINER_MODE

CFLAGS="${CFLAGS} -W -Wall -O2"

AC_PROG_CC
AC_PROG_CXX
AM_PROG_AR
LT_INIT

AM_INIT_AUTOMAKE([no-exeext dist-xz silent-rules subdir-objects])
AM_SILENT_RULES([yes])

PKG_CHECK_MODULES(UDEV, libudev)
PKG_CHECK_MODULES([GLIB], [glib-2.0 gio-unix-2.0])
PKG_CHECK_MODULES([GST], [gstreamer-1.0])

AC_CONFIG_FILES([
	Makefile
])
AC_OUTPUT
