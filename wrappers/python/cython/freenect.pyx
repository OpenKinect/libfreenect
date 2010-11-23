import cython

FORMAT_RGB = 0
FORMAT_BAYER = 1
FORMAT_11_BIT = 0
FORMAT_10_BIT = 1
LED_OFF = 0
LED_GREEN = 1
LED_RED = 2
LED_YELLOW = 3
LED_BLINK_YELLOW = 4
LED_BLINK_GREEN = 5
LED_BLINK_RED_YELLOW = 6

cdef extern from "Python.h":
    object PyString_FromStringAndSize(char *s, Py_ssize_t len)

cdef extern from "libfreenect.h":
    ctypedef void (*freenect_depth_cb)(void *dev, char *depth, int timestamp) # was u_int32
    ctypedef void (*freenect_rgb_cb)(void *dev, char *rgb, int timestamp) # was u_int32
    int freenect_init(void **ctx, int usb_ctx) # changed from void * as usb_ctx is always NULL
    int freenect_shutdown(void *ctx)
    int freenect_process_events(void *ctx)
    int freenect_num_devices(void *ctx)
    int freenect_open_device(void *ctx, void **dev, int index)
    int freenect_close_device(void *dev)
    #void freenect_set_user(void *dev, void *user)
    #void *freenect_get_user(void *dev)
    void freenect_set_depth_callback(void *dev, freenect_depth_cb cb)
    void freenect_set_rgb_callback(void *dev, freenect_rgb_cb cb)
    int freenect_set_rgb_format(void *dev, int fmt)
    int freenect_set_depth_format(void *dev, int fmt)
    int freenect_start_depth(void *dev)
    int freenect_start_rgb(void *dev)
    int freenect_stop_depth(void *dev)
    int freenect_stop_rgb(void *dev)
    int freenect_set_tilt_degs(void *dev, double angle)
    int freenect_set_led(void *dev, int option)
    int freenect_get_raw_accel(void *dev, short int* x, short int* y, short int* z) # had to make these short int
    int freenect_get_mks_accel(void *dev, double* x, double* y, double* z)


cdef class DevPtr:
   cdef void* _ptr 
   def __repr__(self): 
      return "<Dev Pointer>"

cdef class CtxPtr:
   cdef void* _ptr 
   def __repr__(self): 
      return "<Ctx Pointer>"

def set_rgb_format(DevPtr dev, int fmt):
    return freenect_set_rgb_format(dev._ptr, fmt)

def set_depth_format(DevPtr dev, int fmt):
    return freenect_set_depth_format(dev._ptr, fmt)

def start_depth(DevPtr dev):
    return freenect_start_depth(dev._ptr)

def start_rgb(DevPtr dev):
    return freenect_start_rgb(dev._ptr)

def stop_depth(DevPtr dev):
    return freenect_stop_depth(dev._ptr)

def stop_rgb(DevPtr dev):
    return freenect_stop_rgb(dev._ptr)

def shutdown(CtxPtr ctx):
    return freenect_shutdown(ctx._ptr)

def process_events(CtxPtr ctx):
    return freenect_process_events(ctx._ptr)

def num_devices(CtxPtr ctx):
    return freenect_num_devices(ctx._ptr)

def close_device(DevPtr dev):
    return freenect_close_device(dev._ptr)

def set_tilt_degs(DevPtr dev, float angle):
    freenect_set_tilt_degs(dev._ptr, angle)

def set_led(DevPtr dev, int option):
    return freenect_set_led(dev._ptr, option)

cdef init():
    cdef void* ctx
    if freenect_init(cython.address(ctx), 0) < 0:
        print('Error: Cant open')
    cdef CtxPtr ctx_out
    ctx_out = CtxPtr()
    ctx_out._ptr = ctx
    return ctx_out

cdef open_device(CtxPtr ctx, int index):
    cdef void* dev
    if freenect_open_device(ctx._ptr, cython.address(dev), index) < 0:
        print('Error: Cant open')    
    cdef DevPtr dev_out
    dev_out = DevPtr()
    dev_out._ptr = dev
    return dev_out

_depth_cb, _rgb_cb = None, None

cdef void depth_cb(void *dev, char *data, int timestamp):
    nbytes = 614400  # 480 * 640 * 2
    cdef DevPtr dev_out
    dev_out = DevPtr()
    dev_out._ptr = dev
    print(timestamp)
    if _depth_cb:
       _depth_cb(dev_out, PyString_FromStringAndSize(data, nbytes), timestamp)


cdef void rgb_cb(void *dev, char *data, int timestamp):
    nbytes = 921600  # 480 * 640 * 3
    cdef DevPtr dev_out
    dev_out = DevPtr()
    dev_out._ptr = dev
    print(timestamp)
    if _rgb_cb:
       _rgb_cb(dev_out, PyString_FromStringAndSize(data, nbytes), timestamp)


def runloop(depth=None, rgb=None):
    """Sets up the kinect and maintains a runloop

    This is where most of the action happens.  You can get the dev pointer from the callback
    and let this function do all of the setup for you.  You may want to use threads to perform
    computation externally as the callbacks should really just be used for copying data.

    Args:
        depth: A function that takes (dev, depth, timestamp), corresponding to C function.
            If None (default), then you won't get a callback for depth.
        rgb: A function that takes (dev, rgb, timestamp), corresponding to C function.
            If None (default), then you won't get a callback for rgb.
    """
    global _depth_cb, _rgb_cb
    if depth:
       _depth_cb = depth
    if rgb:
       _rgb_cb = rgb
    cdef DevPtr dev
    cdef CtxPtr ctx
    cdef void* devp
    cdef void* ctxp
    ctx = init()
    dev = open_device(ctx, 0)
    devp = dev._ptr
    ctxp = ctx._ptr
    freenect_set_depth_format(devp, 0)
    freenect_start_depth(devp)
    freenect_set_rgb_format(devp, FORMAT_RGB)
    freenect_start_rgb(devp)
    freenect_set_depth_callback(devp, depth_cb)
    freenect_set_rgb_callback(devp, rgb_cb)
    while freenect_process_events(ctxp) >= 0:
        pass

def _load_numpy():
    try:
        import numpy as np
        return np
    except ImportError, e:
        print('You need the numpy library to use this function')
        raise e


def depth_cb_np(dev, string, timestamp):
   """Converts the raw depth data into a numpy array for your function

    Args:
        dev: DevPtr object
        string: A python string with the depth data
        timestamp: An int representing the time

    Returns:
        (dev, data, timestamp) where data is a 2D numpy array
   """
   np = _load_numpy()
   data = np.fromstring(string, dtype=np.uint16)
   data.resize((480, 640))
   return dev, data, timestamp


def rgb_cb_np(dev, string, timestamp):
   """Converts the raw depth data into a numpy array for your function

    Args:
        dev: DevPtr object
        string: A python string with the RGB data
        timestamp: An int representing the time

    Returns:
        (dev, data, timestamp) where data is a 2D numpy array
   """
   np = _load_numpy()
   data = np.fromstring(string, dtype=np.uint8)
   data.resize((480, 640, 3))
   return dev, data, timestamp
