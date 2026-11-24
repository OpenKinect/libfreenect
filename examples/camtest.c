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

// Usage: freenect-camtest [seconds] [options]
// Without seconds it runs until Ctrl+C; with them it stops by itself.
// Once a second it prints the frames that arrived and, for millimetre depth,
// the nearest, farthest and centre distance.
//
//   --depth-res low|verylow   depth at 320x240 or 80x60 (default 640x480)
//   --range default|near|full depth range kept (default 800-4000 mm)
//   --hires                   colour at 1280x1024 (1280x960 on 1473/K4W)
//   --sync                    automatic depth/colour timing alignment
//   --no-depth                colour only

static unsigned depth_frames = 0, video_frames = 0;
static unsigned sec_depth = 0, sec_video = 0;
static int depth_w = 640, depth_h = 480;
static unsigned mm_min = 0, mm_max = 0, mm_centre = 0, mm_valid = 0;

static void depth_cb(freenect_device* dev, void* data, uint32_t timestamp)
{
	const uint16_t *d = (const uint16_t*)data;
	unsigned lo = 0xffff, hi = 0, valid = 0;
	int i, n = depth_w * depth_h;
	(void)dev; (void)timestamp;
	depth_frames++;
	sec_depth++;
	for (i = 0; i < n; i++) {
		if (d[i] == 0)
			continue;
		valid++;
		if (d[i] < lo) lo = d[i];
		if (d[i] > hi) hi = d[i];
	}
	mm_min = valid ? lo : 0;
	mm_max = hi;
	mm_valid = valid;
	mm_centre = d[(depth_h / 2) * depth_w + depth_w / 2];
}

static void video_cb(freenect_device* dev, void* data, uint32_t timestamp)
{
	(void)dev; (void)data; (void)timestamp;
	video_frames++;
	sec_video++;
}

static void usage(void)
{
	printf("Usage: freenect-camtest [seconds] [--depth-res low|verylow] [--range default|near|full] [--hires] [--sync] [--no-depth]\n");
}

int main(int argc, char** argv)
{
	int seconds = 0, i;
	freenect_resolution depth_res = FREENECT_RESOLUTION_MEDIUM;
	freenect_resolution video_res = FREENECT_RESOLUTION_MEDIUM;
	freenect_depth_range range = FREENECT_DEPTH_RANGE_DEFAULT;
	int sync = 0, no_depth = 0;

	// Stop cleanly on Ctrl+C, termination, or when the terminal is closed.
	fn_install_exit_handlers();

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];
		const char *v = i + 1 < argc ? argv[i + 1] : "";
		if (strcmp(a, "--depth-res") == 0) {
			if (strcmp(v, "low") == 0) depth_res = FREENECT_RESOLUTION_LOW;
			else if (strcmp(v, "verylow") == 0) depth_res = FREENECT_RESOLUTION_VERY_LOW;
			else if (strcmp(v, "medium") == 0) depth_res = FREENECT_RESOLUTION_MEDIUM;
			else { usage(); return 2; }
			i++;
		} else if (strcmp(a, "--range") == 0) {
			if (strcmp(v, "default") == 0) range = FREENECT_DEPTH_RANGE_DEFAULT;
			else if (strcmp(v, "near") == 0) range = FREENECT_DEPTH_RANGE_NEAR;
			else if (strcmp(v, "full") == 0) range = FREENECT_DEPTH_RANGE_FULL;
			else { usage(); return 2; }
			i++;
		} else if (strcmp(a, "--hires") == 0) {
			video_res = FREENECT_RESOLUTION_HIGH;
		} else if (strcmp(a, "--no-depth") == 0) {
			no_depth = 1;
		} else if (strcmp(a, "--sync") == 0) {
			sync = 1;
		} else if (a[0] >= '0' && a[0] <= '9') {
			seconds = atoi(a);
		} else {
			usage();
			return 2;
		}
	}

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
	freenect_frame_mode dmode = freenect_find_depth_mode(depth_res, FREENECT_DEPTH_MM);
	depth_w = dmode.width;
	depth_h = dmode.height;
	ret = freenect_set_depth_mode(fn_dev, dmode);
	if (ret < 0)
	{
		printf("Depth mode not available\n");
		freenect_shutdown(fn_ctx);
		return ret;
	}
	ret = freenect_set_video_mode(fn_dev, freenect_find_video_mode(video_res, FREENECT_VIDEO_RGB));
	if (ret < 0)
	{
		printf("Video mode not available\n");
		freenect_shutdown(fn_ctx);
		return ret;
	}
	freenect_set_depth_range(fn_dev, range);

	// Set frame callbacks.
	freenect_set_depth_callback(fn_dev, depth_cb);
	freenect_set_video_callback(fn_dev, video_cb);

	// Start depth and video.
	ret = no_depth ? 0 : freenect_start_depth(fn_dev);
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
	{
		freenect_frame_mode vm = freenect_get_current_video_mode(fn_dev);
		if (no_depth)
			printf("No depth; colour %dx%d\n", vm.width, vm.height);
		else
			printf("Depth %dx%d mm, range %s; colour %dx%d\n", depth_w, depth_h,
			       range == FREENECT_DEPTH_RANGE_FULL ? "full" : range == FREENECT_DEPTH_RANGE_NEAR ? "near" : "default",
			       vm.width, vm.height);
	}
	if (sync) {
		ret = freenect_set_stream_timing_auto(fn_dev, 1);
		printf("Automatic timing alignment: %s\n", ret == 0 ? "on" : "not available");
	}
	fflush(stdout);

	// Run until interruption, failure, or the requested duration.
	time_t start = time(NULL), last = start;
	while (!fn_exit_requested)
	{
		struct timeval tv = { 0, 100000 }; // wake up at least every 100 ms
		if (freenect_process_events_timeout(fn_ctx, &tv) < 0)
			break;
		time_t now = time(NULL);
		if (now != last) {
			printf("%3ld s: depth %2u, video %2u frames; depth mm: nearest %u, farthest %u, centre %u, %u%% of pixels valid\n",
			       (long)(now - start), sec_depth, sec_video, mm_min, mm_max, mm_centre,
			       (unsigned)(100.0 * mm_valid / (depth_w * depth_h)));
			fflush(stdout);
			sec_depth = sec_video = 0;
			last = now;
		}
		if (seconds > 0 && now - start >= seconds)
			break;
	}
	double elapsed = difftime(time(NULL), start);

	printf("Shutting down\n");
	if (elapsed > 0)
		printf("Frames in %.0f s: depth %u (%.1f/s), video %u (%.1f/s)\n",
		       elapsed, depth_frames, depth_frames / elapsed, video_frames, video_frames / elapsed);
	fflush(stdout);

	// Stop everything and shutdown.
	if (!no_depth)
		freenect_stop_depth(fn_dev);
	freenect_stop_video(fn_dev);
	freenect_close_device(fn_dev);
	freenect_shutdown(fn_ctx);

	printf("Done!\n");
	return fn_exit_done(0);
}
