# libfreenect on Windows

libfreenect drives the Kinect v1 from user mode through [libusb](https://libusb.info), on Microsoft's in-box **WinUSB** driver.
It installs no kernel driver of its own, so it works on Windows 11 with *Memory integrity* (core isolation) turned on.
The Kinect for Windows SDK's camera driver (`kinectcamera.sys`) is blocked there.

Requirements: Windows 8.1 or newer, and libusb 1.0.26 or newer.

## Binding the Kinect to WinUSB

Each Kinect USB function needs WinUSB as its driver. `freenect-driver-install` does this.
It's built by default on Windows (CMake option `BUILD_DRIVER_INSTALLER`).

From an **Administrator** terminal:

    freenect-driver-install --install

To list the Kinect functions and their current drivers (no Administrator rights needed):

    freenect-driver-install --list

To undo the installation:

    freenect-driver-install --uninstall

Then unplug and replug the Kinect.

### What `--install` does

1. It writes a driver package to `%ProgramData%\libfreenect\driver`: `libfreenect-kinect.inf`, which only references the in-box `winusb.inf`, and its catalog `libfreenect-kinect.cat`.
2. It creates a new self-signed code-signing certificate, signs the catalog with it, and **destroys the private key straight away**.
   Windows requires driver packages to be signed by a trusted publisher. Because the key no longer exists, the certificate that gets trusted can't sign anything else.
3. It adds that certificate to the local machine's *Trusted Root Certification Authorities* and *Trusted Publishers* stores, adds the package to the driver store, and installs it on every Kinect function that is plugged in.
   Functions that appear later pick it up automatically, for example the audio device after its firmware upload.

It binds these functions:

| Function | USB ID |
|---|---|
| Camera (depth, colour, IR) | 045E:02AE, 045E:02BF |
| Motor, accelerometer, LED | 045E:02B0, 045E:02AF |
| Audio controller (firmware loader, Xbox `audios.bin` firmware) | 045E:02AD, 045E:02BE |
| Audio controller running Microsoft's UAC firmware: control and security interfaces only | 045E:02BB and 045E:02C3, interfaces 0 and 1 |

The microphone-array interfaces (2 and 3) of 045E:02BB/02C3 are left alone, so Windows' own USB Audio driver keeps them and the four microphones show up as a normal recording device.

`--uninstall` removes every package and certificate the tool has added, and the downloaded audio firmware.

To check a package without changing the system, use:

    freenect-driver-install --package-only <dir>

It builds and signs a package in `<dir>`, trusts no certificate and installs nothing.

### Alternatives

[Zadig](https://zadig.akeo.ie/) also works: choose **WinUSB** (or libusbK) for each function above (enable *Options > List All Devices*).
The old libusb-win32 (libusb0) driver can't do isochronous transfers, so it can't stream video or depth.
If you use [UsbDk](https://github.com/daynix/UsbDk) instead, set `LIBFREENECT_USBDK=1`.

## Audio firmware

The Kinect's audio controller needs firmware after every power-up.

**Microsoft's firmware (`UAC.BIN`).** At the end of `--install`, `freenect-driver-install` fetches Microsoft's firmware (skip that with `--no-firmware`):
1. It downloads Microsoft's Kinect for Windows Runtime v1.8 installer (about 111 MB) from download.microsoft.com.
2. It checks that Microsoft Corporation signed it.
3. It extracts only the firmware file, without running the installer, and verifies the firmware's SHA-256.
4. It saves the firmware as `%ProgramData%\libfreenect\UACFirmware.bin`, and deletes the downloaded installer.

libfreenect doesn't distribute Microsoft's firmware, and Microsoft's licence terms for the Kinect for Windows Runtime apply to it. To fetch it on its own:

    freenect-driver-install --fetch-firmware [--from KinectRuntime-v1.8-Setup.exe] [DIR]

`--from` uses an installer you've already downloaded instead of downloading it again.

**Xbox firmware (`audios.bin`).** `src/fwfetcher.py` fetches it from the Xbox 360 system update. It's the firmware libfreenect's own audio API (`freenect_start_audio`) speaks. That API is disabled on Windows.

**Which image is loaded.** libfreenect uploads the firmware when an application opens the audio device. On Kinect for Windows and the Xbox model 1473, the audio controller also drives the motor and LED. On Windows libfreenect prefers `UACFirmware.bin` and falls back to `audios.bin`; on other platforms it's the other way round. Set `LIBFREENECT_AUDIO_FIRMWARE=uac` or `=xbox` to allow only one of them.

The files are looked for in `%LIBFREENECT_FIRMWARE_PATH%`, the current directory, `%LOCALAPPDATA%\libfreenect`, the directory of the program, and `%ProgramData%\libfreenect`. To upload ahead of time, for example at login, run:

    freenect-fwloader [--firmware path\to\image]

With `UACFirmware.bin`, the audio controller comes back as 045E:02BB (Kinect for Windows: 02C3). Its microphones are then a standard USB Audio device, a *Kinect USB Audio* recording device in Windows.

## Building

libusb, CMake and MSVC are enough. pthreads-win32 is optional: without it, the wrappers and examples use a header-only pthread subset in `platform/windows/pthread`.

    cmake .. -DLIBUSB_1_INCLUDE_DIR="C:\path\to\libusb\include" -DLIBUSB_1_LIBRARY="C:\path\to\libusb-1.0.lib"
    cmake --build . --config Release
