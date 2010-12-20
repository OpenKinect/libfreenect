/**
@mainpage  libfreenect
@author The OpenKinect Community - http://www.github.com/openkinect

Cross-platform driver for the Microsoft Kinect Camera

Website: http://www.openkinect.org

@section libfreenectIntro Introduction

libfreenect is an open source, cross platform development library for
the Microsoft Kinect camera. It provides basic functionality to
connect to the camera, set configuration values, retrieve (and in some
cases decompress) images, and provides functionalty for the LED and
Motor.

@section libfreenectDesignOverview Design Overview

libfreenect provides access to devices via two structs:

- A context, which manages aspects of thread safety when using
  multiple devices on multiple threads.
- A device, which talks to the hardware and manages transfers and configuration.

Either or both of these structs are passed to the functions in order
to interact with the hardware. The USB access is handled by
libusb-1.0, which should work in a mostly non-blocking fashion across
all platforms (see function documentation for specifics).

@section libfreenectShouldIUseIt Should You Use libfreenect?

The main design goal of libfreenect is to provide a simple, usable
reference implementation of the Kinect USB protocol for access via
non-Xbox hardware. With this in mind, the library does not contain any
algorithms relevant to computer vision usages of the camera.

If you are looking for machine vision algorithms, we recommend the
OpenCV library, available at

http://www.opencv.org

If you are looking to use the kinect in a larger framework that may
involve other depth sensors, we recommend the OpenNI framework,
available at

http://www.openni.org

Note that libfreenect can be used as a hardware node in OpenNI.

*/
