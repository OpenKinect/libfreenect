#!/usr/bin/env python
"""Sweeps throught the depth image showing 100 range at a time"""
import freenect
import cv2
import numpy as np
import time

cv2.namedWindow('Depth')


def disp_thresh(lower, upper):
    depth, timestamp = freenect.sync_get_depth()
    depth = 255 * np.logical_and(depth > lower, depth < upper)
    depth = depth.astype(np.uint8)
    cv2.imshow('Depth', depth)
    cv2.waitKey(10)


lower = 0
upper = 100
max_upper = 2048
while upper < max_upper:
    print('%d < depth < %d' % (lower, upper))
    disp_thresh(lower, upper)
    time.sleep(.1)
    lower += 20
    upper += 20
