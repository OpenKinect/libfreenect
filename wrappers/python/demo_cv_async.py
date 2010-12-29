#!/usr/bin/env python
import freenect
import cv
import frame_convert

cv.NamedWindow('Depth')
cv.NamedWindow('RGB')
keep_running = True


def display_depth(dev, data, timestamp):
    global keep_running
    cv.ShowImage('Depth', frame_convert.pretty_depth_cv(data))
    if cv.WaitKey(10) == 27:
        keep_running = False


def display_rgb(dev, data, timestamp):
    global keep_running
    cv.ShowImage('RGB', frame_convert.video_cv(data))
    if cv.WaitKey(10) == 27:
        keep_running = False


def body(*args):
    if not keep_running:
        raise freenect.Kill


print('Press ESC in window to stop')
freenect.runloop(depth=display_depth,
                 video=display_rgb,
                 body=body)
