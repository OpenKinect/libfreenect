# cython: language_level=3
#
# This file is part of the OpenKinect Project. http://www.openkinect.org
#
# Copyright (c) 2026 individual OpenKinect contributors. See the CONTRIB file
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

"""The Kinect's microphone array: beamforming and sound source localisation
(libfreenect_audio_dsp.h), with the Kinect SDK's behaviour.

    import freenect_audio
    bf = freenect_audio.Beamformer()
    mono = bf.process(frames)        # frames: (n, 4) int32 or float32, 16 kHz
    angle, confidence = bf.source_angle

Record the 4 microphones through the operating system (with Microsoft's UAC
firmware the Kinect is a standard 4-channel 16 kHz microphone), for example
with the sounddevice package, and feed the frames in any block size. Angles
are in degrees, 0 straight ahead, positive towards microphone 4.
"""

from libc.stdint cimport int16_t, int32_t
import math
import numpy as np

cdef extern from "libfreenect_audio_dsp.h":
    ctypedef struct freenect_dsp
    ctypedef struct freenect_mic_geometry
    enum:
        FREENECT_DSP_RATE
        FREENECT_DSP_FRAME
        FREENECT_DSP_NUM_BEAMS
        FREENECT_DSP_LATENCY
    ctypedef enum freenect_dsp_beam_mode:
        FREENECT_DSP_BEAM_AUTOMATIC
        FREENECT_DSP_BEAM_ADAPTIVE
        FREENECT_DSP_BEAM_MANUAL
    freenect_dsp* freenect_dsp_create(const freenect_mic_geometry* geometry)
    void freenect_dsp_destroy(freenect_dsp* dsp)
    int freenect_dsp_process(freenect_dsp* dsp, const int32_t* frames, int n, int16_t* out) nogil
    int freenect_dsp_process_float(freenect_dsp* dsp, const float* frames, int n, int16_t* out) nogil
    int freenect_dsp_set_beam_mode(freenect_dsp* dsp, freenect_dsp_beam_mode mode)
    int freenect_dsp_set_beam(freenect_dsp* dsp, int beam)
    double freenect_dsp_get_beam_angle(freenect_dsp* dsp)
    int freenect_dsp_get_source_angle(freenect_dsp* dsp, double* angle, double* confidence)
    int freenect_dsp_load_sdk_weights(freenect_dsp* dsp, const char* path)
    int freenect_dsp_using_sdk_weights(freenect_dsp* dsp)
    int freenect_dsp_set_noise_suppression(freenect_dsp* dsp, int on)
    int freenect_dsp_set_agc(freenect_dsp* dsp, int on)
    int freenect_dsp_set_echo_cancellation(freenect_dsp* dsp, int on)
    int freenect_dsp_feed_reference(freenect_dsp* dsp, const int16_t* samples, int n)

RATE = FREENECT_DSP_RATE
FRAME = FREENECT_DSP_FRAME
NUM_BEAMS = FREENECT_DSP_NUM_BEAMS
LATENCY = FREENECT_DSP_LATENCY
BEAM_AUTOMATIC = FREENECT_DSP_BEAM_AUTOMATIC
BEAM_ADAPTIVE = FREENECT_DSP_BEAM_ADAPTIVE
BEAM_MANUAL = FREENECT_DSP_BEAM_MANUAL
BEAM_ANGLES = tuple(-50 + 10 * i for i in range(FREENECT_DSP_NUM_BEAMS))

cdef class Beamformer:
    """libfreenect's beamformer for the Kinect's array.

    Beamformer() follows the talker among 11 fixed beams (-50..50 degrees),
    as the SDK does. mode=BEAM_ADAPTIVE steers continuously; mode=BEAM_MANUAL
    with angle= keeps one beam. sdk_weights=True uses Microsoft's beam
    weights if fwfetcher.py --audio-weights installed them (else
    libfreenect's own, which are as good)."""
    cdef freenect_dsp* _d

    def __cinit__(self, mode=BEAM_AUTOMATIC, angle=None, sdk_weights=False):
        self._d = freenect_dsp_create(NULL)
        if self._d == NULL:
            raise MemoryError()

    def __init__(self, mode=BEAM_AUTOMATIC, angle=None, sdk_weights=False):
        if angle is not None:
            self.set_beam(angle)
        else:
            self.set_mode(mode)
        if sdk_weights:
            self.load_sdk_weights()

    def __dealloc__(self):
        if self._d != NULL:
            freenect_dsp_destroy(self._d)

    def process(self, frames):
        """Beamforms (n, 4) frames: int32 at full scale 2**31 (the USB
        format) or float at full scale 1.0. Returns n int16 samples, mono,
        16 kHz, delayed by LATENCY samples."""
        cdef int32_t[:, ::1] fi
        cdef float[:, ::1] ff
        cdef int16_t[::1] o
        cdef int n, r = 0
        a = np.asarray(frames)
        if a.ndim != 2 or a.shape[1] != 4:
            raise ValueError("frames must be (n, 4): one column per microphone")
        n = a.shape[0]
        out = np.empty(n, dtype=np.int16)
        if n == 0:
            return out
        o = out
        if np.issubdtype(a.dtype, np.integer):
            fi = np.ascontiguousarray(a, dtype=np.int32)
            with nogil:
                r = freenect_dsp_process(self._d, &fi[0, 0], n, &o[0])
        else:
            ff = np.ascontiguousarray(a, dtype=np.float32)
            with nogil:
                r = freenect_dsp_process_float(self._d, &ff[0, 0], n, &o[0])
        if r < 0:
            raise RuntimeError("freenect_dsp_process failed (%d)" % r)
        return out

    def set_mode(self, int mode):
        """BEAM_AUTOMATIC (follow the talker among the fixed beams),
        BEAM_ADAPTIVE (steer continuously) or BEAM_MANUAL (see set_beam)."""
        if freenect_dsp_set_beam_mode(self._d, <freenect_dsp_beam_mode> mode) < 0:
            raise ValueError("mode: BEAM_AUTOMATIC, BEAM_ADAPTIVE or BEAM_MANUAL")

    def set_beam(self, angle):
        """Keep the beam nearest to angle (degrees, -50..50); BEAM_MANUAL."""
        cdef int i = int(round((float(angle) + 50.0) / 10.0))
        if i < 0 or i >= FREENECT_DSP_NUM_BEAMS:
            raise ValueError("angle: -50 to 50 degrees")
        freenect_dsp_set_beam_mode(self._d, FREENECT_DSP_BEAM_MANUAL)
        freenect_dsp_set_beam(self._d, i)

    @property
    def beam_angle(self):
        """The direction the output listens to now, in degrees."""
        return math.degrees(freenect_dsp_get_beam_angle(self._d))

    @property
    def source_angle(self):
        """(angle in degrees, confidence 0..1) of the tracked talker, as the
        SDK's SoundSourceAngle, or None before one is found."""
        cdef double a, c
        if freenect_dsp_get_source_angle(self._d, &a, &c) < 0:
            return None
        return (math.degrees(a), c)

    def load_sdk_weights(self, path=None):
        """Use Microsoft's beam weights (path None: where the fetchers
        install them). Returns True if they are in use."""
        cdef bytes p
        if path is None:
            return freenect_dsp_load_sdk_weights(self._d, NULL) == 0
        p = str(path).encode()
        return freenect_dsp_load_sdk_weights(self._d, p) == 0

    @property
    def using_sdk_weights(self):
        return freenect_dsp_using_sdk_weights(self._d) != 0

    def set_noise_suppression(self, on):
        """Needs libfreenect built with SpeexDSP: False if unavailable."""
        return freenect_dsp_set_noise_suppression(self._d, 1 if on else 0) == 0

    def set_agc(self, on):
        return freenect_dsp_set_agc(self._d, 1 if on else 0) == 0

    def set_echo_cancellation(self, on):
        return freenect_dsp_set_echo_cancellation(self._d, 1 if on else 0) == 0

    def feed_reference(self, samples):
        """For echo cancellation: what the loudspeakers play (int16, mono,
        16 kHz), in step with the microphones; before the matching process()."""
        cdef int16_t[::1] s = np.ascontiguousarray(samples, dtype=np.int16)
        if s.shape[0] == 0:
            return
        freenect_dsp_feed_reference(self._d, &s[0], s.shape[0])
