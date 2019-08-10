# Copyright 2016-2018 Philipp Zabel
# SPDX-License-Identifier: GPL-2.0-or-later

gnome = import('gnome')

tracker_xml = 'de.phfuenf.ouvrt.Tracker1.xml'
camera_xml = 'de.phfuenf.ouvrt.Camera1.xml'
radio_xml = 'de.phfuenf.ouvrt.Radio1.xml'

gdbus_generated = gnome.gdbus_codegen(
  'gdbus-generated',
  sources: [
    tracker_xml,
    camera_xml,
    radio_xml,
  ],
  interface_prefix: 'de.phfuenf.ouvrt.',
  namespace: 'Ouvrt',
  object_manager: true
)
