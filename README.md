libfreenect
===========

libfreenect is a userspace driver for the Microsoft Kinect.
It runs on Linux, OSX, and Windows and supports

- RGB and Depth Images
- Motors
- Accelerometer
- LED
- Audio

Notice: If you have the newer Kinect v2 (XBox One), use [OpenKinect/libfreenect2](https://github.com/OpenKinect/libfreenect2) instead.


# Build Instructions

To build libfreenect, you'll need

- [libusb](http://libusb.info) >= 1.0.18 (Windows needs >= 1.0.26)
- [CMake](http://cmake.org) >= 3.12.4
- [python](http://python.org) >= 2.7 or >= 3.3 (only if BUILD_PYTHON=ON or BUILD_PYTHON2=ON or BUILD_PYTHON3=ON or BUILD_REDIST_PACKAGE=OFF)

For the examples, you'll need

- OpenGL   (included with OSX)
- glut     (included with OSX)
- [pthreads-win32](http://sourceforge.net/projects/pthreads4w/) (Windows, optional: without it, a built-in header-only pthread subset over native Win32 threads is used)

## <a name="fetch-build"></a>Fetch & Build

    git clone https://github.com/OpenKinect/libfreenect
    cd libfreenect
    mkdir build
    cd build
    cmake -L .. # -L lists all the project options
    make

    # if you don't have `make` or don't want color output
    # cmake --build .

Use CMake options to control what gets built.
For example, to build the Python wrapper (defaults to system Python):

    cmake .. -DBUILD_PYTHON=ON
    make

Or the Python 3 wrapper:

    cmake .. -DBUILD_PYTHON3=ON
    make


You can specify a build with debug symbols:

    cmake .. -DCMAKE_BUILD_TYPE=debug
    # or with optimizations
    # cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo

You can build .deb, .rpm, and/or .tgz packages with `cpack`:

    cmake .. -DBUILD_CPACK_DEB=ON -DBUILD_CPACK_RPM=ON -DBUILD_CPACK_TGZ=ON
    cpack

For audio support, you must upload firmware to the device.
Newer Kinect models may require audio firmware for motor and LED support.
The best method is to [insert firmware at runtime](https://github.com/OpenKinect/libfreenect/issues/376#issuecomment-41211251) just after calling `freenect_init()`.

Alternately, firmware for Kinect model 1414 can be downloaded automatically by specifying:

    cmake .. -DBUILD_REDIST_PACKAGE=OFF

Note that firmware may not be legal to redistribute in your jurisdiction!

## OSX

If you don't have a package manager, install [Homebrew](http://brew.sh/).
For a manual build, see [the wiki](http://openkinect.org/wiki/Getting_Started#Manual_Build_under_OSX).

### Homebrew

    brew install libfreenect
    # or get the very latest:
    # brew install --HEAD libfreenect

### MacPorts

    sudo port install git-core cmake libusb libtool

Continue with [Fetch & Build](#fetch-build).


## Linux

Remember to install the [udev rules](https://github.com/OpenKinect/libfreenect/tree/master/platform/linux/udev).
For a manual build, see [the wiki](http://openkinect.org/wiki/Getting_Started#Manual_Build_on_Linux).

### Ubuntu/Debian/Mint

The version packaged in Ubuntu may be very old.
To install newer packaged builds, see [the wiki](http://openkinect.org/wiki/Getting_Started#Ubuntu.2FDebian).
Continue with this section for a manual build.

    sudo apt-get install git cmake build-essential libusb-1.0-0-dev

    # only if you are building the examples:
    sudo apt-get install freeglut3-dev libxmu-dev libxi-dev

Continue with [Fetch & Build](#fetch-build).

There is also a [debian branch](https://github.com/OpenKinect/libfreenect/tree/debian) for packaging purposes.

### Gentoo Linux

There are [dev-libs/libfreenect](https://github.com/piedar/overboard/tree/master/dev-libs/libfreenect) ebuilds in the [overboard repo](https://github.com/piedar/overboard).

### Arch Linux

There is a [libfreenect](https://aur.archlinux.org/packages/libfreenect) PKGBUILD in the AUR.
Alternately, the [libfreenect-git](https://aur.archlinux.org/packages/libfreenect-git) PKGBUILD builds the very latest.


## Windows

libfreenect talks to the Kinect from user mode through libusb, using Microsoft's in-box WinUSB driver.
No kernel driver of its own is needed, so it works on Windows 11 with Memory integrity (core isolation) turned on, unlike the old Kinect for Windows SDK driver.
Use libusb 1.0.26 or newer on Windows 8.1 or newer. Older libusb versions lack WinUSB isochronous support or have a known regression in it.

Bind the Kinect to WinUSB by running `freenect-driver-install --install` from an Administrator terminal; `--list` shows the current drivers and `--uninstall` undoes it.
See [platform/windows/README.md](platform/windows/README.md) for what it changes.
[Zadig](http://zadig.akeo.ie/) also works: install **WinUSB** (or libusbK) for the camera, the motor and the audio device (enable *Options > List All Devices*).
The old libusb-win32 (libusb0) driver cannot stream video or depth.
If you have [UsbDk](https://github.com/daynix/UsbDk) installed instead, set the environment variable `LIBFREENECT_USBDK=1` to make libusb use it.

Follow [Fetch & Build](#fetch-build) or use Github and CMake GUI tools.
pthreads-win32 is optional (see above), so MSVC builds of the wrappers and examples need only libusb and, for the viewers, GLUT.
Remember to supply paths to CMake so it can find dependencies.
For example:

    cmake .. -DLIBUSB_1_INCLUDE_DIR="C:\path\to\libusb\include" -DLIBUSB_1_LIBRARY="C:\path\to\libusb\libusb.lib"


# Wrappers

Interfaces to various languages are provided in [wrappers/](https://github.com/OpenKinect/libfreenect/tree/master/wrappers).
Wrappers are not guaranteed to be API stable or up to date.

- C (using a synchronous API)
- C++
- C#
- python
- ruby
- actionscript
- Java (JNA)

# Fakenect

Using fakenect, you can record a session to a directory and play it back later.

    mkdir session
    fakenect-record ./session

To use a fakenect recorded stream, just provide the fakenect lib as a pre loaded library with `LD_PRELOAD` and indicates the recorded files directory with `FAKENECT_PATH`.

- Sample with python wrappers :
```shell
    LD_PRELOAD="/usr/local/lib/fakenect/libfakenect.so" FAKENECT_PATH="./session" python ./wrappers/python/demo_cv_sync.py
```
- Sample with C bin :
```shell
    LD_PRELOAD="/usr/local/lib/fakenect/libfakenect.so" FAKENECT_PATH="./session" freenect-glview
```

# Code Contributions

In order of importance:

- Make sure to sign commits: `git commit -s`
- Use a [feature branch](https://www.atlassian.com/git/workflows#!workflow-feature-branch) in your own fork and target master with pull requests
- Tab indentation, no trailing whitespace


# Maintainers

Ongoing Development and Maintenance by the OpenKinect Community

http://www.openkinect.org

- Original Code and Engineering: Hector Martin (marcan)
- Community Lead: Josh Blake (JoshB)
- Integration: Kyle Machulis (qDot)


# License

The libfreenect project is covered under a dual Apache v2/GPL v2
license. The licensing criteria are listed below, as well as at the
top of each source file in the repo.

```
This file is part of the OpenKinect Project. http://www.openkinect.org

Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB
file for details.

This code is licensed to you under the terms of the Apache License,
version 2.0, or, at your option, the terms of the GNU General Public
License, version 2.0. See the APACHE20 and GPL2 files for the text of
the licenses, or the following URLs:
http://www.apache.org/licenses/LICENSE-2.0
http://www.gnu.org/licenses/gpl-2.0.txt

If you redistribute this file in source form, modified or unmodified,
you may:

- Leave this header intact and distribute it under the same terms,
  accompanying it with the APACHE20 and GPL2 files, or
- Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
- Delete the GPL v2 clause and accompany it with the APACHE20 file

In all cases you must keep the copyright notice intact and include a
copy of the CONTRIB file.

Binary distributions must follow the binary distribution requirements
of either License.
```


# More Information

Information about the OpenKinect project can be found at http://www.openkinect.org

For questions, support, and discussion, check out the google groups mailing list at http://groups.google.com/group/openkinect

Or the IRC channel at \#openkinect on [Freenode](http://freenode.net/)

We are also on twitter at http://twitter.com/openkinect
