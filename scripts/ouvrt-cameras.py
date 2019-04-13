#!/usr/bin/python3
# PipeWire ouvrt camera observer
# Copyright 2019 Philipp Zabel
# SPDX-License-Identifier:	GPL-2.0+

import gi, signal, os, sys
gi.require_version('Gst', '1.0')
gi.require_version('GLib', '2.0')
from gi.repository import GLib, Gst

def bus_call(bus, message, loop):
    if message.type == Gst.MessageType.ERROR:
        err, debug = message.parse_error()
        if err.domain == 'gst-resource-error-quark' and err.code == 3 and \
           err.message == 'Output window was closed':
            None
        else:
            sys.stderr.write('Error: %s: %s\n' % (err, debug))
        loop.quit()
    return True

class SigHandler():
    def __init__(self, loop):
        self.loop = loop
        for sig in [signal.SIGINT, signal.SIGTERM]:
            signal.signal(sig, self.handler)

    def handler(self, sig, frame):
        self.loop.quit()

def main(args):
    Gst.init(args)

    provider = Gst.DeviceProviderFactory.get_by_name('pipewiredeviceprovider')
    if not provider:
        sys.stderr.write('PipeWire device provider not found.\n' +
                         'Is the GStreamer PipeWire plugin installed?\n')
        return 1

    ok = provider.start()
    if not ok:
        sys.stderr.write('Failed to start PipeWire device provider.\n')
        return 1

    # FIXME: get_devices() segfaults if the provider is not started
    devices = provider.get_devices()

    provider.stop()

    sources = []

    for device in devices:
        name = device.get_display_name()
        props = device.get_properties()
        media_name = props['media.name']
        category = props['pipewire.category']
        if media_name != 'ouvrt-camera' or category != 'Capture':
            continue
        s = device.get_caps().get_structure(0)
        print(s)
        if s.get_name() != 'video/x-raw':
            continue
        src = device.create_element('pipewiresrc')
        sources.append(src)

    if len(sources) == 0:
        sys.stderr.write('Failed to find PipeWire capture device.\n' +
                         'Is ouvrtd running and are cameras connected?\n')
        return 1

    loop = GLib.MainLoop()

    SigHandler(loop)

    pipes = []

    for src in sources:
        pipe = Gst.Pipeline.new('pipe0')
        convert = Gst.ElementFactory.make('videoconvert')
        sink = Gst.ElementFactory.make('autovideosink')

        pipe.add(src)
        pipe.add(convert)
        pipe.add(sink)

        src.link(convert)
        convert.link(sink)

        pipe.set_state(Gst.State.PLAYING)

        bus = pipe.get_bus()
        bus.add_signal_watch()
        bus.connect('message', bus_call, loop)

        pipes.append(pipe)

    loop.run()

    for pipe in pipes:
        pipe.set_state(Gst.State.NULL)

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
