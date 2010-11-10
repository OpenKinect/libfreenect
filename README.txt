
Horribly hacky first take at a Kinect Camera driver. Does RGB and Depth.

main.c implements a simple OpenGL driver. Hopefully it should be mostly self-explanatory...
You pretty much just open the USB device, call cams_init(dev, depthimg, rgbimg), and your
depthimg and rgbimg callbacks get called as libusb processes events.
