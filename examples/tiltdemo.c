/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * Andrew Miller <amiller@dappervision.com>
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

#include "libfreenect.h"
#include "libfreenect_sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef _WIN32
  #include <unistd.h>
#else
  // Microsoft Visual C++ does not provide the <unistd.h> header, but most of
  // its contents can be found within the <stdint.h> header:
  #include <stdint.h>
  // except for the UNIX sleep() function that has to be emulated:
  #include <windows.h>
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/sleep.html
  // According to the link above, the semantics of UNIX sleep() is as follows:
  // "If sleep() returns because the requested time has elapsed, the value
  //  returned shall be 0. If sleep() returns due to delivery of a signal, the
  //  return value shall be the "unslept" amount (the requested time minus the
  //  time actually slept) in seconds."
  // The following function does not implement the return semantics, but
  // will do for now... A proper implementation would require Performance
  // Counters before and after the forward call to the Windows Sleep()...
  unsigned sleep(unsigned seconds)
  {
    Sleep(seconds*1000);  // The Windows Sleep operates on milliseconds
    return(0);
  }
  // Note for MinGW-gcc users: MinGW-gcc also does not provide the UNIX sleep()
  // function within <unistd.h>, but it does provide usleep(); trivial wrapping
  // of sleep() aroung usleep() is possible, however the usleep() documentation
  // (http://docs.hp.com/en/B2355-90682/usleep.2.html) clearly states that:
  // "The useconds argument must be less than 1,000,000. (...)
  //  (...) The usleep() function may fail if:
  //  [EINVAL]
  //     The time interval specified 1,000,000 or more microseconds."
  // which means that something like below can be potentially dangerous:
  // unsigned sleep(unsigned seconds)
  // {
  //   usleep(seconds*1000000);  // The usleep operates on microseconds
  //   return(0);
  // }
  // So, it is strongly advised to stick with the _WIN32/_MSC_VER
  // http://www.xinotes.org/notes/note/439/
#endif//_WIN32



/* This is a simple demo. It connects to the kinect and plays with the motor,
   the accelerometers, and the LED. It doesn't do anything with images. And,
   unlike the other examples, no OpenGL is required!

   So, this should serve as the reference example for working with the motor,
   accelerometers, and LEDs.   */

void no_kinect_quit(void)
{
	printf("Error: Kinect not connected?\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	srand(time(0));

	while (1) {
		// Pick a random tilt and a random LED state
		freenect_led_options led = (freenect_led_options) (rand() % 6); // explicit cast
		int tilt = (rand() % 30)-15;
		freenect_raw_tilt_state *state = 0;
		double dx, dy, dz;

		// Set the LEDs to one of the possible states
		if (freenect_sync_set_led(led, 0)) no_kinect_quit();

		// Set the tilt angle (in degrees)
		if (freenect_sync_set_tilt_degs(tilt, 0)) no_kinect_quit();

		// Get the raw accelerometer values and tilt data
		if (freenect_sync_get_tilt_state(&state, 0)) no_kinect_quit();

		// Get the processed accelerometer values (calibrated to gravity)
		freenect_get_mks_accel(state, &dx, &dy, &dz);

		printf("led[%d] tilt[%d] accel[%lf,%lf,%lf]\n", led, tilt, dx,dy,dz);

		sleep(3);
	}
}


