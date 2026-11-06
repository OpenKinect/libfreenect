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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libfreenect.h"
#include "console_exit.h"

// Usage: freenect-camtest [seconds]
// Without an argument it runs until Ctrl+C; with one it stops by itself and
// prints how many frames arrived.

static unsigned depth_frames = 0, video_frames = 0;

void depth_cb(freenect_device* dev, void* data, uint32_t timestamp)
{
	depth_frames++;
	printf("Received depth frame at %d\n", timestamp);
}

void video_cb(freenect_device* dev, void* data, uint32_t timestamp)
{
	video_frames++;
	printf("Received video frame at %d\n", timestamp);
}

int main(int argc, char** argv)
{
	// Stop cleanly on Ctrl+C, termination, or when the terminal is closed.
	fn_install_exit_handlers();
	int seconds = argc > 1 ? atoi(argv[1]) : 0;

	// Initialize libfreenect.
	freenect_context* fn_ctx;
	int ret = freenect_init(&fn_ctx, NULL);
	if (ret < 0)
		return ret;

	// Show debug messages and use camera only.
	freenect_set_log_level(fn_ctx, FREENECT_LOG_DEBUG);
	freenect_select_subdevices(fn_ctx, FREENECT_DEVICE_CAMERA);

	// Find out how many devices are connected.
	int num_devices = ret = freenect_num_devices(fn_ctx);
	if (ret < 0)
		return ret;
	if (num_devices == 0)
	{
		printf("No device found!\n");
		freenect_shutdown(fn_ctx);
		return 1;
	}

	// Open the first device.
	freenect_device* fn_dev;
	ret = freenect_open_device(fn_ctx, &fn_dev, 0);
	if (ret < 0)
	{
		freenect_shutdown(fn_ctx);
		return ret;
	}

	// Set depth and video modes.
	ret = freenect_set_depth_mode(fn_dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM));
	if (ret < 0)
	{
		freenect_shutdown(fn_ctx);
		return ret;
	}
	ret = freenect_set_video_mode(fn_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
	if (ret < 0)
	{
		freenect_shutdown(fn_ctx);
		return ret;
	}

	// Set frame callbacks.
	freenect_set_depth_callback(fn_dev, depth_cb);
	freenect_set_video_callback(fn_dev, video_cb);

	// Start depth and video.
	ret = freenect_start_depth(fn_dev);
	if (ret < 0)
	{
		freenect_shutdown(fn_ctx);
		return ret;
	}
	ret = freenect_start_video(fn_dev);
	if (ret < 0)
	{
		freenect_shutdown(fn_ctx);
		return ret;
	}

	// Run until interruption, failure, or the requested duration.
	time_t start = time(NULL);
	while (!fn_exit_requested)
	{
		struct timeval tv = { 0, 100000 }; // wake up at least every 100 ms
		if (freenect_process_events_timeout(fn_ctx, &tv) < 0)
			break;
		if (seconds > 0 && time(NULL) - start >= seconds)
			break;
	}
	double elapsed = difftime(time(NULL), start);

	printf("Shutting down\n");
	if (elapsed > 0)
		printf("Frames in %.0f s: depth %u (%.1f/s), video %u (%.1f/s)\n",
		       elapsed, depth_frames, depth_frames / elapsed, video_frames, video_frames / elapsed);

	// Stop everything and shutdown.
	freenect_stop_depth(fn_dev);
	freenect_stop_video(fn_dev);
	freenect_close_device(fn_dev);
	freenect_shutdown(fn_ctx);

	printf("Done!\n");
	fn_exit_done();

	return 0;
}
