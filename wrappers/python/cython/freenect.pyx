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

cdef extern from "stdlib.h":
    void memcpy(void *s1, void *s2, int n) nogil

cdef extern from "pthread.h":
    ctypedef struct pthread_mutex_t:
        pass
    
    ctypedef struct pthread_cond_t:
        pass
    
    int pthread_mutex_init(pthread_mutex_t *, void *)
    int pthread_mutex_lock(pthread_mutex_t *) nogil
    int pthread_mutex_unlock(pthread_mutex_t *) nogil
    
    int pthread_cond_init(pthread_cond_t *, void *)
    int pthread_cond_destroy(pthread_cond_t *)
    int pthread_cond_signal(pthread_cond_t *) nogil
    int pthread_cond_wait(pthread_cond_t *, pthread_mutex_t *) nogil

cdef extern from "Python.h":
    object PyString_FromStringAndSize(char *s, Py_ssize_t len)

cdef extern from "libfreenect.h":
    ctypedef void (*freenect_depth_cb)(void *dev, char *depth, int timestamp) # was u_int32
    ctypedef void (*freenect_rgb_cb)(void *dev, char *rgb, int timestamp) # was u_int32
    int freenect_init(void **ctx, int usb_ctx) # changed from void * as usb_ctx is always NULL
    int freenect_shutdown(void *ctx)
    int freenect_process_events(void *ctx) nogil
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

cdef depth_bytes = 614400 # 480 * 640 * 2
cdef rgb_bytes = 921600  # 480 * 640 * 3
cdef char depth_im[614400] 
cdef char rgb_im[921600]

def get_depth():
    """Get the next available depth frame from the kinect.

    Returns:
      A python string for the 16 bit depth image (640*480*2 bytes)
    """
    with nogil:
        pthread_mutex_lock(&depth_mutex)
        pthread_cond_wait(&depth_cond, &depth_mutex)
    string = PyString_FromStringAndSize(depth_im, depth_bytes)
    pthread_mutex_unlock(&depth_mutex)
    return string
    
def get_rgb():
    """Get the next available RGB frame from the kinect.

    Returns:
      A python string for the 8 bit 3 channel depth image (640*480*3 bytes)
    """
    with nogil:
        pthread_mutex_lock(&rgb_mutex)
        pthread_cond_wait(&rgb_cond, &rgb_mutex)
    string = PyString_FromStringAndSize(rgb_im, rgb_bytes)
    pthread_mutex_unlock(&rgb_mutex)
    return string


cdef void depth_cb(void *dev, char *data, int timestamp) nogil:
    global depth_im
    pthread_mutex_lock(&depth_mutex)
    pthread_cond_signal(&depth_cond)
    memcpy(depth_im, data, depth_bytes)
    pthread_mutex_unlock(&depth_mutex)

cdef void rgb_cb(void *dev, char *data, int timestamp) nogil:
    global rgb_im
    pthread_mutex_lock(&rgb_mutex)
    pthread_cond_signal(&rgb_cond)
    memcpy(rgb_im, data, rgb_bytes)
    pthread_mutex_unlock(&rgb_mutex)

cdef pthread_mutex_t rgb_mutex
cdef pthread_mutex_t depth_mutex
cdef pthread_cond_t rgb_cond
cdef pthread_cond_t depth_cond

from threading import Thread
runloop_thread = Thread(target=runloop)

def is_running():
    """Check if the kinect background processing is still running
  
    Returns:
      True if the background thread is alive
    """
    return runloop_thread.is_alive()

def start():
    """Sets up the kinect and runs a background thread. Has no effect if start() has already been called.

    """
    if not is_running(): runloop_thread.start()

def runloop():
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
    pthread_mutex_init(&rgb_mutex, NULL)
    pthread_mutex_init(&depth_mutex, NULL)
    pthread_cond_init(&rgb_cond, NULL)
    pthread_cond_init(&depth_cond, NULL)
    with nogil:
      while freenect_process_events(ctxp) >= 0:
          pass

def _load_numpy():
    try:
        import numpy as np
        return np
    except ImportError, e:
        print('You need the numpy library to use this function')
        raise e
        
def get_rgb_np():
    """Get the next available RGB frame from the kinect, as a numpy array.

    Returns:
      A numpy array, shape:(640,480,3) dtype:np.uint8
    """
    np = _load_numpy()
    string = get_rgb()
    data = np.fromstring(string, dtype=np.uint8)
    data.resize((480, 640, 3))
    return data

def get_depth_np():
    """Get the next available depth frame from the kinect, as a numpy array.

    Returns:
      A numpy array, shape:(640,480) dtype:np.uint16
    """
    np = _load_numpy()
    string = get_depth()
    data = np.fromstring(string, dtype=np.uint16)
    data.resize((480, 640))
    return data

