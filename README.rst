ouvrt
=====

1. About_
2. `Setup and build`_
3. ouvrtd_
4. Tools_
5. Todo_

.. _About: `1. About`_
.. _Setup and build: `2. Setup and build`_
.. _ouvrtd: `3. ouvrtd`_
.. _Tools: `4. Tools`_
.. _Todo: `5. Todo`_

1. About
--------

ouvrt is a playground to understand how the positional tracking systems used
by the PlayStation VR, Oculus Rift (DK2, CV1), and HTC Vive virtual reality
headsets work. The main component is the ouvrtd daemon that detects and opens
relevant USB devices and sets them up for tracking.

Currently the following devices can be detected:

- Lenovo Explorer headset (USB)
- PlayStation VR headset (USB, via PSVR processing box)
- Rift CV1 headset (USB)
- Rift DK2 headset (USB)
- Rift DK2 Positional Tracker (USB)
- Rift remote (wireless via Rift CV1 headset)
- Rift touch controller (wireless via Rift CV1 headset)
- Vive headset (USB)
- Vive base station (optical via Vive headset or controller)
- Vive controller (USB or wireless via Vive headset)

Features are still limited to enabling the tracking LEDs for PlayStation VR,
Rift DK2 and CV1, setting up the camera sensor for synchronized exposure (DK2
only), and capturing video frames into a GStreamer pipeline for debugging
(DK2). IMU sensor data is captured and can be sent along with axis and button
state via UDP.

2. Setup and build
------------------

The following prerequisite libraries and development packages are necessary
to build ouvrt:

- GLib/GObject/GIO
- GStreamer (optional)
- JSON-GLib
- OpenCV (optional)
- libudev
- Linux kernel headers (hidraw, uvc, v4l2)
- Meson
- zlib
- PipeWire (optional)

On a Debian stretch system these can be installed with the following commands::

  $ apt-get install build-essential libglib2.0-dev libjson-glib-dev \
    libudev-dev meson pkg-config

And optionally::

  $ apt-get install libgstreamer-1.0-dev
  $ apt-get install libopencv-dev
  $ apt-get install libpipewire-0.2-dev libspa-lib-0.1-dev

To configure the build system and build everything, follow the standard Meson
build procedure::

  $ meson builddir
  $ cd builddir
  $ ninja

To build without an optional dependency, disable the corresponding
option before calling ninja, for example::

  $ cd builddir
  $ meson configure -D gstreamer=false -D opencv=false -D pipewire=false

3. ouvrtd
---------

Make sure you have permissions to access the /dev/hidraw and /dev/video devices
corresponding to the Rift DK2 and the DK2 Positional Tracker. Then run ouvrtd::

  $ ./ouvrtd

If compiled with GStreamer support, the daemon will create a shared memory
socket /tmp/ouvrtd-gst-0 and, if a DK2 Positional Tracker is connected, write
frames into it as soon as a GStreamer shmsrc connects to it. To see the
captured frames, run::

  $ gst-launch-1.0 shmsrc socket-path=/tmp/ouvrtd-gst-0 is-live=true ! \
    video/x-raw,format=GRAY8,width=752,height=480,framerate=60/1 ! \
    videoconvert ! autovideosink

4. Tools
--------

The dump-eeprom tool reads the Positional Tracker DK2 EEPROM and writes it to
a file or stdout::

  $ ./dump-eeprom camera-dk2-rom.bin

  $ ./dump-eeprom - | hexdump -C

5. Todo
-------

- Add blob detection and tracking
- Enable Rift DK2 IR LED blinking patterns
- Add individual blinking LED detection to the blob tracker
- Add a 3D model of the tracking LEDs, readout from the Rift
- Add support for camera intrinsic and lens distortion parameters, readout
  from the Rift and/or camera EEPROM
- Add a PnP solver to estimate the pose from 3D-2D point correspondences
- Add RANSAC PnP solver support for the initial pose estimation
- Feed the projection of the estimated pose back as starting points for the
  PnP solver and blob tracker
- Add Rift DK2 IMU support to estimate the pose from integrated gyro and
  acceleration sensor readouts
- Add sensor fusion, correcting the IMU pose from the camera pose regularly,
  use the fused pose estimate to feed back into PnP solver and blob tracker
- Implement proper time handling for all of this
