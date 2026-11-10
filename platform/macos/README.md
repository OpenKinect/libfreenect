# libfreenect on macOS

Supported: the newest macOS and the one before it, on Apple Silicon and Intel.
libfreenect uses libusb's IOKit backend; no kernel extension, driver or
special permission is needed.

    brew install libusb glfw cmake
    cmake -S . -B build && cmake --build build

To build a universal library (arm64 and x86_64), libusb has to be universal
too (Homebrew's isn't): build it from source with
`CFLAGS="-arch arm64 -arch x86_64"` and configure libfreenect with
`-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`, `LIBUSB_1_INCLUDE_DIR` and
`LIBUSB_1_LIBRARY` pointing to it. CI does exactly this
(`.github/workflows/ci.yml`, "macOS universal").

## Audio firmware at plug-in

The Kinect's audio controller starts in a bootloader and needs firmware before
its microphones work (and, on the model 1473 and the Kinect for Windows, the
motor and LED). Microsoft's UAC firmware turns it into a standard USB audio
device, which Core Audio then offers as a 4-channel microphone (Audio MIDI
Setup, Sound settings), with no libfreenect application running.

Get the firmware (downloaded from Microsoft, checked and extracted; nothing of
Microsoft's is shipped with libfreenect):

    python3 fwfetcher.py --uac --user     # ~/Library/Application Support/libfreenect

libfreenect also looks in `/opt/homebrew/share/libfreenect` and
`/usr/local/share/libfreenect`. If `audios.bin` (the Xbox firmware) is there
too, libfreenect prefers it; set `LIBFREENECT_AUDIO_FIRMWARE=uac` to use the UAC
firmware.

To upload it automatically whenever the Kinect is plugged in, install the
launchd agent (`share/libfreenect/macos/org.openkinect.fwloader.plist` after
`cmake --install`, or `build/platform/macos/` in a build tree):

    cp org.openkinect.fwloader.plist ~/Library/LaunchAgents/
    launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/org.openkinect.fwloader.plist

launchd starts `freenect-fwloader --launchd` when the audio bootloader
(USB 045e:02ad or 045e:02be) appears; it uploads the firmware and exits. To
remove it:

    launchctl bootout gui/$(id -u)/org.openkinect.fwloader
    rm ~/Library/LaunchAgents/org.openkinect.fwloader.plist

To use the agent with firmware in `~/Library/Application Support`, install it
as above (as your user, not in `/Library/LaunchAgents` for all users).

## One beamformed microphone

With the UAC firmware running, Core Audio offers the Kinect's 4 microphones as
one 4-channel input. libfreenect turns them into one microphone,
**Kinect Beamformed Microphone**, which every app can pick (Sound settings,
FaceTime, Zoom, …). It follows the talker among 11 fixed beams from −50° to
+50°, as Microsoft's SDK does.

It takes two parts, both built with libfreenect (the audio DSP is on by
default):
- `KinectBeamMic.driver`, a Core Audio server plug-in. It runs in user space,
  inside `coreaudiod`; no kernel extension. It publishes the microphone and a
  hidden sink that feeds it.
- `freenect-coreaudio-mic.app` ("Kinect Beam Mic"), a background helper with
  no window or Dock icon. It captures the Kinect's 4 microphones, beamforms
  them and plays the result into the sink. A plug-in can't capture another
  device itself, which is why the work is split this way.

After `cmake --install` both are in `share/libfreenect/macos`:

    sudo cp -R KinectBeamMic.driver /Library/Audio/Plug-Ins/HAL/
    sudo killall coreaudiod
    cp org.openkinect.beam-mic.plist ~/Library/LaunchAgents/
    launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/org.openkinect.beam-mic.plist

The agent starts the helper at login and keeps it running. The first time it
records, macOS asks whether "Kinect Beam Mic" may use the microphone; allow
it (System Settings > Privacy & Security > Microphone).

To check: `freenect-coreaudio-mic.app/Contents/MacOS/freenect-coreaudio-mic --list`
shows the audio devices and marks the Kinect. `--self-test` plays a tone
through the plug-in and records it from the microphone (no Kinect needed).
Run by hand, the helper takes `--beam auto|adaptive|ANGLE`, `--sdk-weights`,
`--noise-suppression`, `--agc` (with SpeexDSP) and `--verbose`.

To remove it:

    launchctl bootout gui/$(id -u)/org.openkinect.beam-mic
    rm ~/Library/LaunchAgents/org.openkinect.beam-mic.plist
    sudo rm -rf /Library/Audio/Plug-Ins/HAL/KinectBeamMic.driver
    sudo killall coreaudiod

Both bundles are signed ad hoc when they're built, which is enough on the Mac
that built them. To give them to others, sign them with a Developer ID and
notarize them.

## Notes

- libfreenect claims only interface 0 of the audio controller; with the UAC
  firmware Core Audio keeps the microphone interfaces.
- The viewers use GLFW and OpenGL, which is deprecated on macOS but still
  present.
- Planned: a Camera Extension that serves depth and colour to other apps
  (optional, needs Developer ID signing).
