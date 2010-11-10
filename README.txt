Horribly hacky first take at a Kinect Camera driver. Does RGB and Depth.

main.c implements a simple OpenGL driver. Hopefully it should be mostly self-explanatory...
You pretty much just open the USB device, call cams_init(dev, depthimg, rgbimg), and your
depthimg and rgbimg callbacks get called as libusb processes events.

Libfreenect is Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2 or
version 3; see:
 http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 http://www.gnu.org/licenses/gpl-3.0.txt

