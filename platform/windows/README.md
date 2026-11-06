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

`--uninstall` removes every package and certificate the tool has added.

To check a package without changing the system, use:

    freenect-driver-install --package-only <dir>

It builds and signs a package in `<dir>`, trusts no certificate and installs nothing.

### Alternatives

[Zadig](https://zadig.akeo.ie/) also works: choose **WinUSB** (or libusbK) for each function above (enable *Options > List All Devices*).
The old libusb-win32 (libusb0) driver can't do isochronous transfers, so it can't stream video or depth.
If you use [UsbDk](https://github.com/daynix/UsbDk) instead, set `LIBFREENECT_USBDK=1`.

## Audio firmware

The Kinect's audio controller needs firmware (`audios.bin`, fetched with `src/fwfetcher.py`) after every power-up.
On Kinect for Windows and the Xbox model 1473 it also controls the motor and LED.
libfreenect uploads the firmware when an application opens the audio device.
To do it ahead of time, for example at login, run:

    freenect-fwloader [--firmware path\to\audios.bin]

Without `--firmware`, `audios.bin` is looked for in `%LIBFREENECT_FIRMWARE_PATH%`, the current directory, `%LOCALAPPDATA%\libfreenect`, the directory of the program, and `%ProgramData%\libfreenect`.

## Building

libusb, CMake and MSVC are enough. pthreads-win32 is optional: without it, the wrappers and examples use a header-only pthread subset in `platform/windows/pthread`.

    cmake .. -DLIBUSB_1_INCLUDE_DIR="C:\path\to\libusb\include" -DLIBUSB_1_LIBRARY="C:\path\to\libusb-1.0.lib"
    cmake --build . --config Release
