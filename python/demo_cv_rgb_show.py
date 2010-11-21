from freenect import *
import cv

cv.NamedWindow('RGB')


def display(dev, data, timestamp):
    image = cv.CreateImageHeader((data.shape[1], data.shape[0]),
                                 cv.IPL_DEPTH_8U,
                                 3)
    # Note: We swap from RGB to BGR here
    cv.SetData(image, data[:, :, ::-1].tostring(),
               data.dtype.itemsize * 3 * data.shape[1])
    cv.ShowImage('RGB', image)
    cv.WaitKey(5)
runloop(rgb=rgb_cb_np_factory(display))
