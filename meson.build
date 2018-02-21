# Copyright 2016-2018 Philipp Zabel
# SPDX-License-Identifier:	GPL-2.0+

project('ouvrt', ['c', 'cpp'],
	version : '0.1',
	meson_version : '>= 0.35',
	license : 'GPL')

with_gstreamer = get_option('gstreamer')
with_opencv = get_option('opencv')

add_global_arguments('-Wall', language : 'c')

gio_dep = dependency('gio-unix-2.0')
glib_dep = dependency('glib-2.0')
gobj_dep = dependency('gobject-2.0', version : '>= 2.44')
if with_gstreamer != 'false'
  gst_dep = dependency('gstreamer-1.0', required : with_gstreamer == 'true')
else
  gst_dep = []
endif
json_glib_dep = dependency('json-glib-1.0', version : '>= 1.2')
if with_opencv != 'false'
  opencv_dep = dependency('opencv', required : with_opencv == 'true')
else
  opencv_dep = []
endif
thread_dep = dependency('threads')
udev_dep = dependency('libudev')
usb_dep = dependency('libusb-1.0', version : '>= 1.0.16')
zlib_dep = dependency('zlib')
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)

build_gst = with_gstreamer != 'false' and gst_dep.found()
build_opencv = with_opencv != 'false' and opencv_dep.found()

if build_gst
	add_global_arguments('-DHAVE_GST=1', language : 'c')
	add_global_arguments('-DHAVE_DEBUG_STREAM=1', language : 'c')
endif
if build_opencv
	add_global_arguments('-DHAVE_OPENCV=1', language : 'c')
endif

gdbus_codegen = find_program('gdbus-codegen')

tracker_xml = 'xml/de.phfuenf.ouvrt.Tracker1.xml'
camera_xml = 'xml/de.phfuenf.ouvrt.Camera1.xml'

gdbus_generated = custom_target(
  'gdbus-generated.h',
  input : [
    tracker_xml,
    camera_xml
  ],
  output : [
    'gdbus-generated.c',
    'gdbus-generated.h'
  ],
  command : [
    gdbus_codegen,
    '--interface-prefix', 'de.phfuenf.ouvrt.',
    '--generate-c-code', 'gdbus-generated',
    '--c-namespace', 'Ouvrt',
    '--c-generate-object-manager',
    '@INPUT@'
  ]
)

subdir('src')

inc_src = include_directories('src')

subdir('tools')
