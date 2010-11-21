#
# Copyright (c) 2010 Brandyn White (bwhite@dappervision.com)
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
import ctypes
import array
import itertools
FREENECT_FORMAT_RGB = 0
FREENECT_FORMAT_BAYER = 1
FREENECT_FORMAT_11_BIT = 0
FREENECT_FORMAT_10_BIT = 1
LED_OFF = 0
LED_GREEN = 1
LED_RED = 2
LED_YELLOW = 3
LED_BLINK_YELLOW = 4
LED_BLINK_GREEN = 5
LED_BLINK_RED_YELLOW = 6


def _try_load(paths, names, extensions):
    out_accum = []
    for x in itertools.product(paths, names, extensions):
        try:
            return ctypes.cdll.LoadLibrary('%s%s%s' % x)
        except OSError, e:
            out_accum.append(e)
    print("Here are all of the things we tried...")
    for x in out_accum:
        print(x)
    raise OSError("Couldn't find shared library, was it built properly?")


def _setup_shared_library():
    """Types all of the shared library functions

    These headers are copied directly from libfreenect.h in order to simplify
    updating

    Returns:
        (fn, freenect_depth_cb, freenect_rgb_cb) where
        fn: freenect dynamic library updated with type info
        freenect_depth_cb: ctypes callback wrapper for depth
        freenect_rgb_cb: ctypes callback wrapper for rgb
    """

    def mk(res, fun, arg):
        fun.restype = res
        fun.argtypes = arg
    from ctypes import c_int, c_void_p, c_uint32, c_double, c_int16, POINTER
    c_int16_p = POINTER(c_int16)
    c_double_p = POINTER(c_double)
    # This shared library could be a few places, lets just try them
    fn = _try_load(['', '../build/lib/', '/usr/local/lib/'],
                   ['libfreenect'],
                   ['.so', '.dylib', '.dll'])
    # int freenect_init(freenect_context **ctx, freenect_usb_context *usb_ctx);
    mk(c_int, fn.freenect_init, [c_void_p, c_void_p])
    # int freenect_shutdown(freenect_context *ctx);
    mk(c_int, fn.freenect_shutdown, [c_void_p])
    # int freenect_process_events(freenect_context *ctx);
    mk(c_int, fn.freenect_process_events, [c_void_p])
    # int freenect_num_devices(freenect_context *ctx);
    mk(c_int, fn.freenect_num_devices, [c_void_p])
    # int freenect_open_device(freenect_context *ctx, freenect_device **dev, int index);
    mk(c_int, fn.freenect_open_device, [c_void_p, c_void_p, c_int])
    # int freenect_close_device(freenect_device *dev);
    mk(c_int, fn.freenect_close_device, [c_void_p])
    # void freenect_set_user(freenect_device *dev, void *user);
    mk(None, fn.freenect_set_user, [c_void_p, c_void_p])
    # void *freenect_get_user(freenect_device *dev);
    mk(c_void_p, fn.freenect_get_user, [c_void_p])
    # typedef void (*freenect_depth_cb)(freenect_device *dev, freenect_depth *depth, uint32_t timestamp);
    freenect_depth_cb = ctypes.CFUNCTYPE(None, c_void_p, c_void_p, c_uint32)
    # void freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb);
    mk(None, fn.freenect_set_depth_callback, [c_void_p, freenect_depth_cb])
    # typedef void (*freenect_rgb_cb)(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp);
    freenect_rgb_cb = ctypes.CFUNCTYPE(None, c_void_p, c_void_p, c_uint32)
    # void freenect_set_rgb_callback(freenect_device *dev, freenect_rgb_cb cb);
    mk(None, fn.freenect_set_rgb_callback, [c_void_p, freenect_rgb_cb])
    # int freenect_set_rgb_format(freenect_device *dev, freenect_rgb_format fmt);
    mk(c_int, fn.freenect_set_rgb_format, [c_void_p, c_int])
    # int freenect_set_depth_format(freenect_device *dev, freenect_depth_format fmt);
    mk(c_int, fn.freenect_set_depth_format, [c_void_p, c_int])
    # int freenect_start_depth(freenect_device *dev);
    mk(c_int, fn.freenect_start_depth, [c_void_p])
    # int freenect_start_rgb(freenect_device *dev);
    mk(c_int, fn.freenect_start_rgb, [c_void_p])
    # int freenect_stop_depth(freenect_device *dev);
    mk(c_int, fn.freenect_stop_depth, [c_void_p])
    # int freenect_stop_rgb(freenect_device *dev);
    mk(c_int, fn.freenect_stop_rgb, [c_void_p])
    # int freenect_set_tilt_degs(freenect_device *dev, double angle);
    mk(c_int, fn.freenect_set_tilt_degs, [c_void_p, c_double])
    # int freenect_set_led(freenect_device *dev, freenect_led_options option);
    mk(c_int, fn.freenect_set_led, [c_void_p, c_int])
    # int freenect_get_raw_accel(freenect_device *dev, int16_t* x, int16_t* y, int16_t* z);
    mk(c_int, fn.freenect_get_raw_accel, [c_void_p, c_int16_p, c_int16_p, c_int16_p])
    # int freenect_get_mks_accel(freenect_device *dev, double* x, double* y, double* z);
    mk(c_int, fn.freenect_get_mks_accel, [c_void_p, c_double_p, c_double_p, c_double_p])
    return fn, freenect_depth_cb, freenect_rgb_cb


# Populate module namespace to mimic C interface
def _populate_namespace():
    _fn, freenect_depth_cb, freenect_rgb_cb = _setup_shared_library()
    for x in dir(_fn):
        if x.startswith('freenect_'):
            globals()[x] = getattr(_fn, x)
    return freenect_depth_cb, freenect_rgb_cb


def raw_accel(dev):
    """Convinience wrapper for raw accelerometers

    Args:
        dev: ctypes dev pointer

    Returns:
        Tuple of (x, y, z) as ctype.c_int16 values
    """
    x, y, z = ctypes.c_int16(), ctypes.c_int16(), ctypes.c_int16()
    freenect_get_raw_accel(dev, ctypes.byref(x), ctypes.byref(y),
                           ctypes.byref(z))
    return x, y, z


def mks_accel(dev):
    """Convinience wrapper for mks accelerometers

    Args:
        dev: ctypes dev pointer

    Returns:
        Tuple of (x, y, z) as ctype.c_double values
    """
    x, y, z = ctypes.c_double(), ctypes.c_double(), ctypes.c_double()
    freenect_get_mks_accel(dev, ctypes.byref(x), ctypes.byref(y),
                           ctypes.byref(z))
    return x, y, z


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
    depth = freenect_depth_cb(depth if depth else lambda *x: None)
    rgb = freenect_rgb_cb(rgb if rgb else lambda *x: None)
    ctx = ctypes.c_void_p()
    if freenect_init(ctypes.byref(ctx), 0) < 0:
        print('Error: Cant open')
    dev = ctypes.c_void_p()
    if freenect_open_device(ctx, ctypes.byref(dev), 0) < 0:
        print('Error: Cant open')
    freenect_set_depth_format(dev, 0)
    freenect_set_depth_callback(dev, depth)
    freenect_start_depth(dev)
    freenect_set_rgb_format(dev, FREENECT_FORMAT_RGB)
    freenect_set_rgb_callback(dev, rgb)
    freenect_start_rgb(dev)
    while freenect_process_events(ctx) >= 0:
        pass


def _load_numpy():
    try:
        import numpy as np
        return np
    except ImportError, e:
        print('You need the numpy library to use this function')
        raise e


def depth_cb_string_factory(func):
    """Converts the raw depth data into a python string for your function

    Args:
        func: A function that takes (dev, data, timestamp), corresponding to C function
            except that data is a python string corresponding to the data.
    """

    def depth_cb(dev, depth, timestamp):
        nbytes = 614400  # 480 * 640 * 2
        func(dev, ctypes.string_at(depth, nbytes), timestamp)
    return depth_cb


def rgb_cb_string_factory(func):
    """Converts the raw RGB data into a python string for your function

    Args:
        func: A function that takes (dev, data, timestamp), corresponding to C function
            except that data is a python string corresponding to the data.
    """

    def rgb_cb(dev, rgb, timestamp):
        nbytes = 921600  # 480 * 640 * 3
        func(dev, ctypes.string_at(rgb, nbytes), timestamp)
    return rgb_cb


def depth_cb_np_factory(func):
    """Converts the raw depth data into a numpy array for your function

    Args:
        func: A function that takes (dev, data, timestamp), corresponding to C function
            except that data is a 2D numpy array corresponding to the data.
    """
    np = _load_numpy()

    def depth_cb(dev, string, timestamp):
        data = np.fromstring(string, dtype=np.uint16)
        data.resize((480, 640))
        func(dev, data, timestamp)
    return depth_cb_string_factory(depth_cb)


def rgb_cb_np_factory(func):
    """Converts the raw RGB data into a numpy array for your function

    Args:
        func: A function that takes (dev, data, timestamp), corresponding to C function
            except that data is a 2D numpy array corresponding to the data.
    """
    np = _load_numpy()

    def rgb_cb(dev, string, timestamp):
        data = np.fromstring(string, dtype=np.uint8)
        data.resize((480, 640, 3))
        func(dev, data, timestamp)
    return rgb_cb_string_factory(rgb_cb)


def depth_cb_array_factory(func):
    """Converts the raw depth data into a 1D python array (not numpy) for your function

    Args:
        func: A function that takes (dev, data, timestamp), corresponding to C function
            except that data is a 1D python array corresponding to the data.
    """

    def depth_cb(dev, string, timestamp):
        data = array.array('H')
        assert(data.itemsize == 2)
        data.fromstring(string)
        func(dev, data, timestamp)
    return depth_cb_string_factory(depth_cb)


def rgb_cb_array_factory(func):
    """Converts the raw RGB data into a 1D python array (not numpy) for your function

    Args:
        func: A function that takes (dev, data, timestamp), corresponding to C function
            except that data is a 1D python array corresponding to the data.
    """

    def rgb_cb(dev, string, timestamp):
        data = array.array('B')
        assert(data.itemsize == 1)
        data.fromstring(string)
        func(dev, data, timestamp)
    return rgb_cb_string_factory(rgb_cb)

freenect_depth_cb, freenect_rgb_cb = _populate_namespace()
