#!/usr/bin/env python
import freenect
import cv
import numpy as np

cv.NamedWindow('Depth')
cv.NamedWindow('RGB')
keep_running = True


def display_depth(dev, data, timestamp):
    global keep_running
    data = data.astype(np.uint8)
    image = cv.CreateImageHeader((data.shape[1], data.shape[0]),
                                 cv.IPL_DEPTH_8U,
                                 1)
    cv.SetData(image, data.tostring(),
               data.dtype.itemsize * data.shape[1])
    cv.ShowImage('Depth', image)
    if cv.WaitKey(10) == 27:
        keep_running = False


def display_rgb(dev, data, timestamp):
    global keep_running
    image = cv.CreateImageHeader((data.shape[1], data.shape[0]),
                                 cv.IPL_DEPTH_8U,
                                 3)
    # Note: We swap from RGB to BGR here
    cv.SetData(image, data[:, :, ::-1].tostring(),
               data.dtype.itemsize * 3 * data.shape[1])
    cv.ShowImage('RGB', image)
    if cv.WaitKey(10) == 27:
        keep_running = False


def body(*args):
    if not keep_running:
        raise freenect.Kill
print('Press ESC in window to stop')
freenect.runloop(depth=display_depth,
                 video=display_rgb,
                 body=body)
