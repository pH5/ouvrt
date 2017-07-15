# Copyright 2016-2018 Philipp Zabel
# SPDX-License-Identifier:	GPL-2.0+

libouvrt_sources = [
  'ar0134.c',
  'ar0134.h',
  'blobwatch.c',
  'blobwatch.h',
  'esp570.c',
  'esp570.h',
  'esp770u.c',
  'esp770u.h',
  'flicker.c',
  'flicker.h',
  'mt9v034.c',
  'mt9v034.h',
  'uvc.c',
  'uvc.h'
]
libouvrt_deps = [
  glib_dep,
  m_dep,
  usb_dep
]
libouvrt = static_library(
  'libouvrt',
  libouvrt_sources,
  dependencies : libouvrt_deps
)

ouvrtd_sources = [
  gdbus_generated,
  'camera.c',
  'camera-dk2.c',
  'camera-dk2.h',
  'camera.h',
  'camera-v4l2.c',
  'camera-v4l2.h',
  'dbus.c',
  'dbus.h',
  'debug.c',
  'debug.h',
  'device.c',
  'device.h',
  'hololens-camera.c',
  'hololens-camera.h',
  'hololens-hid-reports.h',
  'hololens-imu.c',
  'hololens-imu.h',
  'imu.c',
  'imu.h',
  'json.c',
  'json.h',
  'leds.c',
  'leds.h',
  'lenovo-explorer.c',
  'lenovo-explorer.h',
  'lighthouse.c',
  'lighthouse.h',
  'maths.c',
  'maths.h',
  'motion-controller.c',
  'motion-controller.h',
  'opencv.h',
  'ouvrtd.c',
  'psvr.c',
  'psvr.h',
  'psvr-hid-reports.h',
  'rift.c',
  'rift.h',
  'rift-hid-reports.h',
  'rift-radio.c',
  'rift-radio.h',
  'telemetry.c',
  'telemetry.h',
  'tracker.c',
  'tracker.h',
  'tracking-model.c',
  'tracking-model.h',
  'usb-device.c',
  'usb-device.h',
  'usb-ids.h',
  'vive-config.c',
  'vive-config.h',
  'vive-controller.c',
  'vive-controller.h',
  'vive-controller-usb.c',
  'vive-controller-usb.h',
  'vive-firmware.c',
  'vive-firmware.h',
  'vive-headset.c',
  'vive-headset.h',
  'vive-headset-mainboard.c',
  'vive-headset-mainboard.h',
  'vive-imu.c',
  'vive-imu.h'
]
if build_gst
	ouvrtd_sources += [ 'debug-gst.c' ]
endif
if build_opencv
	ouvrtd_sources += [ 'opencv.cpp' ]
endif
ouvrtd_deps = [
  glib_dep,
  gio_dep,
  json_glib_dep,
  m_dep,
  thread_dep,
  udev_dep,
  usb_dep,
  zlib_dep,
  # optional
  gst_dep,
  opencv_dep
]
executable(
  'ouvrtd',
  ouvrtd_sources,
  dependencies : ouvrtd_deps,
  link_with : libouvrt,
  install : true
)
