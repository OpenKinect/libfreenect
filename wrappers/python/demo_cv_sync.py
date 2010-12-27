#!/usr/bin/env python
import freenect
import cv
import numpy as np

cv.NamedWindow('Depth')
cv.NamedWindow('Video')
size = (640, 480)

# preallocate temporary images to avoid GC stalls and runtime memory allocs
rgb=np.zeros((size[1],size[0],3),np.uint8)

while 1:
    depth, timestamp = freenect.sync_get_depth()
    bgr, timestamp = freenect.sync_get_video()

    # maximize dynamic range of the given 11 bits into the used 16 bits
    depth<<=(16-11)
    # Reordering bgr as rgb
    # Using the slice directly doesn't work because the underlying buffer 
    # is used and it does not change.
    rgb[:] = bgr[:,:,::-1]

    cv.ShowImage('Depth', depth)
    cv.ShowImage('Video', rgb)
    cv.WaitKey(10)

