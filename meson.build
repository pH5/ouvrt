project('ouvrt', ['c', 'cpp'],
	version : '0.1',
	meson_version : '>= 0.35',
	license : 'GPL')

add_global_arguments('-Wall', language : 'c')

gio_dep = dependency('gio-unix-2.0')
gl_dep = dependency('gl')
glfw_dep = dependency('glfw3')
glib_dep = dependency('glib-2.0')
gobj_dep = dependency('gobject-2.0')
gst_dep = dependency('gstreamer-1.0')
json_glib_dep = dependency('json-glib-1.0')
opencv_dep = dependency('opencv', required : false)
thread_dep = dependency('threads')
udev_dep = dependency('libudev')
zlib_dep = dependency('zlib')
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)

gdbus_codegen = find_program('gdbus-codegen')
sed = find_program('sed')

tracker_xml = 'xml/de.phfuenf.ouvrt.Tracker1.xml'
camera_xml = 'xml/de.phfuenf.ouvrt.Camera1.xml'

gdbus_generated = custom_target('gdbus-generated.h',
	input : [tracker_xml, camera_xml],
	output : ['gdbus-generated.c', 'gdbus-generated.h'],
	command : [gdbus_codegen, '--interface-prefix', 'de.phfuenf.ouvrt.',
		'--generate-c-code', 'gdbus-generated',
		'--c-namespace', 'Ouvrt',
		'--c-generate-object-manager',
		'@INPUT@'])

libouvrt_sources = [
	'src/blobwatch.c',
	'src/blobwatch.h',
	'src/esp570.c',
	'src/esp570.h',
	'src/flicker.c',
	'src/flicker.h',
	'src/mt9v034.c',
	'src/mt9v034.h'
]
libouvrt = static_library('libouvrt', libouvrt_sources)

ouvrtd_sources = [
	gdbus_generated,
	'src/camera.c',
	'src/camera-dk2.c',
	'src/camera-dk2.h',
	'src/camera.h',
	'src/camera-v4l2.c',
	'src/camera-v4l2.h',
	'src/dbus.c',
	'src/dbus.h',
	'src/debug.c',
	'src/debug-gst.c',
	'src/debug-gst.h',
	'src/debug.h',
	'src/device.c',
	'src/device.h',
	'src/imu.h',
	'src/json.c',
	'src/json.h',
	'src/leds.c',
	'src/leds.h',
	'src/lighthouse.c',
	'src/lighthouse.h',
	'src/math.c',
	'src/math.h',
	'src/opencv.h',
	'src/ouvrtd.c',
	'src/psvr.c',
	'src/psvr.h',
	'src/psvr-hid-reports.h',
	'src/rift.c',
	'src/rift.h',
	'src/rift-hid-reports.h',
	'src/rift-radio.c',
	'src/rift-radio.h',
	'src/tracker.c',
	'src/tracker.h',
	'src/tracking-model.c',
	'src/tracking-model.h',
	'src/usb-ids.h',
	'src/vive-config.c',
	'src/vive-config.h',
	'src/vive-controller.c',
	'src/vive-controller.h',
	'src/vive-controller-usb.c',
	'src/vive-controller-usb.h',
	'src/vive-firmware.c',
	'src/vive-firmware.h',
	'src/vive-headset.c',
	'src/vive-headset.h',
	'src/vive-headset-mainboard.c',
	'src/vive-headset-mainboard.h',
	'src/vive-imu.c',
	'src/vive-imu.h'
]
if opencv_dep.found()
	ouvrtd_sources += [ 'src/opencv.cpp' ]
endif
ouvrtd_deps = [
	glib_dep,
	gio_dep,
	gst_dep,
	json_glib_dep,
	opencv_dep,
	thread_dep,
	udev_dep,
	zlib_dep
]
executable('ouvrtd', ouvrtd_sources,
	dependencies : ouvrtd_deps,
	link_with : libouvrt)

executable('dump-eeprom', 'tools/dump-eeprom.c',
	include_directories : include_directories('src'),
	link_with : libouvrt)
