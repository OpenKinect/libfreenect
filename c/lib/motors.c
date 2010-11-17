/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "freenect_internal.h"
// The kinect can tilt from +31 to -31 degrees in what looks like 1 degree increments
// The control input looks like 2*desired_degrees
const double MAX_TILT_ANGLE = 31;
const double MIN_TILT_ANGLE = -31;


int freenect_set_tilt_in_degrees(freenect_device *dev, double angle)
{
  int ret;
  uint8_t empty[0x1];
  angle = (((angle<MIN_TILT_ANGLE)?MIN_TILT_ANGLE:angle)>MAX_TILT_ANGLE)?MAX_TILT_ANGLE:((angle<MIN_TILT_ANGLE)?MIN_TILT_ANGLE:angle);
  angle = angle * 2;
  ret = fnusb_control(&dev->usb_motor, 0x40, 0x31, (uint16_t)angle, 0x0, empty, 0x0);
  return ret;

}

int freenect_set_tilt_in_radians(freenect_device *dev, double angle)
{
  int ret;
  angle = angle/M_PI*180;
  ret = freenect_set_tilt_in_degrees(dev,angle);
  return ret;

}

int freenect_set_led(freenect_device *dev, freenect_led_options option)
{
  int ret;
  uint8_t empty[0x1];
  ret = fnusb_control(&dev->usb_motor, 0x40, 0x06, (uint16_t)option, 0x0, empty, 0x0);
  return ret;

}


