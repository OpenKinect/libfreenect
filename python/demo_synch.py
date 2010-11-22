#!/usr/bin/env python
from freenect_synch import get_depth, get_rgb
import cv  
import numpy as np


def array2cv(a):
  dtype2depth = {
        'uint8':   cv.IPL_DEPTH_8U,
        'int8':    cv.IPL_DEPTH_8S,
        'uint16':  cv.IPL_DEPTH_16U,
        'int16':   cv.IPL_DEPTH_16S,
        'int32':   cv.IPL_DEPTH_32S,
        'float32': cv.IPL_DEPTH_32F,
        'float64': cv.IPL_DEPTH_64F,
    }
  try:
    nChannels = a.shape[2]
  except:
    nChannels = 1
  cv_im = cv.CreateImageHeader((a.shape[1],a.shape[0]),
          dtype2depth[str(a.dtype)],
          nChannels)
  cv.SetData(cv_im, a.tostring(),
             a.dtype.itemsize*nChannels*a.shape[1])
  return cv_im
  
def doloop():
    cv.NamedWindow('depth')
    cv.NamedWindow('rgb')
    global depth, rgb
    while True:
        # Get a fresh frame
        depth, rgb = get_depth(), get_rgb()
        
        # Build a two panel color image
        d3 = np.dstack((depth,depth,depth)).astype(np.uint8)
        da = np.hstack((d3,rgb))
        
        # Simple Downsample
        cv.ShowImage('both',array2cv(da[::2,::2,::-1]))
        cv.WaitKey(5)
        
doloop()

"""
IPython usage:
 ipython
 [1]: run -i demo_bgf
 #<ctrl -c>  (to interrupt the loop)
 [2]: %timeit -n100 QueryDepth(),QueryRGB() # profile the kinect capture

"""

