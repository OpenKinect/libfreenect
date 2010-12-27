#!/usr/bin/env python
import freenect
import cv
import numpy as np

cv.NamedWindow('Depth')
cv.NamedWindow('RGB')

die = False

def display_depth(dev, data, timestamp):
    data <<= (16-11) # 11 bits -> 16 bits
    image = cv.CreateImageHeader((data.shape[1], data.shape[0]),
                                 cv.IPL_DEPTH_16U,
                                 1)
    cv.SetData(image, data.tostring(),
               data.dtype.itemsize * data.shape[1])
    cv.ShowImage('Depth', image)
    global die
    if cv.WaitKey(5) == 27 : die = True

rgb = None

def display_rgb(dev, data, timestamp):
    global rgb
    if rgb is None :
        rgb = np.zeros(data.shape, np.uint8)
    rgb[:] = data[:, :, ::-1]
    image = cv.CreateImageHeader((data.shape[1], data.shape[0]),
                                 cv.IPL_DEPTH_8U,
                                 3)
    # Note: We swap from RGB to BGR here
    cv.SetData(image, rgb,
               data.dtype.itemsize * 3 * data.shape[1])
    cv.ShowImage('RGB', image)
    global die
    if cv.WaitKey(5) == 27 : die = True

def body(context, device) :
    if die : raise freenect.Kill

freenect.runloop(depth=display_depth,
                 video=display_rgb,
                 body=body)

