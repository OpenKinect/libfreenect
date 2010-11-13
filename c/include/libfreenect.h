/*  libfreenect - an open source Kinect driver

Copyright (C) 2010  Hector Martin "marcan" <hector@marcansoft.com>

This code is licensed to you under the terms of the GNU GPL, version 2 or version 3;
see:
 http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 http://www.gnu.org/licenses/gpl-3.0.txt
*/

#ifndef LIBFREENECT_H
#define LIBFREENECT_H

typedef void (*depthcb)(uint16_t *buf, int width, int height);
typedef void (*rgbcb)(uint8_t *buf, int width, int height);

#ifdef __cplusplus
extern "C" {
#endif
	void cams_init(libusb_device_handle *d, depthcb depth_cb, rgbcb rgb_cb);
#ifdef __cplusplus
}
#endif

#endif
