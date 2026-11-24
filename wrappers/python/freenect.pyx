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

from libc.stdint cimport *
import numpy as np
cimport numpy as npc

cdef extern from "numpy/arrayobject.h":
    cdef object PyArray_SimpleNewFromData(int nd, npc.npy_intp *dims,
                                           int typenum, void *data)

cdef extern from "stdlib.h":
    void free(void *ptr)

cdef extern from "libfreenect.h":
    ctypedef enum freenect_video_format:
        FREENECT_VIDEO_RGB
        FREENECT_VIDEO_BAYER
        FREENECT_VIDEO_IR_8BIT
        FREENECT_VIDEO_IR_10BIT
        FREENECT_VIDEO_IR_10BIT_PACKED
        FREENECT_VIDEO_YUV_RGB
        FREENECT_VIDEO_YUV_RAW

    ctypedef enum freenect_depth_format:
        FREENECT_DEPTH_11BIT
        FREENECT_DEPTH_10BIT
        FREENECT_DEPTH_11BIT_PACKED
        FREENECT_DEPTH_10BIT_PACKED
        FREENECT_DEPTH_REGISTERED
        FREENECT_DEPTH_MM

    ctypedef enum freenect_led_options:
        FREENECT_LED_OFF "LED_OFF"
        FREENECT_LED_GREEN "LED_GREEN"
        FREENECT_LED_RED "LED_RED"
        FREENECT_LED_YELLOW "LED_YELLOW"
        FREENECT_LED_BLINK_GREEN "LED_BLINK_GREEN"
        FREENECT_LED_BLINK_RED_YELLOW "LED_BLINK_RED_YELLOW"

    ctypedef enum freenect_resolution:
        FREENECT_RESOLUTION_LOW
        FREENECT_RESOLUTION_MEDIUM
        FREENECT_RESOLUTION_HIGH
        FREENECT_RESOLUTION_VERY_LOW

    ctypedef enum freenect_device_flags:
        FREENECT_DEVICE_MOTOR
        FREENECT_DEVICE_CAMERA
        FREENECT_DEVICE_AUDIO

    ctypedef enum freenect_tilt_status_code:
        TILT_STATUS_STOPPED
        TILT_STATUS_LIMIT
        TILT_STATUS_MOVING

    ctypedef struct freenect_raw_tilt_state:
        int16_t accelerometer_x
        int16_t accelerometer_y
        int16_t accelerometer_z
        int8_t tilt_angle
        freenect_tilt_status_code tilt_status

    ctypedef struct freenect_frame_mode:
        uint32_t reserved
        freenect_resolution resolution
        int32_t video_format # we chose one of video_format/depth_format arbitrarily, since they are in a union
        int32_t bytes
        int16_t width
        int16_t height
        int8_t data_bits_per_pixel
        int8_t padding_bits_per_pixel
        int8_t framerate
        int8_t is_valid

    ctypedef struct freenect_context
    ctypedef struct freenect_device

    ctypedef void (*freenect_depth_cb)(freenect_device *dev, void *depth, uint32_t timestamp)
    ctypedef void (*freenect_video_cb)(freenect_device *dev, void *video, uint32_t timestamp)
    int freenect_init(freenect_context **ctx, void *usb_ctx)
    int freenect_shutdown(freenect_context *ctx)
    int freenect_process_events(freenect_context *ctx) nogil
    int freenect_num_devices(freenect_context *ctx)
    void freenect_select_subdevices(freenect_context *ctx, freenect_device_flags subdevs)
    int freenect_open_device(freenect_context *ctx, freenect_device **dev, int index)
    int freenect_close_device(freenect_device *dev)
    void freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb)
    void freenect_set_video_callback(freenect_device *dev, freenect_video_cb cb)

    int freenect_get_video_mode_count()
    freenect_frame_mode freenect_get_video_mode(int mode_num)
    freenect_frame_mode freenect_get_current_video_mode(void *dev)
    freenect_frame_mode freenect_find_video_mode(int res, int fmt)
    int freenect_set_video_mode(freenect_device *dev, freenect_frame_mode mode)

    int freenect_get_depth_mode_count()
    freenect_frame_mode freenect_get_depth_mode(int mode_num)
    freenect_frame_mode freenect_get_current_depth_mode(void *dev)
    freenect_frame_mode freenect_find_depth_mode(int res, int fmt)
    int freenect_set_depth_mode(freenect_device *dev, freenect_frame_mode mode)

    int freenect_start_depth(freenect_device *dev)
    int freenect_start_video(freenect_device *dev)
    int freenect_stop_depth(freenect_device *dev)
    int freenect_stop_video(freenect_device *dev)
    int freenect_set_tilt_degs(freenect_device *dev, double angle)
    int freenect_set_led(freenect_device *dev, freenect_led_options option)
    int freenect_update_tilt_state(freenect_device *dev)
    freenect_raw_tilt_state* freenect_get_tilt_state(freenect_device *dev)
    void freenect_get_mks_accel(freenect_raw_tilt_state *state, double* x, double* y, double* z)
    double freenect_get_tilt_degs(freenect_raw_tilt_state *state)


cdef extern from "libfreenect_sync.h":
    int freenect_sync_get_video(void **video, uint32_t *timestamp, int index, freenect_video_format fmt) nogil
    int freenect_sync_get_depth(void **depth, uint32_t *timestamp, int index, freenect_depth_format fmt) nogil
    void freenect_sync_stop()


VIDEO_RGB = FREENECT_VIDEO_RGB
VIDEO_BAYER = FREENECT_VIDEO_BAYER
VIDEO_IR_8BIT = FREENECT_VIDEO_IR_8BIT
VIDEO_IR_10BIT = FREENECT_VIDEO_IR_10BIT
VIDEO_IR_10BIT_PACKED = FREENECT_VIDEO_IR_10BIT_PACKED
VIDEO_YUV_RGB = FREENECT_VIDEO_YUV_RGB
VIDEO_YUV_RAW = FREENECT_VIDEO_YUV_RAW
DEPTH_11BIT = FREENECT_DEPTH_11BIT
DEPTH_10BIT = FREENECT_DEPTH_10BIT
DEPTH_11BIT_PACKED = FREENECT_DEPTH_11BIT_PACKED
DEPTH_10BIT_PACKED = FREENECT_DEPTH_10BIT_PACKED
DEPTH_REGISTERED = FREENECT_DEPTH_REGISTERED
DEPTH_MM = FREENECT_DEPTH_MM
LED_OFF = FREENECT_LED_OFF
LED_GREEN = FREENECT_LED_GREEN
LED_RED = FREENECT_LED_RED
LED_YELLOW = FREENECT_LED_YELLOW
LED_BLINK_GREEN = FREENECT_LED_BLINK_GREEN
LED_BLINK_RED_YELLOW = FREENECT_LED_BLINK_RED_YELLOW
RESOLUTION_LOW = FREENECT_RESOLUTION_LOW
RESOLUTION_MEDIUM = FREENECT_RESOLUTION_MEDIUM
RESOLUTION_HIGH = FREENECT_RESOLUTION_HIGH
RESOLUTION_VERY_LOW = FREENECT_RESOLUTION_VERY_LOW
DEVICE_MOTOR = FREENECT_DEVICE_MOTOR
DEVICE_CAMERA = FREENECT_DEVICE_CAMERA
DEVICE_AUDIO = FREENECT_DEVICE_AUDIO

cdef inline str _format_ptr(void *ptr):
    if sizeof(void *) == 4:
        return "0x%08x" % <Py_ssize_t>ptr
    elif sizeof(void *) == 8:
        return "0x%016x" % <Py_ssize_t>ptr
    else:
        raise TypeError("What kind of system are you using?!")

cdef class CtxPtr:
    cdef freenect_context* _ptr
    def __init__(self):
        # Safety: do not allow Python to create instances as they would be NULL
        raise TypeError("Cannot create instances of CtxPtr from Python")

    def __repr__(self):
        return "<Ctx Pointer %s>" % _format_ptr(self._ptr)

cdef class DevPtr:
    cdef freenect_device* _ptr
    cdef CtxPtr ctx
    def __init__(self):
        # Safety: do not allow Python to create instances as they would be NULL
        raise TypeError("Cannot create instances of DevPtr from Python")

    def __repr__(self):
        return "<Dev Pointer %s>" % _format_ptr(self._ptr)

cdef class StatePtr:
    cdef freenect_raw_tilt_state* _ptr
    def __init__(self):
        # Safety: do not allow Python to create instances as they would be NULL
        raise TypeError("Cannot create instances of StatePtr from Python")

    def __repr__(self):
        return "<State Pointer %s>" % _format_ptr(self._ptr)

    def _get_accelx(self):
        return int(self._ptr.accelerometer_x)

    def _get_accely(self):
        return int(self._ptr.accelerometer_y)

    def _get_accelz(self):
        return int(self._ptr.accelerometer_z)

    def _get_tilt_angle(self):
        return int(self._ptr.tilt_angle)

    def _get_tilt_status(self):
        return int(self._ptr.tilt_status)

    accelerometer_x = property(_get_accelx)
    accelerometer_y = property(_get_accely)
    accelerometer_z = property(_get_accelz)
    tilt_angle = property(_get_tilt_angle)
    tilt_status = property(_get_tilt_status)

def set_depth_mode(DevPtr dev, int res, int mode):
    return freenect_set_depth_mode(dev._ptr, freenect_find_depth_mode(res, mode))

def set_video_mode(DevPtr dev, int res, int mode):
    return freenect_set_video_mode(dev._ptr, freenect_find_video_mode(res, mode))

def get_depth_format(DevPtr dev):
    return freenect_get_current_depth_mode(dev._ptr).video_format

def get_video_format(DevPtr dev):
    return freenect_get_current_video_mode(dev._ptr).video_format

def start_depth(DevPtr dev):
    return freenect_start_depth(dev._ptr)

def start_video(DevPtr dev):
    return freenect_start_video(dev._ptr)

def stop_depth(DevPtr dev):
    return freenect_stop_depth(dev._ptr)

def stop_video(DevPtr dev):
    return freenect_stop_video(dev._ptr)

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

def set_led(DevPtr dev, freenect_led_options option):
    return freenect_set_led(dev._ptr, option)

def update_tilt_state(DevPtr dev):
    return freenect_update_tilt_state(dev._ptr)

def get_tilt_state(DevPtr dev):
    cdef freenect_raw_tilt_state* state = freenect_get_tilt_state(dev._ptr)
    cdef StatePtr state_out = StatePtr.__new__(StatePtr)
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


def error_open_device():
    print("Error: Can't open device. 1.) is it plugged in? 2.) Read the README")

cpdef init(int subdevices=FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA):
    """A libfreenect context. subdevices: which parts of the Kinect
    open_device opens (DEVICE_MOTOR | DEVICE_CAMERA by default; add
    DEVICE_AUDIO for get_mic_geometry). None if libfreenect can't start."""
    cdef freenect_context* ctx
    if freenect_init(&ctx, NULL) < 0:
        return
    freenect_select_subdevices(ctx, <freenect_device_flags> subdevices)
    cdef CtxPtr ctx_out = CtxPtr.__new__(CtxPtr)
    ctx_out._ptr = ctx
    return ctx_out

cpdef open_device(CtxPtr ctx, int index):
    cdef freenect_device* dev
    if freenect_open_device(ctx._ptr, &dev, index) < 0:
        return
    cdef DevPtr dev_out = DevPtr.__new__(DevPtr)
    dev_out._ptr = dev
    dev_out.ctx = ctx
    return dev_out

_depth_cb, _video_cb = None, None

cdef void depth_cb(freenect_device *dev, void *data, uint32_t timestamp) noexcept with gil:
    cdef freenect_frame_mode mode = freenect_get_current_depth_mode(dev)
    if not mode.is_valid:
        return
    if not _depth_cb:
        return
    cdef DevPtr dev_out = DevPtr.__new__(DevPtr)
    dev_out._ptr = dev
    pydata = _depth_cb_np(data, &mode)
    _depth_cb(dev_out, pydata, timestamp)

cdef void video_cb(freenect_device *dev, void *data, uint32_t timestamp) noexcept with gil:
    cdef freenect_frame_mode mode = freenect_get_current_video_mode(dev)
    if not mode.is_valid:
        return
    if not _video_cb:
        return
    cdef DevPtr dev_out = DevPtr.__new__(DevPtr)
    dev_out._ptr = dev
    pydata = _video_cb_np(data, &mode)
    _video_cb(dev_out, pydata, timestamp)

def set_depth_callback(DevPtr dev, cb):
    global _depth_cb
    if cb is not None:
        _depth_cb = cb
        freenect_set_depth_callback(dev._ptr, depth_cb)
    else:
        _depth_cb = None
        freenect_set_depth_callback(dev._ptr, NULL)

def set_video_callback(DevPtr dev, cb):
    global _video_cb
    if cb is not None:
        _video_cb = cb
        freenect_set_video_callback(dev._ptr, video_cb)
    else:
        _video_cb = None
        freenect_set_video_callback(dev._ptr, NULL)


class Kill(Exception):
    """This kills the runloop, raise from the body only"""


def runloop(depth=None, video=None, body=None, dev=None):
    """Sets up the kinect and maintains a runloop

    This is where most of the action happens.  You can get the dev pointer from the callback
    and let this function do all of the setup for you.  You may want to use threads to perform
    computation externally as the callbacks should really just be used for copying data.

    Args:
        depth: A function that takes (dev, depth, timestamp), corresponding to C function.
            If None (default), then you won't get a callback for depth.
        video: A function that takes (dev, video, timestamp), corresponding to C function.
            If None (default), then you won't get a callback for video.
        body: A function that takes (dev, ctx) and is called in the body of process_events
        dev: Optional freenect device context. If supplied, this function will use it instead
            of creating and destroying its own..
    """
    global _depth_cb, _video_cb
    if depth:
       _depth_cb = depth
    if video:
       _video_cb = video
    cdef DevPtr mdev
    cdef CtxPtr ctx
    cdef freenect_device* devp
    cdef freenect_context* ctxp
    if dev is None:
        ctx = init()
        if not ctx:
            error_open_device()
            return
        mdev = open_device(ctx, 0)
        if not mdev:
            error_open_device()
            return
        if depth is not None:
            freenect_set_depth_mode(mdev._ptr, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT))
        if video is not None:
            freenect_set_video_mode(mdev._ptr, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB))

    else:
        mdev = dev
    devp = mdev._ptr
    ctxp = mdev.ctx._ptr
    if depth is not None:
        freenect_start_depth(devp)
        freenect_set_depth_callback(devp, depth_cb)
    if video is not None:
        freenect_start_video(devp)
        freenect_set_video_callback(devp, video_cb)
    try:
        while True:
            with nogil:
                if freenect_process_events(ctxp) < 0:
                    break
            if body:
                body(mdev, mdev.ctx)
    except Kill:
        pass
    freenect_stop_depth(devp)
    freenect_stop_video(devp)
    if dev is None:
        freenect_close_device(devp)
        freenect_shutdown(ctxp)

def base_runloop(CtxPtr ctx, body=None):
    """Starts a runloop

    This function can be used instead of runloop() to allow the Python code to
    perform all setup steps independently. This simply calls process_events in
    a loop, optionally calling a body function in between. Raise the Kill
    exception to break out of the runloop.

    Args:
        ctx: Freenect library context
        body: A function that takes (ctx) and is called in the body of process_events
    """
    cdef freenect_context* ctxp
    ctxp = ctx._ptr
    try:
        while True:
            with nogil:
                if freenect_process_events(ctxp) < 0:
                    break
            if body:
                body(ctx)
    except Kill:
        pass

npc.import_array()

cdef object _depth_cb_np(void *data, freenect_frame_mode *mode):
    cdef npc.npy_intp dims[2]

    if mode.video_format in (DEPTH_11BIT, DEPTH_10BIT, DEPTH_REGISTERED, DEPTH_MM):
        dims[0], dims[1] = mode.height, mode.width
        return PyArray_SimpleNewFromData(2, dims, npc.NPY_UINT16, data)
    else:
        return (<char *>data)[:mode.bytes]


cdef _video_cb_np(void *data, freenect_frame_mode *mode):
    cdef npc.npy_intp dims[3]

    if mode.video_format in (VIDEO_RGB, VIDEO_YUV_RGB):
        dims[0], dims[1], dims[2]  = mode.height, mode.width, 3
        return PyArray_SimpleNewFromData(3, dims, npc.NPY_UINT8, data)
    elif mode.video_format == VIDEO_IR_8BIT:
        dims[0], dims[1]  = mode.height, mode.width
        return PyArray_SimpleNewFromData(2, dims, npc.NPY_UINT8, data)
    elif mode.video_format == VIDEO_IR_10BIT:
        dims[0], dims[1]  = mode.height, mode.width
        return PyArray_SimpleNewFromData(2, dims, npc.NPY_UINT16, data)
    else:
        return (<char *>data)[:mode.bytes]

def sync_get_depth(index=0, format=DEPTH_11BIT):
    """Get the next available depth frame from the kinect, as a numpy array.

    Args:
        index: Kinect device index (default: 0)
        format: Depth format (default: DEPTH_11BIT)

    Returns:
        (depth, timestamp) or None on error
        depth: A numpy array, shape:(480,640) dtype:np.uint16
        timestamp: int representing the time
    """
    cdef void* data
    cdef uint32_t timestamp
    cdef npc.npy_intp dims[2]
    cdef int out
    cdef int _index = index
    cdef freenect_depth_format _format = format
    with nogil:
        out = freenect_sync_get_depth(&data, &timestamp, _index, _format)
    if out:
        error_open_device()
        return
    if format in [DEPTH_11BIT, DEPTH_10BIT, DEPTH_MM, DEPTH_REGISTERED]:
        dims[0], dims[1]  = 480, 640
        return PyArray_SimpleNewFromData(2, dims, npc.NPY_UINT16, data), timestamp
    else:
        raise TypeError('Conversion not implemented for type [%d]' % (format))


def sync_get_video(index=0, format=VIDEO_RGB):
    """Get the next available rgb frame from the kinect, as a numpy array.

    Args:
        index: Kinect device index (default: 0)
        format: Depth format (default: VIDEO_RGB)

    Returns:
        (depth, timestamp) or None on error
        depth: A numpy array, shape:(480, 640, 3) dtype:np.uint8
        timestamp: int representing the time
    """
    cdef void* data
    cdef uint32_t timestamp
    cdef npc.npy_intp dims[3]
    cdef int out
    cdef int _index = index
    cdef freenect_video_format _format = format
    with nogil:
        out = freenect_sync_get_video(&data, &timestamp, _index, _format)
    if out:
        error_open_device()
        return
    if format == VIDEO_RGB:
        dims[0], dims[1], dims[2]  = 480, 640, 3
        return PyArray_SimpleNewFromData(3, dims, npc.NPY_UINT8, data), timestamp
    elif format == VIDEO_IR_8BIT:
        dims[0], dims[1]  = 480, 640
        return PyArray_SimpleNewFromData(2, dims, npc.NPY_UINT8, data), timestamp
    elif format == VIDEO_IR_10BIT:
        dims[0], dims[1]  = 480, 640
        return PyArray_SimpleNewFromData(2, dims, npc.NPY_UINT16, data), timestamp
    else:
        raise TypeError('Conversion not implemented for type [%d]' % (format))


def sync_stop():
    """Terminate the synchronous runloop if running, else this is a NOP
    """
    freenect_sync_stop()


# ---------------------------------------------------------------- device

cdef extern from "libfreenect.h":
    ctypedef enum freenect_depth_range:
        FREENECT_DEPTH_RANGE_DEFAULT
        FREENECT_DEPTH_RANGE_NEAR
        FREENECT_DEPTH_RANGE_FULL
    ctypedef enum freenect_color_setting:
        FREENECT_COLOR_AUTO_EXPOSURE
        FREENECT_COLOR_EXPOSURE_TIME
        FREENECT_COLOR_FRAME_INTERVAL
        FREENECT_COLOR_GAIN
        FREENECT_COLOR_BRIGHTNESS
        FREENECT_COLOR_POWER_LINE_FREQUENCY
        FREENECT_COLOR_BACKLIGHT_COMPENSATION
        FREENECT_COLOR_AUTO_WHITE_BALANCE
        FREENECT_COLOR_WHITE_BALANCE
        FREENECT_COLOR_CONTRAST
        FREENECT_COLOR_HUE
        FREENECT_COLOR_SATURATION
        FREENECT_COLOR_GAMMA
        FREENECT_COLOR_SHARPNESS
    int freenect_device_connected(freenect_device *dev)
    const char *freenect_get_device_serial(freenect_device *dev)
    int freenect_set_depth_range(freenect_device *dev, freenect_depth_range range)
    freenect_depth_range freenect_get_depth_range(freenect_device *dev)
    int freenect_set_color_setting(freenect_device *dev, freenect_color_setting setting, double value)
    int freenect_get_color_setting(freenect_device *dev, freenect_color_setting setting, double *value)
    int freenect_nudge_stream_timing(freenect_device *dev, int depth_nudge_us)
    int freenect_set_stream_timing_auto(freenect_device *dev, int on)
    int freenect_set_ir_emitter(freenect_device *dev, int on)

DEPTH_RANGE_DEFAULT = FREENECT_DEPTH_RANGE_DEFAULT
DEPTH_RANGE_NEAR = FREENECT_DEPTH_RANGE_NEAR
DEPTH_RANGE_FULL = FREENECT_DEPTH_RANGE_FULL
COLOR_AUTO_EXPOSURE = FREENECT_COLOR_AUTO_EXPOSURE
COLOR_EXPOSURE_TIME = FREENECT_COLOR_EXPOSURE_TIME
COLOR_FRAME_INTERVAL = FREENECT_COLOR_FRAME_INTERVAL
COLOR_GAIN = FREENECT_COLOR_GAIN
COLOR_BRIGHTNESS = FREENECT_COLOR_BRIGHTNESS
COLOR_POWER_LINE_FREQUENCY = FREENECT_COLOR_POWER_LINE_FREQUENCY
COLOR_BACKLIGHT_COMPENSATION = FREENECT_COLOR_BACKLIGHT_COMPENSATION
COLOR_AUTO_WHITE_BALANCE = FREENECT_COLOR_AUTO_WHITE_BALANCE
COLOR_WHITE_BALANCE = FREENECT_COLOR_WHITE_BALANCE
COLOR_CONTRAST = FREENECT_COLOR_CONTRAST
COLOR_HUE = FREENECT_COLOR_HUE
COLOR_SATURATION = FREENECT_COLOR_SATURATION
COLOR_GAMMA = FREENECT_COLOR_GAMMA
COLOR_SHARPNESS = FREENECT_COLOR_SHARPNESS

def select_subdevices(CtxPtr ctx, int subdevices):
    """Which parts of the Kinect open_device opens (DEVICE_* flags)."""
    freenect_select_subdevices(ctx._ptr, <freenect_device_flags> subdevices)

def device_connected(DevPtr dev):
    """False once the Kinect has been unplugged."""
    return freenect_device_connected(dev._ptr) != 0

def get_device_serial(DevPtr dev):
    """The camera's serial number, or None."""
    cdef const char* s = freenect_get_device_serial(dev._ptr)
    return s.decode('ascii', 'replace') if s != NULL else None

def set_depth_range(DevPtr dev, int depth_range):
    """DEPTH_RANGE_DEFAULT (800-4000 mm), DEPTH_RANGE_NEAR (400-3000 mm) or
    DEPTH_RANGE_FULL (every distance the sensor reports)."""
    return freenect_set_depth_range(dev._ptr, <freenect_depth_range> depth_range)

def get_depth_range(DevPtr dev):
    return int(freenect_get_depth_range(dev._ptr))

def set_ir_emitter(DevPtr dev, on):
    """Switch the IR projector on or off (it is off whenever depth is stopped)."""
    return freenect_set_ir_emitter(dev._ptr, 1 if on else 0)

def set_color_setting(DevPtr dev, int setting, double value):
    """A colour camera setting of the Kinect for Windows (COLOR_*), with the
    SDK's values. Returns 0, or < 0 (-ENOTSUP on the Xbox 360 models)."""
    return freenect_set_color_setting(dev._ptr, <freenect_color_setting> setting, value)

def get_color_setting(DevPtr dev, int setting):
    """The value of a colour camera setting, or None if there is none."""
    cdef double v = 0
    if freenect_get_color_setting(dev._ptr, <freenect_color_setting> setting, &v) < 0:
        return None
    return v

def nudge_stream_timing(DevPtr dev, int depth_nudge_us):
    return freenect_nudge_stream_timing(dev._ptr, depth_nudge_us)

def set_stream_timing_auto(DevPtr dev, on):
    """Keep depth and colour frames in phase automatically."""
    return freenect_set_stream_timing_auto(dev._ptr, 1 if on else 0)


# ---------------------------------------------------------------- microphones

cdef extern from "libfreenect_audio.h":
    enum: FREENECT_MAX_MICS
    ctypedef struct freenect_mic:
        uint16_t type
        int16_t x_mm
        int16_t y_mm
        int16_t z_mm
        int16_t vertical_angle
        int16_t horizontal_angle
    ctypedef struct freenect_mic_geometry:
        uint16_t array_type
        int16_t vertical_angle_begin
        int16_t vertical_angle_end
        int16_t horizontal_angle_begin
        int16_t horizontal_angle_end
        uint16_t freq_low_hz
        uint16_t freq_high_hz
        int num_mics
        freenect_mic mics[4]
    int freenect_get_mic_geometry(freenect_device* dev, freenect_mic_geometry* out)
    int freenect_audio_streaming_available(freenect_device* dev)

def get_mic_geometry(DevPtr dev):
    """The microphone array as the Kinect describes it (needs DEVICE_AUDIO
    in init()): a dict with the working ranges and a list of microphones
    (position in mm, type 2 = cardioid, angles in 1/10000 radian), in the
    order of the audio channels. None if it can't be read."""
    cdef freenect_mic_geometry g
    if freenect_get_mic_geometry(dev._ptr, &g) < 0:
        return None
    mics = []
    for i in range(min(g.num_mics, FREENECT_MAX_MICS)):
        m = g.mics[i]
        mics.append(dict(type=m.type, x_mm=m.x_mm, y_mm=m.y_mm, z_mm=m.z_mm,
                         vertical_angle=m.vertical_angle, horizontal_angle=m.horizontal_angle))
    return dict(array_type=g.array_type, vertical_angle_range=(g.vertical_angle_begin, g.vertical_angle_end),
                horizontal_angle_range=(g.horizontal_angle_begin, g.horizontal_angle_end),
                frequency_range_hz=(g.freq_low_hz, g.freq_high_hz), mics=mics)

def audio_streaming_available(DevPtr dev):
    """Whether libfreenect's own audio streaming works on this device (with
    Microsoft's firmware, record the microphones through the OS instead)."""
    return freenect_audio_streaming_available(dev._ptr) > 0


# ---------------------------------------------------------------- coordinate mapper

cdef extern from "libfreenect_coordinate_mapper.h":
    ctypedef struct freenect_coordinate_mapper
    ctypedef struct freenect_depth_point:
        int32_t x
        int32_t y
        uint16_t depth_mm
    ctypedef struct freenect_color_point:
        int32_t x
        int32_t y
    ctypedef struct freenect_camera_point:
        float x
        float y
        float z
    enum: FREENECT_CALIBRATION_SIZE
    freenect_coordinate_mapper* freenect_create_coordinate_mapper(freenect_device* dev)
    freenect_coordinate_mapper* freenect_create_coordinate_mapper_from_calibration(const void* data, size_t size)
    void freenect_destroy_coordinate_mapper(freenect_coordinate_mapper* m)
    int freenect_mapper_get_calibration(freenect_coordinate_mapper* m, void* buf, size_t size)
    void freenect_mapper_set_mirrored(freenect_coordinate_mapper* m, int mirrored)
    int freenect_map_depth_point_to_color(freenect_coordinate_mapper* m, freenect_resolution depth_res,
                                          freenect_resolution color_res, freenect_depth_point p, freenect_color_point* out)
    int freenect_map_depth_frame_to_color(freenect_coordinate_mapper* m, freenect_resolution depth_res,
                                          freenect_resolution color_res, const uint16_t* depth_mm, freenect_color_point* out)
    int freenect_map_color_frame_to_depth(freenect_coordinate_mapper* m, freenect_resolution depth_res,
                                          freenect_resolution color_res, const uint16_t* depth_mm, freenect_depth_point* out)
    int freenect_map_depth_point_to_camera(freenect_coordinate_mapper* m, freenect_resolution depth_res,
                                           freenect_depth_point p, freenect_camera_point* out)
    int freenect_map_depth_frame_to_camera(freenect_coordinate_mapper* m, freenect_resolution depth_res,
                                           const uint16_t* depth_mm, freenect_camera_point* out)
    int freenect_map_camera_point_to_depth(freenect_coordinate_mapper* m, freenect_resolution depth_res,
                                           freenect_camera_point p, freenect_depth_point* out)
    int freenect_map_camera_point_to_color(freenect_coordinate_mapper* m, freenect_resolution color_res,
                                           freenect_camera_point p, freenect_color_point* out)
    int freenect_map_color_frame_to_camera(freenect_coordinate_mapper* m, freenect_resolution depth_res,
                                           freenect_resolution color_res, const uint16_t* depth_mm, freenect_camera_point* out)

MAPPER_INVALID = -2147483648
_DEPTH_SIZES = {FREENECT_RESOLUTION_VERY_LOW: (60, 80), FREENECT_RESOLUTION_LOW: (240, 320),
                FREENECT_RESOLUTION_MEDIUM: (480, 640)}
_COLOR_SIZES = {FREENECT_RESOLUTION_MEDIUM: (480, 640), FREENECT_RESOLUTION_HIGH: (960, 1280)}
_DEPTH_POINT = np.dtype([('x', '<i4'), ('y', '<i4'), ('depth_mm', '<u2')], align=True)

def _depth_frame(depth, int res):
    if res not in _DEPTH_SIZES:
        raise ValueError("depth resolution: RESOLUTION_VERY_LOW, _LOW or _MEDIUM")
    a = np.ascontiguousarray(depth, dtype=np.uint16)
    if a.shape != _DEPTH_SIZES[res]:
        raise ValueError("depth frame must be %dx%d (rows x columns) for this resolution, not %s"
                         % (_DEPTH_SIZES[res][0], _DEPTH_SIZES[res][1], a.shape))
    return a

def _check(int r, what):
    if r < 0:
        raise RuntimeError("%s failed (%d)" % (what, r))

cdef class CoordinateMapper:
    """Maps between depth pixels, colour pixels and camera space (metres), as
    the Kinect SDK's INuiCoordinateMapper, with the Kinect's own calibration.

    CoordinateMapper(dev) reads it from an open Kinect (with DEVICE_CAMERA);
    CoordinateMapper.from_calibration(data) uses one saved with .calibration.
    Depth frames are uint16 arrays in mm (DEPTH_MM), in libfreenect's
    orientation. Colour resolution RESOLUTION_HIGH is the SDK's 1280x960."""
    cdef freenect_coordinate_mapper* _m

    def __cinit__(self, DevPtr dev=None):
        self._m = NULL
        if dev is not None:
            self._m = freenect_create_coordinate_mapper(dev._ptr)
            if self._m == NULL:
                raise RuntimeError("could not read the Kinect's calibration")

    def __dealloc__(self):
        if self._m != NULL:
            freenect_destroy_coordinate_mapper(self._m)

    @staticmethod
    def from_calibration(data):
        """A mapper from a calibration saved with .calibration."""
        cdef bytes b = bytes(data)
        cdef CoordinateMapper m = CoordinateMapper.__new__(CoordinateMapper)
        m._m = freenect_create_coordinate_mapper_from_calibration(<const char*> b, len(b))
        if m._m == NULL:
            raise ValueError("not a libfreenect calibration (%d bytes)" % len(b))
        return m

    cdef int _ok(self) except -1:
        if self._m == NULL:
            raise RuntimeError("no calibration: use CoordinateMapper(dev) or CoordinateMapper.from_calibration()")
        return 0

    @property
    def calibration(self):
        """The calibration as bytes, to save and load with from_calibration."""
        cdef char buf[512]
        self._ok()
        _check(freenect_mapper_get_calibration(self._m, buf, sizeof(buf)), "get_calibration")
        return buf[:FREENECT_CALIBRATION_SIZE]

    def set_mirrored(self, mirrored):
        """Use the SDK's mirrored image orientation (x' = width - 1 - x) for all pixels."""
        self._ok()
        freenect_mapper_set_mirrored(self._m, 1 if mirrored else 0)

    def depth_point_to_color(self, int x, int y, int depth_mm, int depth_res=FREENECT_RESOLUTION_MEDIUM,
                             int color_res=FREENECT_RESOLUTION_MEDIUM):
        """The colour pixel (x, y) that sees a depth pixel, or None."""
        cdef freenect_depth_point p
        cdef freenect_color_point c
        self._ok()
        p.x = x
        p.y = y
        p.depth_mm = depth_mm
        if freenect_map_depth_point_to_color(self._m, <freenect_resolution> depth_res, <freenect_resolution> color_res, p, &c) < 0:
            return None
        if c.x == MAPPER_INVALID:
            return None
        return (c.x, c.y)

    def depth_frame_to_color(self, depth, int depth_res=FREENECT_RESOLUTION_MEDIUM, int color_res=FREENECT_RESOLUTION_MEDIUM):
        """The colour pixel of every depth pixel: int32 array (rows, columns, 2)
        of (x, y), MAPPER_INVALID where there is no depth."""
        self._ok()
        a = _depth_frame(depth, depth_res)
        out = np.empty(a.shape + (2,), dtype=np.int32)
        cdef const uint16_t[:, ::1] d = a
        cdef int32_t[:, :, ::1] o = out
        _check(freenect_map_depth_frame_to_color(self._m, <freenect_resolution> depth_res, <freenect_resolution> color_res,
                                                 &d[0, 0], <freenect_color_point*> &o[0, 0, 0]), "depth_frame_to_color")
        return out

    def color_frame_to_depth(self, depth, int depth_res=FREENECT_RESOLUTION_MEDIUM, int color_res=FREENECT_RESOLUTION_MEDIUM):
        """The depth pixel seen by every colour pixel: (xy, depth_mm), an int32
        array (rows, columns, 2) and a uint16 array (rows, columns) at the
        colour resolution; MAPPER_INVALID and 0 where no depth pixel lands."""
        self._ok()
        if color_res not in _COLOR_SIZES:
            raise ValueError("colour resolution: RESOLUTION_MEDIUM or RESOLUTION_HIGH")
        a = _depth_frame(depth, depth_res)
        out = np.empty(_COLOR_SIZES[color_res], dtype=_DEPTH_POINT)
        cdef const uint16_t[:, ::1] d = a
        cdef npc.ndarray arr = out
        _check(freenect_map_color_frame_to_depth(self._m, <freenect_resolution> depth_res, <freenect_resolution> color_res,
                                                 &d[0, 0], <freenect_depth_point*> npc.PyArray_DATA(arr)), "color_frame_to_depth")
        return np.stack([out['x'], out['y']], axis=-1), np.ascontiguousarray(out['depth_mm'])

    def depth_point_to_camera(self, int x, int y, int depth_mm, int depth_res=FREENECT_RESOLUTION_MEDIUM):
        """The camera space point (x, y, z) in metres of a depth pixel."""
        cdef freenect_depth_point p
        cdef freenect_camera_point c
        self._ok()
        p.x = x
        p.y = y
        p.depth_mm = depth_mm
        _check(freenect_map_depth_point_to_camera(self._m, <freenect_resolution> depth_res, p, &c), "depth_point_to_camera")
        return (c.x, c.y, c.z)

    def depth_frame_to_camera(self, depth, int depth_res=FREENECT_RESOLUTION_MEDIUM):
        """Camera space points of a whole depth frame: float32 (rows, columns, 3), in metres."""
        self._ok()
        a = _depth_frame(depth, depth_res)
        out = np.empty(a.shape + (3,), dtype=np.float32)
        cdef const uint16_t[:, ::1] d = a
        cdef float[:, :, ::1] o = out
        _check(freenect_map_depth_frame_to_camera(self._m, <freenect_resolution> depth_res, &d[0, 0],
                                                  <freenect_camera_point*> &o[0, 0, 0]), "depth_frame_to_camera")
        return out

    def camera_point_to_depth(self, float x, float y, float z, int depth_res=FREENECT_RESOLUTION_MEDIUM):
        """The depth pixel (x, y, depth_mm) a camera space point projects to, or None."""
        cdef freenect_camera_point p
        cdef freenect_depth_point o
        self._ok()
        p.x = x
        p.y = y
        p.z = z
        if freenect_map_camera_point_to_depth(self._m, <freenect_resolution> depth_res, p, &o) < 0:
            return None
        if o.x == MAPPER_INVALID:
            return None
        return (o.x, o.y, o.depth_mm)

    def camera_point_to_color(self, float x, float y, float z, int color_res=FREENECT_RESOLUTION_MEDIUM):
        """The colour pixel (x, y) that sees a camera space point, or None."""
        cdef freenect_camera_point p
        cdef freenect_color_point o
        self._ok()
        p.x = x
        p.y = y
        p.z = z
        if freenect_map_camera_point_to_color(self._m, <freenect_resolution> color_res, p, &o) < 0:
            return None
        if o.x == MAPPER_INVALID:
            return None
        return (o.x, o.y)

    def color_frame_to_camera(self, depth, int depth_res=FREENECT_RESOLUTION_MEDIUM, int color_res=FREENECT_RESOLUTION_MEDIUM):
        """The camera space point seen by every colour pixel: float32 (rows, columns, 3)."""
        self._ok()
        if color_res not in _COLOR_SIZES:
            raise ValueError("colour resolution: RESOLUTION_MEDIUM or RESOLUTION_HIGH")
        a = _depth_frame(depth, depth_res)
        out = np.empty(_COLOR_SIZES[color_res] + (3,), dtype=np.float32)
        cdef const uint16_t[:, ::1] d = a
        cdef float[:, :, ::1] o = out
        _check(freenect_map_color_frame_to_camera(self._m, <freenect_resolution> depth_res, <freenect_resolution> color_res,
                                                  &d[0, 0], <freenect_camera_point*> &o[0, 0, 0]), "color_frame_to_camera")
        return out
