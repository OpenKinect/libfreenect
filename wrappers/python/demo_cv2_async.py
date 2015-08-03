#!/usr/bin/env python
import freenect
import cv2
import frame_convert2

cv2.namedWindow('Depth')
cv2.namedWindow('RGB')
keep_running = True


def display_depth(dev, data, timestamp):
    global keep_running
    cv2.imshow('Depth', frame_convert2.pretty_depth_cv(data))
    if cv2.waitKey(10) == 27:
        keep_running = False


def display_rgb(dev, data, timestamp):
    global keep_running
    cv2.imshow('RGB', frame_convert2.video_cv(data))
    if cv2.waitKey(10) == 27:
        keep_running = False


def body(*args):
    if not keep_running:
        raise freenect.Kill


print('Press ESC in window to stop')
freenect.runloop(depth=display_depth,
                 video=display_rgb,
                 body=body)
