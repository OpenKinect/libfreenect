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

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#define GRAVITY 9.80665

#define MAX_DEVICES 100

/* copied from cameras.c */
#define MAKE_RESERVED(res, fmt) (uint32_t)(((res & 0xff) << 8) | (((fmt & 0xff))))
#define RESERVED_TO_RESOLUTION(reserved) (freenect_resolution)((reserved >> 8) & 0xff)
#define RESERVED_TO_FORMAT(reserved) ((reserved) & 0xff)

#define video_mode_count 12
static freenect_frame_mode supported_video_modes[video_mode_count] = {
	// reserved, resolution, format, bytes, width, height, data_bits_per_pixel, padding_bits_per_pixel, framerate, is_valid
	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_RGB), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_RGB}, 1280*1024*3, 1280, 1024, 24, 0, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_RGB}, 640*480*3, 640,  480, 24, 0, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_BAYER), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_BAYER}, 1280*1024, 1280, 1024, 8, 0, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_BAYER), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_BAYER}, 640*480, 640, 480, 8, 0, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_IR_8BIT), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_IR_8BIT}, 1280*1024, 1280, 1024, 8, 0, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_8BIT), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_IR_8BIT}, 640*488, 640, 488, 8, 0, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_IR_10BIT), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_IR_10BIT}, 1280*1024*2, 1280, 1024, 10, 6, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_10BIT), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_IR_10BIT}, 640*488*2, 640, 488, 10, 6, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_IR_10BIT_PACKED), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_IR_10BIT_PACKED}, 1280*1024*10/8, 1280, 1024, 10, 0, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_10BIT_PACKED), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_IR_10BIT_PACKED}, 640*488*10/8, 640, 488, 10, 0, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_YUV_RGB), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_YUV_RGB}, 640*480*3, 640, 480, 24, 0, 15, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_YUV_RAW), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_YUV_RAW}, 640*480*2, 640, 480, 16, 0, 15, 1 },
};

#define depth_mode_count 6
static freenect_frame_mode supported_depth_modes[depth_mode_count] = {
	// reserved, resolution, format, bytes, width, height, data_bits_per_pixel, padding_bits_per_pixel, framerate, is_valid
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_11BIT}, 640*480*2, 640, 480, 11, 5, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_10BIT}, 640*480*2, 640, 480, 10, 6, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT_PACKED), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_11BIT_PACKED}, 640*480*11/8, 640, 480, 11, 0, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT_PACKED), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_10BIT_PACKED}, 640*480*10/8, 640, 480, 10, 0, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_REGISTERED), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_REGISTERED}, 640*480*2, 640, 480, 16, 0, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_MM}, 640*480*2, 640, 480, 16, 0, 30, 1},
};

struct _freenect_context {
    int num_devices;
    freenect_device *first;
};

struct _freenect_device {
    freenect_context *parent;
    freenect_device *prev;
    freenect_device *next;

    struct proxynect_device *device;

    void *user_data;

    uint32_t last_timestamp;

    freenect_video_cb video_cb;
    uint8_t *video_buf;
    uint32_t last_video;
    int video_running;

    freenect_depth_cb depth_cb;
    uint8_t *depth_buf;
    uint32_t last_depth;
    int depth_running;
};


int freenect_init(freenect_context **ctx, freenect_usb_context *usb_ctx) {
    freenect_context *newctx = (freenect_context *)malloc(sizeof(freenect_context));
    if(newctx == NULL)
        return -1;

    newctx->num_devices = 0;
    newctx->first = NULL;

    *ctx = newctx;

    return 0;
}

int freenect_shutdown(freenect_context *ctx) {
    while(ctx->first != NULL) {
        freenect_close_device(ctx->first);
    }

    free(ctx);
    return 0;
}

/* Logging not supported here (daemon does all the logging) */
void freenect_set_log_level(freenect_context *ctx, freenect_loglevel level) {}
void freenect_set_log_callback(freenect_context *ctx, freenect_log_cb cb) {}

int freenect_process_events(freenect_context *ctx) {
    struct timeval timeout;
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;

    return freenect_process_events_timeout(ctx, &timeout);
}

void timeval_add(struct timeval *t1, struct timeval *t2) {
    /* Add t2 to t1 */
    t1->tv_usec += t2->tv_usec;
    if(t1->tv_usec >= 1000000) {
        t1->tv_usec -= 1000000;
        t1->tv_sec += 1;
    }
    t1->tv_sec += t2->tv_sec;
}

int timeval_cmp(struct timeval *t1, struct timeval *t2) {
    if(t1->tv_sec > t2->tv_sec)
        return 1;
    else if(t1->tv_sec < t2->tv_sec)
        return -1;
    else
        return t1->tv_usec - t2->tv_usec;
}

static void update_device(freenect_device *dev) {
    if(dev->last_video != dev->device->video.timestamp) {
        int bufsel = dev->device->video.bufsel;
        uint8_t *video_buf = dev->device->video.data[bufsel];
        freenect_frame_mode *video_format = &dev->device->video.frame_mode[bufsel];

        if(dev->video_buf != NULL) {
            memcpy(dev->video_buf, video_buf, video_format->bytes);
            video_buf = dev->video_buf;
        }

        dev->last_video = dev->device->video.timestamp;

        if(dev->video_cb != NULL) {
            dev->video_cb(dev, video_buf, dev->last_video);
        }
    }

    if(dev->last_depth != dev->device->depth.timestamp) {
        int bufsel = dev->device->depth.bufsel;
        uint8_t *depth_buf = dev->device->depth.data[bufsel];
        freenect_frame_mode *depth_format = &dev->device->depth.frame_mode[bufsel];

        if(dev->depth_buf != NULL) {
            memcpy(dev->depth_buf, depth_buf, depth_format->bytes);
            depth_buf = dev->depth_buf;
        }

        dev->last_depth = dev->device->depth.timestamp;

        if(dev->depth_cb != NULL) {
            dev->depth_cb(dev, depth_buf, dev->last_depth);
        }
    }
}

int freenect_process_events_timeout(freenect_context *ctx, struct timeval* timeout) {
    struct timeval end, cur;
    gettimeofday(&end, NULL);
    timeval_add(&end, timeout);

    while(1) {
        gettimeofday(&cur, NULL);
        if(timeval_cmp(&cur, &end) >= 0)
            break;

        freenect_device *dev;
        int updated = 0;
        for(dev = ctx->first; dev != NULL; dev = dev->next) {            
            if(dev->last_timestamp != dev->device->timestamp) {
                updated = 1;
                update_device(dev);
                dev->last_timestamp = dev->device->timestamp;
            }
        }
        if(updated)
            return 0;
        usleep(1000);
    }
    return 0;
}

int freenect_num_devices(freenect_context *ctx) {
    int i;
    for(i=0; i<MAX_DEVICES; i++) {
        struct proxynect_device *device = open_device(i);
        if(device == NULL)
            break;
        close_device(device);
    }

    return i;
}

int freenect_list_device_attributes(freenect_context *ctx, struct freenect_device_attributes** attribute_list) {
    /* XXX unsupported; use freenect_num_devices instead */
    return -1;
}

void freenect_free_device_attributes(struct freenect_device_attributes* attribute_list) {
    /* nothing */
}

int freenect_supported_subdevices(void) {
    /* TODO: audio */
    return FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA;
}

void freenect_select_subdevices(freenect_context *ctx, freenect_device_flags subdevs) {
    /* Do nothing: the device has already been opened by the daemon anyway */
}

int freenect_open_device(freenect_context *ctx, freenect_device **dev, int index) {
    freenect_device *newdev = (freenect_device *)malloc(sizeof(freenect_device));
    newdev->parent = ctx;

    newdev->device = open_device(index);
    if(newdev->device == NULL) {
        perror("opening device");
        free(newdev);
        return -1;
    }

    newdev->next = ctx->first;
    newdev->prev = NULL;
    if(ctx->first != NULL) {
        ctx->first->prev = newdev;
    }
    ctx->first = newdev;
    newdev->user_data = NULL;

    newdev->last_timestamp = 0;

    newdev->video_cb = NULL;
    newdev->video_buf = NULL;
    newdev->last_video = 0;
    newdev->video_running = 0;

    newdev->depth_cb = NULL;
    newdev->depth_buf = NULL;
    newdev->last_depth = 0;
    newdev->depth_running = 0;

    *dev = newdev;
    return 0;
}

int freenect_open_device_by_camera_serial(freenect_context *ctx, freenect_device **dev, const char* camera_serial) {
    /* XXX unsupported */
    return -1;
}

int freenect_close_device(freenect_device *dev) {
    close_device(dev->device);
    if(dev->next != NULL) {
        dev->next->prev = dev->prev;
    }

    if(dev->prev != NULL) {
        dev->prev->next = dev->next;
    } else {
        dev->parent->first = dev->next;
    }

    free(dev);
    return 0;
}

void freenect_set_user(freenect_device *dev, void *user) {
    dev->user_data = user;
}

void *freenect_get_user(freenect_device *dev) {
    return dev->user_data;
}

void freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb) {
    dev->depth_cb = cb;
}

void freenect_set_video_callback(freenect_device *dev, freenect_video_cb cb) {
    dev->video_cb = cb;
}

int freenect_set_depth_buffer(freenect_device *dev, void *buf) {
    dev->depth_buf = buf;
    return 0;
}

int freenect_set_video_buffer(freenect_device *dev, void *buf) {
    dev->video_buf = buf;
    return 0;
}

int freenect_start_depth(freenect_device *dev) {
    dev->depth_running = 1;
    return 0;
}

int freenect_start_video(freenect_device *dev) {
    dev->video_running = 1;
    return 0;
}

int freenect_stop_depth(freenect_device *dev) {
    dev->depth_running = 0;
    return 0;
}

int freenect_stop_video(freenect_device *dev) {
    dev->video_running = 0;
    return 0;
}

int freenect_update_tilt_state(freenect_device *dev) {
    /* Accelerometer data is always considered up-to-date */
    return 0;
}

freenect_raw_tilt_state* freenect_get_tilt_state(freenect_device *dev) {
    return &dev->device->raw_state;
}

double freenect_get_tilt_degs(freenect_raw_tilt_state *state) {
    return ((double)state->tilt_angle) / 2.0;
}

int freenect_set_tilt_degs(freenect_device *dev, double angle) {
    dev->device->settings.tilt_degs = angle;
    dev->device->settings.tilt_degs_changed = 1;
    return 0;
}

freenect_tilt_status_code freenect_get_tilt_status(freenect_raw_tilt_state *state) {
    return state->tilt_status;
}

int freenect_set_led(freenect_device *dev, freenect_led_options option) {
    dev->device->settings.led = option;
    dev->device->settings.led_changed = 1;
    return 0;
}

void freenect_get_mks_accel(freenect_raw_tilt_state *state, double* x, double* y, double* z) {
	*x = (double)state->accelerometer_x/FREENECT_COUNTS_PER_G*GRAVITY;
	*y = (double)state->accelerometer_y/FREENECT_COUNTS_PER_G*GRAVITY;
	*z = (double)state->accelerometer_z/FREENECT_COUNTS_PER_G*GRAVITY;
}

int freenect_get_video_mode_count() {
	return video_mode_count;
}

freenect_frame_mode freenect_get_video_mode(int mode_num) {
	if (mode_num >= 0 && mode_num < video_mode_count)
		return supported_video_modes[mode_num];
	freenect_frame_mode retval;
	retval.is_valid = 0;
	return retval;
}

freenect_frame_mode freenect_get_current_video_mode(freenect_device *dev) {
    return dev->device->video.frame_mode[dev->device->video.bufsel];
}

freenect_frame_mode freenect_find_video_mode(freenect_resolution res, freenect_video_format fmt) {
	uint32_t unique_id = MAKE_RESERVED(res, fmt);
	int i;
	for(i = 0 ; i < video_mode_count; i++) {
		if (supported_video_modes[i].reserved == unique_id)
			return supported_video_modes[i];
	}
	freenect_frame_mode retval;
	retval.is_valid = 0;
	return retval;
}

int freenect_set_video_mode(freenect_device* dev, freenect_frame_mode mode) {
    dev->device->settings.video_mode = mode;
    dev->device->settings.video_mode_changed = 1;
    return 0;
}

int freenect_get_depth_mode_count() {
	return depth_mode_count;
}

freenect_frame_mode freenect_get_depth_mode(int mode_num) {
	if (mode_num >= 0 && mode_num < depth_mode_count)
		return supported_depth_modes[mode_num];
	freenect_frame_mode retval;
	retval.is_valid = 0;
	return retval;
}

freenect_frame_mode freenect_get_current_depth_mode(freenect_device *dev) {
    return dev->device->depth.frame_mode[dev->device->depth.bufsel];
}

freenect_frame_mode freenect_find_depth_mode(freenect_resolution res, freenect_depth_format fmt) {
	uint32_t unique_id = MAKE_RESERVED(res, fmt);
	int i;
	for(i = 0 ; i < depth_mode_count; i++) {
		if (supported_depth_modes[i].reserved == unique_id)
			return supported_depth_modes[i];
	}
	freenect_frame_mode retval;
	retval.is_valid = 0;
	return retval;
}

int freenect_set_depth_mode(freenect_device* dev, const freenect_frame_mode mode) {
    dev->device->settings.depth_mode = mode;
    dev->device->settings.depth_mode_changed = 1;
    return 0;
}
