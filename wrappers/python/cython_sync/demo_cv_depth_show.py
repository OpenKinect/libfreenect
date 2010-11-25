import freenect
import cv
import numpy as np

freenect.start()

cv.NamedWindow('Depth')    
while 1:
    depth = freenect.get_depth_np()
    cv.ShowImage('Depth', depth.astype(np.uint8))
    cv.WaitKey(10)

