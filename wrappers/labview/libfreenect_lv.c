/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * Ryan Gordon <rygordon4@gmail.com>
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
#include <assert.h>

#include "libfreenect.h"
#include "../c_sync/libfreenect_sync.h"
#include "libfreenect_lv.h"

EXPORT short *freenect_lv_get_depth_frame(int index, freenect_depth_format fmt)
{
	short *depth = 0;
	unsigned int timestamp;
	
	if(freenect_sync_get_depth((void**)&depth, &timestamp, index, fmt) < 0)
	{
	    return NULL;
	}

	return depth;
}

EXPORT char *freenect_lv_get_image_frame(int index, freenect_video_format fmt)
{
	char *rgb = 0;
	unsigned int timestamp;
	
	if(freenect_sync_get_video((void**)&rgb, &timestamp, index, fmt) < 0)
	{
	    return NULL;
	}
	return rgb;
}

EXPORT int freenect_lv_set_tilt_degs(int angle, int index)
{
	return freenect_sync_set_tilt_degs(angle, index);
}

EXPORT freenect_raw_tilt_state *freenect_lv_get_tilt_state(int index)
{
	freenect_raw_tilt_state *state = 0;

	freenect_sync_get_tilt_state(&state, index);

	return state;
}

EXPORT void freenect_lv_get_tilt_state_mks(double *dx, double *dy, double *dz, int index)
{
	freenect_raw_tilt_state *state = 0;

	freenect_sync_get_tilt_state(&state, index);
	freenect_get_mks_accel(state, dx, dy, dz);
}

EXPORT void freenect_lv_set_led(freenect_led_options led, int index)
{
	freenect_sync_set_led(led, index);
}

EXPORT void freenect_lv_stop()
{
	freenect_sync_stop();
}