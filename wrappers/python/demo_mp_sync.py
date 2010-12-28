#!/usr/bin/env python
import freenect
import matplotlib.pyplot as mp
import numpy as np
import signal

keep_running = True
mp.ion()
mp.figure(1)
mp.gray()
image_depth = mp.imshow(freenect.sync_get_depth()[0].astype(np.uint8),
                        interpolation='nearest', animated=True)
mp.figure(2)
image_rgb = mp.imshow(freenect.sync_get_video()[0],
                      interpolation='nearest', animated=True)


def handler(signum, frame):
    """Sets up the kill handler, catches SIGINT"""
    global keep_running
    keep_running = False
print('Press Ctrl-C in terminal to stop')
signal.signal(signal.SIGINT, handler)

while keep_running:
    mp.figure(1)
    image_depth.set_data(freenect.sync_get_depth()[0].astype(np.uint8))
    mp.figure(2)
    image_rgb.set_data(freenect.sync_get_video()[0])
    mp.draw()
    mp.waitforbuttonpress(0.01)
