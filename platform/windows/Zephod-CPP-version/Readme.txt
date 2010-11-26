Zephods win32 kinect driver library (& demo)

(also see http://ajaxorg.posterous.com/kinect-driver-for-windows-prototype )

0) Connect the Kinect to your pc
1) Install the motor driver from the drivers folder
2) Install the camera and/or audio driver from the drivers folder
3) Run KinectTest.exe

To build your own applications, include "kinect-win32" and add the kinect project as a dependency (or link against the lib file)
See Kinect-Test.cpp for a usage example.

This project has been based/built on the libusb-win32 lib (see folder for license etc) and the work on the openkinect github ( https://github.com/openkinect ) 

Zephod / Stijn Kuipers 
- http://www.blokmodular.com
- http://nl.linkedin.com/in/stijnkuipers
- @Blok2009 on twitter
- stijn2@chello.nl

Special thanks go to all the people in #OpenKinect and Maa for finding out a better way to init in vista64.

* changes in v17
- added IR modes!

* changes in v16
- gl fixes to make it work on ati boards
- freenect header + wrapper -> you can now use the freenect api on windows!
- Maa added thread start/stop and did general cleanup
- small gl-detection fix courtesy of Tommy Anderson

* changes in v15
- demo changed to run with glsl shaders!
- see title bar for keyboard commands

* changes in v14:
- more demo modes (particles!)
- added some initial calibration info
- moved some common stuff from the demo to kinect-utility

* changes in v13:
- huge cleanup/rewrite of the usb code. unified iso-transfers now make upgrades in packethandling easier
- added osc/blobtracking mode (disabled by default, "SENDOSC" compile switch)
- added static-background removal (use space in GL window to switch)
- added zoom (keypad +/-)

*known issues in v13:
- vista64 still problematic.. drops frames a lot.

* changes in v12:
- more cleanups
- accelero data is available now
- demo has rgb-unproject working (space to switch mode, qwased to tweak params)
- new minimal example to just get the kinect going

* changes in v8:
- new pointcloud demo
- general cleanups everywhere
- keyboard commands for cam/led control in the demo

* changes in v7:
- framedropping support.
	- incomplete frames are no longer sent to the callbacks to prevent glitches
- halved packet size for rgb-transfer
- added smarter packet-type detection for rgb transfer
- added led and motor control

* known issues in v7:
- for some reason the debug build currently drops less packets the the releasebuild.. working on this
- vista64 seems to have trouble with the libusb driver.