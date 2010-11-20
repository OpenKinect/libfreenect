#
# This file is part of the OpenKinect Project. http://www.openkinect.org
#
# Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
# for details.
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
import numpy as np

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


def _setup_shared_library():
    """Types all of the shared library functions

    These headers are copied directly from libfreenect.h in order to simplify updating

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
    c_double_p = POINTER(c_int16)
    fn = ctypes.cdll.LoadLibrary('./libfreenect.so')
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
    # int freenect_set_tilt_in_degrees(freenect_device *dev, double angle);
    mk(c_int, fn.freenect_set_tilt_in_degrees, [c_void_p, c_double])
    # int freenect_set_tilt_in_radians(freenect_device *dev, double angle);
    mk(c_int, fn.freenect_set_tilt_in_radians, [c_void_p, c_double])
    # int freenect_set_led(freenect_device *dev, freenect_led_options option);
    mk(c_int, fn.freenect_set_led, [c_void_p, c_int])
    # int freenect_get_raw_accelerometers(freenect_device *dev, int16_t* x, int16_t* y, int16_t* z);
    mk(c_int, fn.freenect_get_raw_accelerometers, [c_void_p, c_int16_p, c_int16_p, c_int16_p])
    # int freenect_get_mks_accelerometers(freenect_device *dev, double* x, double* y, double* z);
    mk(c_int, fn.freenect_get_mks_accelerometers, [c_void_p, c_double_p, c_double_p, c_double_p])
    return fn, freenect_depth_cb, freenect_rgb_cb

fn, freenect_depth_cb, freenect_rgb_cb = _setup_shared_library()

def runloop(depth_cb, rgb_cb):
    depth_cb = freenect_depth_cb(depth_cb)
    rgb_cb = freenect_rgb_cb(rgb_cb)
    ctx = ctypes.c_void_p()
    if fn.freenect_init(ctypes.byref(ctx), 0) < 0:
        print('Error: Cant open')
    dev = ctypes.c_void_p()
    if fn.freenect_open_device(ctx, ctypes.byref(dev), 0) < 0:
        print('Error: Cant open')
    fn.freenect_set_depth_format(dev, 0)
    fn.freenect_set_depth_callback(dev, depth_cb)
    fn.freenect_start_depth(dev)
    fn.freenect_set_rgb_format(dev, FREENECT_FORMAT_RGB)
    fn.freenect_set_rgb_callback(dev, rgb_cb)
    fn.freenect_start_rgb(dev)
    while fn.freenect_process_events(ctx) >= 0:
        pass

def depth_cb_factory(func):
    def depth_cb(dev, depth, timestamp):
        size, bytes = (480, 640), 614400  # 480 * 640 * 2
        data = np.fromstring(ctypes.string_at(depth, bytes), dtype=np.uint16)
        data.resize(size)
        func(dev, data, timestamp)
    return depth_cb


def rgb_cb_factory(func):
    def rgb_cb(dev, rgb, timestamp):
        size, bytes = (480, 640, 3), 921600  # 480 * 640 * 3
        data = np.fromstring(ctypes.string_at(rgb, bytes), dtype=np.uint8)
        data.resize(size)
        func(dev, data, timestamp)
    return rgb_cb
