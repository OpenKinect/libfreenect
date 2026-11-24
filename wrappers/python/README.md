# libfreenect for Python

Python 3 bindings for libfreenect (Kinect v1), with NumPy:

- `freenect`: the Kinect's streams (synchronous `sync_get_depth`/`sync_get_video`,
  or callbacks with `runloop`), motor, LED and accelerometer, the depth range,
  the IR projector, the Kinect for Windows' colour settings, the serial number,
  the microphone geometry, and `CoordinateMapper` (depth ↔ colour ↔ camera
  space, as Microsoft's SDK maps them, with the Kinect's own calibration);
- `freenect_audio`: `Beamformer`, the microphone array's beamforming and
  talker direction, for 4-channel audio you record through the operating system;
- `freenect_nui`: `Tracker`, skeleton tracking with the SDK's 20 body joints
  plus palms and named fingertips (32 joints), hand states, the hands'
  orientation, clasped hands, two-hand manipulation (turn and zoom a held
  object) and one-hand pointers (cursor, press, grip, swipes).

## Install

From a checkout of libfreenect, with [uv](https://docs.astral.sh/uv/) or pip:

    uv pip install ./wrappers/python

This compiles libfreenect into the modules, so no separate libfreenect
installation is needed. It needs a C compiler, CMake (fetched automatically if
missing) and libusb 1.0.26 or newer:

- **Linux:** `sudo apt install libusb-1.0-0-dev` (and the udev rules from
  `platform/linux` for access without root).
- **macOS:** `brew install libusb`.
- **Windows:** Visual Studio's C compiler, and libusb (for example the release
  from libusb.info). Tell CMake where it is; its DLL then goes into the package:

      uv pip install ./wrappers/python -C cmake.define.LIBUSB_1_INCLUDE_DIR=C:/libusb/include ^
          -C cmake.define.LIBUSB_1_LIBRARY=C:/libusb/VS2022/MS64/dll/libusb-1.0.lib

  The Kinect must be bound to WinUSB (`freenect-driver-install`).

`uv build ./wrappers/python` makes a wheel instead. To build without the
skeleton tracker: `-C cmake.define.FREENECT_PYTHON_NUI=OFF`.

The skeleton tracker uses Microsoft's body-part model, which libfreenect does
not contain: fetch it once with `python3 src/fwfetcher.py --nui --user`
(Linux, macOS) or `freenect-driver-install --fetch-nui-models` (Windows).

## Examples

    import freenect, freenect_nui
    tracker = freenect_nui.Tracker()
    while True:
        depth, _ = freenect.sync_get_depth(format=freenect.DEPTH_MM)
        frame = tracker.process(depth)
        for user in frame.tracked:
            print(user.tracking_id, user.joint("head"), user.joint("index_right"),
                  freenect_nui.HAND_STATE_NAMES[user.hand_state[1]])
            pointers = tracker.pointers(user.index)
            for hand in pointers:
                if hand["events"]:
                    print(hand["events"])

    import freenect_audio
    bf = freenect_audio.Beamformer()
    mono = bf.process(frames)          # frames: (n, 4) int32 or float32 at 16 kHz
    print(bf.source_angle)             # (degrees, confidence)

The `demo_*.py` scripts show the streams with OpenCV.

## Tests

    python -m unittest discover -s wrappers/python/tests -v

No Kinect is needed; the skeleton tests run when the model is installed.

## Building with libfreenect's CMake instead

`cmake -DBUILD_PYTHON3=ON` builds the same modules against libfreenect's
shared libraries and installs them to Python's site-packages. Python 2 is no
longer supported.
