#!/usr/bin/env python
#
# Copyright (c) 2010 Brandyn White (bwhite@dappervision.com)
#                    Andrew Miller (amiller@dappervision.com)
#
# This code is licensed to you under the terms of the Apache License, version
# 2.0, or, at your option, the terms of the GNU General Public License,
# version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
# or the following URLs:
# http://www.apache.org/licenses/LICENSE-2.0
# http://www.gnu.org/licenses/gpl-2.0.txt
#
# If you redistribute this file in source form, modified or unmodified, you
# may:
#   1) Leave this header intact and distribute it under the same terms,
#      accompanying it with the APACHE20 and GPL20 files, or
#   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
#   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
# In all cases you must keep the copyright notice intact and include a copy
# of the CONTRIB file.
#
# Binary distributions must follow the binary distribution requirements of
# either License.
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

DEPTH_BYTES = 614400 # 480 * 640 * 2
RGB_BYTES = 921600  # 480 * 640 * 3

cdef extern from "stdlib.h":
    void free(void *ptr)

cdef extern from "Python.h":
    object PyString_FromStringAndSize(char *s, Py_ssize_t len)

cdef extern from "libfreenect_sync.h":
    int freenect_sync_get_rgb(char **rgb, unsigned int *timestamp) # NOTE: These were uint32_t
    int freenect_sync_get_depth(char **depth, unsigned int *timestamp)
    void freenect_sync_stop()

cdef extern from "libfreenect.h":
    ctypedef void (*freenect_depth_cb)(void *dev, char *depth, int timestamp) # was u_int32
    ctypedef void (*freenect_rgb_cb)(void *dev, char *rgb, int timestamp) # was u_int32
    int freenect_init(void **ctx, int usb_ctx) # changed from void * as usb_ctx is always NULL
    int freenect_shutdown(void *ctx)
    int freenect_process_events(void *ctx)
    int freenect_num_devices(void *ctx)
    int freenect_open_device(void *ctx, void **dev, int index)
    int freenect_close_device(void *dev)
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
    int freenect_update_tilt_state(void *dev)
    void* freenect_get_tilt_state(void *dev)
    void freenect_get_mks_accel(void *state, double* x, double* y, double* z)
    double freenect_get_tilt_degs(void *state)


cdef class DevPtr:
   cdef void* _ptr 
   def __repr__(self): 
      return "<Dev Pointer>"

cdef class CtxPtr:
   cdef void* _ptr 
   def __repr__(self): 
      return "<Ctx Pointer>"

cdef class StatePtr:
   cdef void* _ptr 
   def __repr__(self): 
      return "<State Pointer>"

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

def update_tilt_state(DevPtr dev):
    return freenect_update_tilt_state(dev._ptr)

def get_tilt_state(DevPtr dev):
    cdef void* state = freenect_get_tilt_state(dev._ptr)
    cdef StatePtr state_out
    state_out = StatePtr()
    state_out._ptr = state
    return state_out

def get_mks_accel(StatePtr state):
   cdef double x, y, z
   freenect_get_mks_accel(state._ptr, &x, &y, &z)
   return x, y, z

def get_accel(DevPtr dev):
   """MKS Accelerometer helper

   Args:
       dev:

   Returns:
       (x, y, z) accelerometer values
   """
   update_tilt_state(dev)
   return get_mks_accel(get_tilt_state(dev))
   

def get_tilt_degs(StatePtr state):
   return freenect_get_tilt_degs(state._ptr)

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
    if _depth_cb:
       _depth_cb(*_depth_cb_np(dev_out, PyString_FromStringAndSize(data, nbytes), timestamp))


cdef void rgb_cb(void *dev, char *data, int timestamp):
    nbytes = 921600  # 480 * 640 * 3
    cdef DevPtr dev_out
    dev_out = DevPtr()
    dev_out._ptr = dev
    if _rgb_cb:
       _rgb_cb(*_rgb_cb_np(dev_out, PyString_FromStringAndSize(data, nbytes), timestamp))


class Kill(Exception):
   """This kills the runloop, raise from the body only"""


def runloop(depth=None, rgb=None, body=None):
    """Sets up the kinect and maintains a runloop

    This is where most of the action happens.  You can get the dev pointer from the callback
    and let this function do all of the setup for you.  You may want to use threads to perform
    computation externally as the callbacks should really just be used for copying data.

    Args:
        depth: A function that takes (dev, depth, timestamp), corresponding to C function.
            If None (default), then you won't get a callback for depth.
        rgb: A function that takes (dev, rgb, timestamp), corresponding to C function.
            If None (default), then you won't get a callback for rgb.
        body: A function that takes (dev, ctx) and is called in the body of process_events
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
    try:
       while freenect_process_events(ctxp) >= 0:
          if body:
             body(dev, ctx)
    except Kill:
       pass
    freenect_stop_depth(devp)
    freenect_stop_rgb(devp)
    freenect_close_device(devp)
    freenect_shutdown(ctxp)


def _load_numpy():
    try:
        import numpy as np
        return np
    except ImportError, e:
        print('You need the numpy library to use this function')
        raise e


def _depth_cb_np(dev, string, timestamp):
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


def _rgb_cb_np(dev, string, timestamp):
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


def _sync_get_depth_str():
    """Get the next available depth frame from the kinect.

    Returns:
        (depth, timestamp) or None on error
        depth: A python string for the 16 bit depth image (640*480*2 bytes)
        timestamp: int representing the time
    """
    cdef char* depth
    cdef unsigned int timestamp
    out = freenect_sync_get_depth(&depth, &timestamp)
    if out:
        return
    depth_str = PyString_FromStringAndSize(depth, DEPTH_BYTES)
    free(depth);
    return depth_str, timestamp


def _sync_get_rgb_str():
    """Get the next available rgb frame from the kinect.

    Returns:
        (rgb, timestamp) or None on error
        rgb: A python string for the 8 bit rgb image (640*480*3 bytes)
        timestamp: int representing the time
    """
    cdef char* rgb
    cdef unsigned int timestamp
    out = freenect_sync_get_rgb(&rgb, &timestamp)
    if out:
        return
    rgb_str = PyString_FromStringAndSize(rgb, RGB_BYTES)
    free(rgb);
    return rgb_str, timestamp


def sync_stop():
    """Terminate the synchronous runloop if running, else this is a NOP
    """
    freenect_sync_stop()

        
def sync_get_rgb():
    """Get the next available RGB frame from the kinect, as a numpy array.

    Returns:
        (rgb, timestamp) or None on error
        rgb: A numpy array, shape:(640,480,3) dtype:np.uint8
        timestamp: int representing the time        
    """
    np = _load_numpy()
    try:
        string, timestamp = _sync_get_rgb_str()
    except TypeError:
        return
    data = np.fromstring(string, dtype=np.uint8)
    data.resize((480, 640, 3))
    return data, timestamp


def sync_get_depth():
    """Get the next available depth frame from the kinect, as a numpy array.

    Returns:
        (depth, timestamp) or None on error
        depth: A numpy array, shape:(640,480) dtype:np.uint16
        timestamp: int representing the time
    """
    np = _load_numpy()
    try:
        string, timestamp = _sync_get_depth_str()
    except TypeError:
        return
    data = np.fromstring(string, dtype=np.uint16)
    data.resize((480, 640))
    return data, timestamp

