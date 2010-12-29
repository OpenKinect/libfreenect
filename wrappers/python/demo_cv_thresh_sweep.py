#!/usr/bin/env python
"""Sweeps throught the depth image showing 100 range at a time"""
import freenect
import cv
import numpy as np
import time

cv.NamedWindow('Depth')


def disp_thresh(lower, upper):
    depth, timestamp = freenect.sync_get_depth()
    depth = 255 * np.logical_and(depth > lower, depth < upper)
    depth = depth.astype(np.uint8)
    image = cv.CreateImageHeader((depth.shape[1], depth.shape[0]),
                                 cv.IPL_DEPTH_8U,
                                 1)
    cv.SetData(image, depth.tostring(),
               depth.dtype.itemsize * depth.shape[1])
    cv.ShowImage('Depth', image)
    cv.WaitKey(10)


lower = 0
upper = 100
max_upper = 2048
while upper < max_upper:
    print('%d < depth < %d' % (lower, upper))
    disp_thresh(lower, upper)
    time.sleep(.1)
    lower += 20
    upper += 20
