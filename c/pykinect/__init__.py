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
#/

from ctypes import CDLL, byref, c_void_p, CFUNCTYPE, c_uint8, c_uint16, \
     c_uint32, POINTER, c_int
from array import array

# Taken from libfreenect.h

# The depth value
depth_type = c_uint16
# A pixel R, G, or B value
pixel_type = c_uint8

# Frame dimensions
frame_width = 640
frame_height = 480

# Callback types
RGB_CALLBACK_TYPE = CFUNCTYPE(None, c_void_p, POINTER(pixel_type), c_uint32)
DEPTH_CALLBACK_TYPE = CFUNCTYPE(None, c_void_p, POINTER(depth_type), c_uint32)


# Load the library
libname = "freenect"

try:
    lib = CDLL(libname)
except OSError:
    lib = CDLL("lib%s.so" % libname)


class KinectCore(object):
    """ An object to directly interface with freenect

    Example usage:

    k = KinectCore()
    def rgb_cb(data, timestamp):
        assert len(data) == 640 *480 * 3
        print(timestamp)

    def depth_cb(data, timestamp):
        assert len(data) == 640 *480
        print(timestamp)

    k.rgbcallback = rgb_cb
    k.depthcallback = depth_cb
    while True:
        k.process_events()
    """

    def __init__(self):
        """ Initializes the contet and device objects """
        self.context = c_void_p()
        err = lib.freenect_init(byref(self.context), c_void_p())
        if err < 0:
            raise RuntimeError("freenect_init failed")

        self.dev = c_void_p()
        err = lib.freenect_open_device(self.context, byref(self.dev), 0)
        if err < 0:
            raise IOError("Failed to open device")


    def __setrgb(self, func):
        def wrapper_rgb_func(dev, rgb, ts):
            print "calling rgb wrapper"
            print repr(rgb)
            func(rgb[:frame_width * frame_height * 3], ts)

        c_func = RGB_CALLBACK_TYPE(wrapper_rgb_func)
        self._rgb_cb = c_func
        lib.freenect_set_rgb_callback(self.dev, c_func)
        lib.freenect_set_rgb_format(self.dev, 0)
        lib.freenect_start_rgb(self.dev)

    def __setdepth(self, func):
        def wrapper_depth_func(dev, depths, ts):
            print "calling depth wrapper"
            print repr(depths)
            if depths:
                func(depths[:frame_width * frame_height], ts)

        c_func = DEPTH_CALLBACK_TYPE(wrapper_depth_func)
        self._depth_cb = c_func
        lib.freenect_set_depth_callback(self.dev, c_func)
        lib.freenect_start_depth(self.dev)

    rgb_callback = property(fset=__setrgb,
                            doc="The callback function for RGB data.\n\n" \
                            "It gets two arguments, the array of 8 bit RGB " \
                            "data and a timestamp.")


    depth_callback = property(fset=__setdepth,
                              doc="The callback function for Depth data.\n\n" \
                              "It gets two arguments, the array of 16 bit " \
                              "depths and a timestamp.")

    def process_events(self):
        """ Call in a loop to process events and dispatch callbacks. """
        err = lib.freenect_process_events(self.context)
        if err < 0:
            raise RuntimeError("Error processing events")


if __name__ == "__main__":
    k = KinectCore()

    def rgb_cb(data, timestamp):
        assert len(data) == 640 *480 * 3
        print(timestamp)

    def depth_cb(data, timestamp):
        assert len(data) == 640 *480
        print(timestamp)

    k.rgb_callback = rgb_cb
    k.depth_callback = depth_cb
    while True:
        k.process_events()
