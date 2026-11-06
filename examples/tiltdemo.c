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

#include "console_exit.h"



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
	// Stop cleanly on Ctrl+C, termination, or when the terminal is closed.
	fn_install_exit_handlers();
	srand((unsigned)time(0));

	while (!fn_exit_requested) {
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

		fn_sleep_ms(3000);
	}

	printf("Stopping\n");
	freenect_sync_stop();
	printf("Done\n");
	fn_exit_done();
	return 0;
}
