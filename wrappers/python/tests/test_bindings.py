"""Tests of the Python bindings that need no Kinect.

    python -m unittest discover -s wrappers/python/tests -v

freenect_nui's tracking tests run when Microsoft's model is installed (or
$LIBFREENECT_NUI_MODELS points to it); they are skipped otherwise.
"""
import math
import os
import random
import struct
import unittest

import numpy as np

import freenect
import freenect_audio

try:
    import freenect_nui
except ImportError:  # built without the skeleton tracker
    freenect_nui = None


def calibration_blob(zpi=(7.5, 2.4, 120.0, 0.1042), reg=(0,) * 29, const_shift=800):
    """A calibration in libfreenect's format ("FNC1"), as test_coordinate_mapper.c builds it."""
    b = bytearray(152)
    b[0:4] = b"FNC1"
    b[4] = 1
    for i, v in enumerate(reg):
        b[8 + 4 * i:12 + 4 * i] = struct.pack("<i", v)
    for i, v in enumerate(zpi):
        b[132 + 4 * i:136 + 4 * i] = struct.pack("<f", v)
    b[148:150] = struct.pack("<H", const_shift)
    return bytes(b)


class Core(unittest.TestCase):
    def test_constants(self):
        self.assertEqual(freenect.DEPTH_MM, 5)
        self.assertEqual(freenect.RESOLUTION_VERY_LOW, 3)
        self.assertEqual(freenect.DEPTH_RANGE_FULL, 2)
        self.assertEqual(freenect.COLOR_SHARPNESS, 13)

    def test_context_without_kinect(self):
        ctx = freenect.init()
        if ctx is None:
            self.skipTest("libfreenect could not start (no USB access?)")
        self.assertGreaterEqual(freenect.num_devices(ctx), 0)
        freenect.shutdown(ctx)


class Mapper(unittest.TestCase):
    def setUp(self):
        self.m = freenect.CoordinateMapper.from_calibration(calibration_blob())

    def test_reference_points(self):
        # from the reference implementation (test/mapper_vectors.h, set 0),
        # which works in the SDK's mirrored orientation
        self.m.set_mirrored(True)
        self.assertEqual(self.m.depth_point_to_color(0, 0, 800), (5, 1))
        self.assertEqual(self.m.depth_point_to_color(639, 479, 4000), (633, 480))
        self.assertEqual(self.m.depth_point_to_color(320, 240, 2000), (315, 241))
        self.assertIsNone(self.m.depth_point_to_color(320, 240, 0))

    def test_calibration_round_trip(self):
        blob = self.m.calibration
        self.assertEqual(len(blob), 152)
        m2 = freenect.CoordinateMapper.from_calibration(blob)
        self.assertEqual(m2.depth_point_to_color(320, 240, 2000), self.m.depth_point_to_color(320, 240, 2000))
        with self.assertRaises(ValueError):
            freenect.CoordinateMapper.from_calibration(b"nonsense")

    def test_frames(self):
        depth = np.full((480, 640), 2000, dtype=np.uint16)
        depth[0, 0] = 0
        c = self.m.depth_frame_to_color(depth)
        self.assertEqual(c.shape, (480, 640, 2))
        self.assertEqual(tuple(int(v) for v in c[240, 320]), self.m.depth_point_to_color(320, 240, 2000))
        self.assertEqual(tuple(int(v) for v in c[10, 600]), self.m.depth_point_to_color(600, 10, 2000))
        self.assertEqual(c[0, 0, 0], freenect.MAPPER_INVALID)
        xyz = self.m.depth_frame_to_camera(depth)
        self.assertEqual(xyz.shape, (480, 640, 3))
        self.assertAlmostEqual(float(xyz[240, 320, 2]), 2.0, places=3)
        # camera space and back
        p = self.m.depth_point_to_camera(100, 50, 1500)
        back = self.m.camera_point_to_depth(*p)
        self.assertEqual(back[:2], (100, 50))
        self.assertLess(abs(back[2] - 1500), 3)
        xy, dmm = self.m.color_frame_to_depth(depth)
        self.assertEqual(xy.shape, (480, 640, 2))
        self.assertEqual(dmm.shape, (480, 640))
        self.assertGreater(int((dmm == 2000).sum()), 0.9 * 480 * 640)
        with self.assertRaises(ValueError):
            self.m.depth_frame_to_color(np.zeros((10, 10), np.uint16))


def talker(angle_deg, seconds=4.0, fs=16000):
    """4-channel frames of band-limited noise arriving from angle_deg
    (positive towards microphone 4), float, full scale 1."""
    rng = np.random.default_rng(1)
    n = int(seconds * fs)
    w = rng.standard_normal(n + 64)
    spec = np.fft.rfft(w)
    f = np.fft.rfftfreq(len(w), 1 / fs)
    spec[(f < 300) | (f > 3000)] = 0
    xs = np.array([-0.113, 0.036, 0.076, 0.113])
    out = np.empty((n, 4), np.float32)
    for k, x in enumerate(xs):
        # a source towards +x reaches microphone 4 (x = +113 mm) first
        delay = -x * math.sin(math.radians(angle_deg)) / 343.0
        shifted = np.fft.irfft(spec * np.exp(-2j * math.pi * f * delay), len(w))
        out[:, k] = shifted[32:32 + n]
    return out / np.abs(out).max() * 0.5


class Audio(unittest.TestCase):
    def test_shapes_and_types(self):
        bf = freenect_audio.Beamformer()
        self.assertEqual(bf.process(np.zeros((0, 4), np.float32)).shape, (0,))
        out = bf.process(np.zeros((1000, 4), np.int32))
        self.assertEqual((out.shape, out.dtype), ((1000,), np.int16))
        with self.assertRaises(ValueError):
            bf.process(np.zeros((10, 3)))
        self.assertEqual(len(freenect_audio.BEAM_ANGLES), freenect_audio.NUM_BEAMS)

    def test_finds_the_talker(self):
        frames = talker(30)
        bf = freenect_audio.Beamformer()
        for k in range(0, len(frames), 1000):  # any block size
            bf.process(frames[k:k + 1000])
        angle, conf = bf.source_angle
        self.assertLess(abs(angle - 30), 10, "source at %.1f degrees" % angle)
        self.assertGreater(conf, 0.5)
        self.assertLess(abs(bf.beam_angle - 30), 10.1)

    def test_beam_favours_the_talker(self):
        frames = talker(-30)
        levels = {}
        for a in (-30, 30):
            out = freenect_audio.Beamformer(angle=a).process(frames)[16000:]
            levels[a] = float(np.sqrt(np.mean(out.astype(np.float64) ** 2)))
        self.assertGreater(levels[-30], 2 * levels[30], levels)


@unittest.skipIf(freenect_nui is None, "built without freenect_nui")
class Nui(unittest.TestCase):
    def test_joint_names(self):
        self.assertEqual(len(freenect_nui.JOINT_NAMES), freenect_nui.JOINTS)
        self.assertEqual(freenect_nui.HEAD, 3)
        self.assertEqual(freenect_nui.THUMB_RIGHT, 27)

    def test_tracker(self):
        try:
            t = freenect_nui.Tracker()
        except FileNotFoundError:
            self.skipTest("the skeleton model is not installed")
        frame = t.process(np.zeros((480, 640), np.uint16))
        self.assertEqual(len(frame.users), freenect_nui.MAX_USERS)
        self.assertEqual(frame.tracked, [])
        pi = t.player_index()
        self.assertEqual((pi.shape, int(pi.max())), ((480, 640), 0))
        self.assertIsNone(t.hands(0))
        self.assertIsNone(t.pointers(0))
        u = frame.users[0]
        self.assertEqual(u.joints.shape, (freenect_nui.JOINTS, 3))
        t.set_grip(freenect_nui.GRIP_TRIANGLE | freenect_nui.GRIP_CLOSED)
        t.set_max_tracked(3)
        smoother = freenect_nui.Smoother()
        smoother.smooth(frame)
        self.assertIsNone(frame.bone_orientations(0))

    def test_missing_model(self):
        with self.assertRaises(FileNotFoundError):
            freenect_nui.Tracker(models_dir=os.path.join(os.path.dirname(__file__), "no-such-directory"))


if __name__ == "__main__":
    unittest.main()
