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

DEPTH_BYTES = 614400 # 480 * 640 * 2
RGB_BYTES = 921600  # 480 * 640 * 3


cdef extern from "stdlib.h":
    void free(void *ptr)


cdef extern from "libfreenect_sync.h":
    int freenect_sync_get_rgb(char **rgb, unsigned int *timestamp) # NOTE: These were uint32_t
    int freenect_sync_get_depth(char **depth, unsigned int *timestamp)
    void freenect_sync_stop()


cdef extern from "Python.h":
    object PyString_FromStringAndSize(char *s, Py_ssize_t len)


def get_depth():
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


def get_rgb():
    """Get the next available rgb frame from the kinect.

    Returns:
        (rgb, timestamp) or None on error
        rgb: A python string for the 8 bit rgb image (640*480*3 bytes)
        timestamp: int representing the time
    """
    cdef char* rgb
    cdef unsigned int timestamp
    out = freenect_sync_get_depth(&rgb, &timestamp)
    if out:
        return
    rgb_str = PyString_FromStringAndSize(rgb, RGB_BYTES)
    free(rgb);
    return rgb_str, timestamp


def stop():
    """Terminate the synchronous runloop if running, else this is a NOP
    """
    freenect_sync_stop()


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
        (rgb, timestamp) or None on error
        rgb: A numpy array, shape:(640,480,3) dtype:np.uint8
        timestamp: int representing the time        
    """
    np = _load_numpy()
    try:
        string, timestamp = get_rgb()
    except TypeError:
        return
    data = np.fromstring(string, dtype=np.uint8)
    data.resize((480, 640, 3))
    return data, timestamp


def get_depth_np():
    """Get the next available depth frame from the kinect, as a numpy array.

    Returns:
        (depth, timestamp) or None on error
        depth: A numpy array, shape:(640,480) dtype:np.uint16
        timestamp: int representing the time

    """
    np = _load_numpy()
    try:
        string, timestamp = get_depth()
    except TypeError:
        return
    data = np.fromstring(string, dtype=np.uint16)
    data.resize((480, 640))
    return data, timestamp
