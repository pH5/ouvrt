# Copyright 2016-2018 Philipp Zabel
# SPDX-License-Identifier: GPL-2.0-or-later

project('ouvrt', ['c', 'cpp'],
  version : '0.1',
  meson_version : '>= 0.46',
  license : 'GPL',
  default_options : [
    'buildtype=debugoptimized',
    'c_std=gnu11',
    'warning_level=2'
  ]
)

with_gstreamer = get_option('gstreamer')
with_opencv = get_option('opencv')
with_pipewire = get_option('pipewire')

if with_gstreamer == 'true' and with_pipewire == 'true'
  error('GStreamer and PipeWire support can not be enabled at the same time')
endif
if with_gstreamer == 'true' and with_pipewire == 'auto'
  with_pipewire = 'false'
endif
if with_pipewire == 'true' and with_gstreamer == 'auto'
  with_gstreamer = 'false'
endif

add_global_arguments(['-fno-math-errno'], language : 'c')

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
  opencv_dep = dependency('opencv4', required : false)
  if not opencv_dep.found()
    opencv_dep = dependency('opencv', required : with_opencv == 'true')
  endif
else
  opencv_dep = []
endif
if with_pipewire != 'false'
  pw_dep = dependency('libpipewire-0.2', version : '>= 0.2.2',
    required : with_pipewire == 'true'
  )
  spa_dep = dependency('libspa-0.1', version : '>= 0.2.2',
    required : with_pipewire == 'true'
  )
else
  pw_dep = []
  spa_dep = []
endif
thread_dep = dependency('threads')
udev_dep = dependency('libudev')
usb_dep = dependency('libusb-1.0', version : '>= 1.0.16')
zlib_dep = dependency('zlib')
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)

foreach h : ['linux/hidraw.h', 'linux/uvcvideo.h', 'linux/usb/video.h', 'linux/videodev2.h']
  if not cc.compiles('#include <@0@>'.format(h), name : '@0@'.format(h))
    error('Missing Linux kernel headers')
  endif
endforeach

build_gst = with_gstreamer != 'false' and gst_dep.found()
build_opencv = with_opencv != 'false' and opencv_dep.found()
build_pw = with_pipewire != 'false' and pw_dep.found() and spa_dep.found()
if build_pw and build_gst
  warning('GStreamer and PipeWire support can not be enabled at the same time, GStreamer disabled')
  build_gst = false
endif

if build_gst
  add_global_arguments('-DHAVE_GST=1', language : 'c')
  add_global_arguments('-DHAVE_DEBUG_STREAM=1', language : 'c')
endif
if build_opencv
  add_global_arguments('-DHAVE_OPENCV=1', language : 'c')
endif
if build_pw
	add_global_arguments('-DHAVE_PIPEWIRE=1', language : 'c')
  add_global_arguments('-DHAVE_DEBUG_STREAM=1', language : 'c')
endif

subdir('xml')

subdir('src')

inc_src = include_directories('src')

subdir('tools')
