OpenNI2-FreenectDriver
======================

OpenNI2-FreenectDriver is a bridge to libfreenect implemented as an OpenNI2 driver.
It allows OpenNI2 to use Kinect hardware on Linux and OSX.
OpenNI2-FreenectDriver is distributed under the [Apache 2](https://github.com/OpenKinect/libfreenect/blob/master/APACHE20) license.

Install
-------
1. Download and unpack [OpenNI](http://structure.io/openni) 2.2.0.33 or higher.
2. Go to the top libfreenect directory and build it with the OpenNI2 driver.

        mkdir build
        cd build
        cmake .. -DBUILD_OPENNI2_DRIVER=ON
        make

3. Copy the driver to your OpenNI2 driver repository. You must first change `Repository` to match your project layout.

        Repository="/example/path/to/Samples/Bin/OpenNI2/Drivers/"
        cp -L lib/OpenNI2-FreenectDriver/libFreenectDriver.{so,dylib} ${Repository}
        
        # you could instead make a symlink to avoid copying after every build
        # ln -s lib/OpenNI2-FreenectDriver/libFreenectDriver.{so,dylib} ${Repository}

OpenNI2-FreenectDriver is built with a static libfreenect, so you do not need to include libfreenect when deploying.
However, you will need to make sure target systems have libusb and all other dependencies listed in `ldd libFreenectDriver.so`.

__________________________________________________

Structure
---------
This driver is modeled on TestDevice.cpp and Drivers/Kinect/.
In the FreenectDriver namespace, it ties together the C++ interfaces of OpenNI2 and libfreenect using multiple inheritance.

Driver inherits publically from oni::driver::DriverBase and privately from Freenect::Freenect.
libfreenect.hpp allows protected access to the Freenect context, so that FreenectDriver can call the Freenect's C API.
As a DriverBase, FreenectDriver manages devices and sets up device state callbacks.

Device inherits publically from oni::driver::DeviceBase and Freenect::FreenectDevice.
Because of this, it can be built by Freenect::Freenect::createDevice() and it can define Device's depth and video callbacks.
Those callbacks trigger acquireFrame() in FreenectStream.

VideoStream is a virtual base class inheriting from oni::driver::StreamBase.
It does generic frame setup in buildFrame() and then calls pure virtual populateFrame() to let derived classes finish the frame.
It also provides the base skeleton for setting and getting properties, which cascades down the inheritance tree.

DepthStream and ColorStream are nearly identical in definition and implementation, both inheriting from VideoStream.
They differ mostly in the formats they use to process data and the video modes they support.
These two classes offer a system to store and report supported video modes.
To implement a new mode, simply add it to getSupportedVideoModes() and modify populateFrame() as necessary.

__________________________________________________

Todo
----
* support more FREENECT_RESOLUTION_\*, FREENECT_VIDEO_\*, and FREENECT_DEPTH_\*
* provide more OniVideoMode and OniStreamProperty
* implement remaining derived functions
