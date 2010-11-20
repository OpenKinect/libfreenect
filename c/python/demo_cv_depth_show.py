from freenect import *
import cv
import numpy as np

cv.NamedWindow('Depth')


def display(dev, data, timestamp):
    data -= np.min(data.ravel())
    data *= 65536 / np.max(data.ravel())
    image = cv.CreateImageHeader((data.shape[1], data.shape[0]),
                                 cv.IPL_DEPTH_16U,
                                 1)
    cv.SetData(image, data.tostring(),
               data.dtype.itemsize * data.shape[1])
    cv.ShowImage('Depth', image)
    cv.WaitKey(5)
runloop(depth=depth_cb_np_factory(display))
