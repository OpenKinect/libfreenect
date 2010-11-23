#!/usr/bin/env python
import freenect
import time
import threading
import random

dev = None


def tilt_and_sense():
    global dev
    while not dev:
        time.sleep(1)
    while 1:
        time.sleep(3)
        led = random.randint(0, 6)
        tilt = random.randint(0, 30)
        print('Led[%d] Tilt[%d]' % (led, tilt))
        freenect.set_led(dev, led)
        freenect.set_tilt_degs(dev, tilt)
        print(freenect.raw_accel(dev))
        print(freenect.mks_accel(dev))
threading.Thread(target=tilt_and_sense).start()


def dev_getter(my_dev, *_):
    global dev
    dev = my_dev
freenect.runloop(depth=lambda *x: dev_getter(*freenect.depth_cb_np(*x)))
