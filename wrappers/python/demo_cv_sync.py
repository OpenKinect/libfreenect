#!/usr/bin/env python
import freenect
import cv
import numpy as np

cv.NamedWindow('Depth')
cv.NamedWindow('Video')
while 1:
    depth, timestamp = freenect.sync_get_depth()
    rgb, timestamp = freenect.sync_get_video()

    # maximize dynamic range of the given 11 bits into the used 16 bits
    depth<<=(16-11)

    cv.ShowImage('Depth', depth.astype(np.uint16))
    cv.ShowImage('Video', rgb[:, :, ::-1].astype(np.uint8))
    cv.WaitKey(10)
