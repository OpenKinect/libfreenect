#!/usr/bin/env python
import freenect
import cv
import numpy as np

cv.NamedWindow('Depth')
cv.NamedWindow('Video')
ind = 0
while 1:
    print(ind)
    try:
        depth, timestamp = freenect.sync_get_depth(ind)
        rgb, timestamp = freenect.sync_get_video(ind)
    except TypeError:
        ind = 0
        continue
    # maximize dynamic range of the given 11 bits into the used 16 bits
    depth<<=(16-11)
    ind += 1
    cv.ShowImage('Depth', depth.astype(np.uint16))
    cv.ShowImage('Video', rgb[:, :, ::-1].astype(np.uint8))
    cv.WaitKey(10)
    freenect.sync_stop()# NOTE: May remove if you have good USB bandwidth
