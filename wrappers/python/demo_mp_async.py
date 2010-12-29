#!/usr/bin/env python
import freenect
import matplotlib.pyplot as mp
import signal
import frame_convert

mp.ion()
image_rgb = None
image_depth = None
keep_running = True


def display_depth(dev, data, timestamp):
    global image_depth
    data = frame_convert.pretty_depth(data)
    mp.gray()
    mp.figure(1)
    if image_depth:
        image_depth.set_data(data)
    else:
        image_depth = mp.imshow(data, interpolation='nearest', animated=True)
    mp.draw()


def display_rgb(dev, data, timestamp):
    global image_rgb
    mp.figure(2)
    if image_rgb:
        image_rgb.set_data(data)
    else:
        image_rgb = mp.imshow(data, interpolation='nearest', animated=True)
    mp.draw()


def body(*args):
    if not keep_running:
        raise freenect.Kill


def handler(signum, frame):
    global keep_running
    keep_running = False


print('Press Ctrl-C in terminal to stop')
signal.signal(signal.SIGINT, handler)
freenect.runloop(depth=display_depth,
                 video=display_rgb,
                 body=body)
