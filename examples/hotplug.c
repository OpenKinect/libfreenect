/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2026 individual OpenKinect contributors. See the CONTRIB file
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

/*
 * freenect-hotplug: report Kinects being plugged in and out, and keep depth
 * streaming from one of them across unplugging and plugging in again.
 *
 * Usage: freenect-hotplug [SECONDS] [--no-stream]
 *   Runs until Ctrl+C (or for SECONDS). On arrival it opens the Kinect's
 *   camera and starts depth (unless --no-stream), and prints the frame rate
 *   once a second; on departure it closes the device and waits for the next
 *   arrival.
 */

#ifndef _WIN32
#define _DEFAULT_SOURCE /* clock_gettime, and usleep in console_exit.h */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libfreenect.h"
#include "console_exit.h"

#ifndef _WIN32
#include <time.h>
#endif

static unsigned long long now_ms(void)
{
#ifdef _WIN32
	return GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (unsigned long long)ts.tv_sec * 1000ULL + (unsigned long long)(ts.tv_nsec / 1000000);
#endif
}

static freenect_device *dev = NULL;
static char dev_serial[64];
static int stream = 1;
static int frames = 0;

static void depth_cb(freenect_device *d, void *depth, uint32_t timestamp)
{
	(void)d; (void)depth; (void)timestamp;
	frames++;
}

static void close_dev(void)
{
	if (!dev)
		return;
	if (freenect_device_connected(dev))
		freenect_stop_depth(dev);
	freenect_close_device(dev);
	dev = NULL;
	dev_serial[0] = '\0';
}

static void open_dev(freenect_context *ctx, const char *serial)
{
	if (dev)
		return; /* this example streams from one Kinect */
	if (!serial[0]) {
		printf("  cannot read its serial (another program may have it open); not opening it\n");
		return;
	}
	if (freenect_open_device_by_camera_serial(ctx, &dev, serial) < 0) {
		printf("  could not open it\n");
		dev = NULL;
		return;
	}
	snprintf(dev_serial, sizeof(dev_serial), "%s", serial);
	printf("  opened\n");
	if (stream) {
		freenect_set_depth_callback(dev, depth_cb);
		freenect_set_depth_mode(dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));
		if (freenect_start_depth(dev) < 0)
			printf("  could not start depth\n");
		else
			printf("  depth started\n");
	}
}

static void hotplug_cb(freenect_context *ctx, freenect_hotplug_event event, const char *serial, void *user)
{
	(void)user;
	if (event == FREENECT_HOTPLUG_ARRIVED) {
		printf("Kinect arrived: %s\n", serial[0] ? serial : "(serial unknown)");
		open_dev(ctx, serial);
	} else {
		printf("Kinect left: %s\n", serial[0] ? serial : "(serial unknown)");
		if (dev && !freenect_device_connected(dev)) {
			close_dev();
			printf("  closed; waiting for a Kinect\n");
		}
	}
	fflush(stdout);
}

int main(int argc, char **argv)
{
	freenect_context *ctx;
	int seconds = 0, i;
	unsigned long long start, last_report;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--no-stream") == 0)
			stream = 0;
		else if (argv[i][0] >= '0' && argv[i][0] <= '9')
			seconds = atoi(argv[i]);
		else {
			printf("Usage: %s [SECONDS] [--no-stream]\n"
			       "Reports Kinects being plugged in and out; streams depth from one of them.\n", argv[0]);
			return strcmp(argv[i], "--help") == 0 ? 0 : 1;
		}
	}

	fn_install_exit_handlers();
	if (freenect_init(&ctx, NULL) < 0) {
		fprintf(stderr, "freenect_init() failed\n");
		return 1;
	}
	freenect_set_log_level(ctx, FREENECT_LOG_NOTICE);
	freenect_select_subdevices(ctx, FREENECT_DEVICE_CAMERA);
	freenect_set_hotplug_callback(ctx, hotplug_cb, NULL);
	printf("Waiting for Kinects (Ctrl+C to stop)...\n");
	fflush(stdout);

	start = last_report = now_ms();
	while (!fn_exit_requested && (seconds == 0 || now_ms() - start < (unsigned long long)seconds * 1000ULL)) {
		struct timeval tv = { 0, 100000 };
		freenect_process_events_timeout(ctx, &tv);
		if (now_ms() - last_report >= 1000) {
			last_report = now_ms();
			if (dev && stream) {
				printf("  %s: %d depth frames in the last second\n", dev_serial, frames);
				fflush(stdout);
			}
			frames = 0;
		}
	}

	close_dev();
	freenect_shutdown(ctx);
	printf("Done.\n");
	return fn_exit_done(0);
}
