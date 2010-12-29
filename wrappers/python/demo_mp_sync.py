#!/usr/bin/env python
import freenect
import matplotlib.pyplot as mp
import frame_convert
import signal

keep_running = True


def get_depth():
    return frame_convert.pretty_depth(freenect.sync_get_depth()[0])


def get_video():
    return freenect.sync_get_video()[0]


def handler(signum, frame):
    """Sets up the kill handler, catches SIGINT"""
    global keep_running
    keep_running = False


mp.ion()
mp.gray()
mp.figure(1)
image_depth = mp.imshow(get_depth(), interpolation='nearest', animated=True)
mp.figure(2)
image_rgb = mp.imshow(get_video(), interpolation='nearest', animated=True)
print('Press Ctrl-C in terminal to stop')
signal.signal(signal.SIGINT, handler)

while keep_running:
    mp.figure(1)
    image_depth.set_data(get_depth())
    mp.figure(2)
    image_rgb.set_data(get_video())
    mp.draw()
    mp.waitforbuttonpress(0.01)
