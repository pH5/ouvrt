#!/bin/sh
# Example of capturing the camera video feed from the debug SHM socket to an mp4
# 1280x720x52/1 is for a CV1 camera

if [ $# -lt 1 ]; then
    echo "Usage: $0 filename.mp4"
    echo "Please supply a filename for the output location"
    exit 1
fi
OUT="$1"

echo "Recording to $OUT. Press Ctrl-C to exit"
gst-launch-1.0 -e shmsrc socket-path=/tmp/ouvrtd-gst-0 is-live=true ! \
    video/x-raw,format=GRAY8,width=1280,height=720,framerate=52/1 ! \
    videoconvert ! queue max-size-bytes=0 max-size-buffers=0 ! jpegenc ! \
    jpegparse ! qtmux ! filesink location="$OUT"
