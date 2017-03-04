from freenect import *
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
        freenect_set_led(dev, led)
        freenect_set_tilt_in_degrees(dev, tilt)
        print(raw_accelerometers(dev))
        print(mks_accelerometers(dev))
threading.Thread(target=tilt_and_sense).start()


def dev_getter(my_dev, *_):
    global dev
    dev = my_dev
runloop(depth_cb_factory(dev_getter), lambda *x: None)
