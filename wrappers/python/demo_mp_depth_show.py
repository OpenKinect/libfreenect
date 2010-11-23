#!/usr/bin/env python
import freenect
import matplotlib.pyplot as mp

mp.ion()
image = None


def display(dev, data, timestamp):
    global image
    if image:
        image.set_data(data)
    else:
        image = mp.imshow(data, interpolation='nearest', animated=True)
    mp.draw()

if __name__ == '__main__':
    freenect.runloop(depth=lambda *x: display(*freenect.depth_cb_np(*x)))
