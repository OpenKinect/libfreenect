/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2012 Robert Xiao <brx@cs.cmu.edu>
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

#include "proxynect.h"
#include <libfreenect.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define log(level, ...) do { if(device->loglevel >= FREENECT_LOG_##level) { fprintf(stderr, __VA_ARGS__); } } while(0)

volatile sig_atomic_t running = 1;

struct proxynect_device *device;

void depth_cb(freenect_device *dev, void *depth, uint32_t timestamp)
{
    int bufsel = !device->depth.bufsel;

    device->depth.frame_mode[bufsel] = freenect_get_current_depth_mode(dev);
    memcpy(device->depth.data[bufsel], depth, device->depth.frame_mode[bufsel].bytes);
    device->depth.bufsel = bufsel;

    device->depth.timestamp = timestamp;
}


void video_cb(freenect_device *dev, void *video, uint32_t timestamp)
{
    int bufsel = !device->video.bufsel;

    device->video.frame_mode[bufsel] = freenect_get_current_video_mode(dev);
    memcpy(device->video.data[bufsel], video, device->video.frame_mode[bufsel].bytes);
    device->video.bufsel = bufsel;

    device->video.timestamp = timestamp;
}

int run()
{
    freenect_context *ctx;
	freenect_device *dev;

	if(freenect_init(&ctx, 0)) {
	    log(FATAL, "Error: Failed to get context\n");
	    return 1;
	}

    freenect_set_log_level(ctx, device->loglevel);

    /* TODO: audio */
	freenect_select_subdevices(ctx, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));

    device->timestamp = 0;
    device->video.bufsel = 0;
    device->video.timestamp = 0;
    device->depth.bufsel = 0;
    device->depth.timestamp = 0;

	if(freenect_open_device(ctx, &dev, device->index)) {
	    log(FATAL, "Error: Failed to open device %d\n", device->index);
	    return 2;
	}

	freenect_set_depth_mode(dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));
	freenect_start_depth(dev);
	freenect_set_video_mode(dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
	freenect_start_video(dev);

	freenect_set_depth_callback(dev, depth_cb);
	freenect_set_video_callback(dev, video_cb);

	while (running && freenect_process_events(ctx) >= 0) {
	    if(device->settings.tilt_degs_changed) {
	        log(INFO, "Updating tilt degrees to %.1f\n", device->settings.tilt_degs);
	        freenect_set_tilt_degs(dev, device->settings.tilt_degs);
	        device->settings.tilt_degs_changed = 0;
	    }

        if(device->settings.led_changed) {
	        log(INFO, "Updating LED to %d\n", device->settings.led);
	        freenect_set_led(dev, device->settings.led);
	        device->settings.led_changed = 0;
	    }

        if(device->settings.video_mode_changed) {
            freenect_frame_mode curmode = freenect_get_current_video_mode(dev);
            if(!memcmp(&device->settings.video_mode, &curmode, sizeof(freenect_frame_mode))) {
                log(DEBUG, "Ignoring video format update request for %d\n", device->settings.video_mode.video_format);
            } else {
                log(INFO, "Updating video format to %d\n", device->settings.video_mode.video_format);
                freenect_stop_video(dev);
                freenect_set_video_mode(dev, device->settings.video_mode);
                freenect_start_video(dev);
            }
	        device->settings.video_mode_changed = 0;
	    }

        if(device->settings.depth_mode_changed) {
	        freenect_frame_mode curmode = freenect_get_current_depth_mode(dev);
            if(!memcmp(&device->settings.depth_mode, &curmode, sizeof(freenect_frame_mode))) {
                log(DEBUG, "Ignoring depth format update request for %d\n", device->settings.depth_mode.depth_format);
            } else {
                log(INFO, "Updating depth format to %d\n", device->settings.depth_mode.depth_format);
                freenect_stop_depth(dev);
                freenect_set_depth_mode(dev, device->settings.depth_mode);
                freenect_start_depth(dev);
            }
	        device->settings.depth_mode_changed = 0;
	    }

        if(device->settings.flags_changed) {
            int i;
            for(i=0; i<sizeof(int)*8; i++) {
                int flag = 1<<i;
                if(device->settings.flags_changed & flag) {
                    device->settings.flags_changed &= ~flag;
                    freenect_set_flag(dev, flag, (device->settings.flags & flag) ? FREENECT_ON : FREENECT_OFF);
                }
            }
        }

	    freenect_update_tilt_state(dev);
	    device->raw_state = *freenect_get_tilt_state(dev);
	    device->timestamp++;
	}

	freenect_stop_depth(dev);
	freenect_stop_video(dev);
	freenect_close_device(dev);
	freenect_shutdown(ctx);

    return 0;
}

void signal_cleanup(int num)
{
	running = 0;
	log(NOTICE, "Caught signal, cleaning up\n");
	signal(SIGINT, signal_cleanup);
}

void usage()
{
	fprintf(stderr, "Opens a device for proxynect usage.\n");
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  proxynect [--help] [--force] [--loglevel n] [device-index]\n");
	exit(1);
}

int main(int argc, char **argv)
{
    int c = 1;
    int force = 0;
    int loglevel = 0;
    int index = 0;
    device = NULL;

    while(c < argc) {
        if(!strcmp(argv[c], "--help")) {
            usage();
        } else if(!strcmp(argv[c], "--force")) {
            force = 1;
        } else if(!strcmp(argv[c], "--loglevel")) {
            loglevel = atoi(argv[c+1]);
            c++;
        } else if(argv[c][0] == '-') {
            fprintf(stderr, "Unrecognized option %s; use --help for help.\n", argv[c]);
            exit(1);
        } else {
            index = atoi(argv[c]);
        }
        c++;
    }

    if(index < 0) {
        fprintf(stderr, "Error: Invalid device index %d\n", index);
        return 1;
    }

    device = create_device(index, force);
    if(device == NULL) {
        perror("Failed to create shared device");
        return 1;
    }

    device->index = index;
    device->loglevel = loglevel;

	signal(SIGINT, signal_cleanup);

    int ret = run();

    destroy_device(device);

    return ret;
}
