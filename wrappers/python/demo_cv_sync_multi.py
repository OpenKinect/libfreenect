#!/usr/bin/env python
import freenect
import cv
import numpy as np

cv.NamedWindow('Depth')
cv.NamedWindow('Video')
ind = 0
size = (640, 480)

# preallocate temporary images to avoid GC stalls and runtime memory allocs
rgb=np.zeros((size[1],size[0],3),np.uint8)

while 1:
    print(ind)
    try:
        depth, timestamp = freenect.sync_get_depth(ind)
        bgr, timestamp = freenect.sync_get_video(ind)
    except TypeError:
        ind = 0
        continue
    # maximize dynamic range of the given 11 bits into the used 16 bits
    depth<<=(16-11)
    # Reordering bgr as rgb
    # Using the slice directly doesn't work because the underlying buffer 
    # is used and it does not change.
    rgb[:] = bgr[:,:,::-1]
    ind += 1

    cv.ShowImage('Depth', depth)
    cv.ShowImage('Video', rgb)
    cv.WaitKey(10)
    freenect.sync_stop()# NOTE: May remove if you have good USB bandwidth

