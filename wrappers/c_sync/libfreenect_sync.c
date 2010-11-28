/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 Brandyn White (bwhite@dappervision.com)
 *                    Andrew Miller (amiller@dappervision.com)
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

#include <libfreenect.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include "libfreenect_sync.h"

pthread_mutex_t rgb_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t depth_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t rgb_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t depth_cond = PTHREAD_COND_INITIALIZER;
char depth_img[FREENECT_DEPTH_11BIT_SIZE];
char rgb_img[FREENECT_VIDEO_RGB_SIZE];
uint32_t depth_timestamp;
uint32_t rgb_timestamp;
int thread_running = 0;
pthread_t thread;

void depth_producer_cb(freenect_device *dev, void *depth, uint32_t timestamp) {
    pthread_mutex_lock(&depth_mutex);
    memcpy(depth_img, depth, FREENECT_DEPTH_11BIT_SIZE);
    depth_timestamp = timestamp;
    pthread_cond_signal(&depth_cond);
    pthread_mutex_unlock(&depth_mutex);
}

void rgb_producer_cb(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp) {
    pthread_mutex_lock(&rgb_mutex);
    memcpy(rgb_img, rgb, FREENECT_VIDEO_RGB_SIZE);
    rgb_timestamp = timestamp;
    pthread_cond_signal(&rgb_cond);
    pthread_mutex_unlock(&rgb_mutex);
}

void *init(void *unused) {
    freenect_context *ctx;
    freenect_device *dev;
    freenect_init(&ctx, 0);
    freenect_open_device(ctx, &dev, 0);
    freenect_set_depth_format(dev, FREENECT_DEPTH_11BIT);
    freenect_start_depth(dev);
    freenect_set_rgb_format(dev, FREENECT_VIDEO_RGB);
    freenect_start_rgb(dev);
    freenect_set_depth_callback(dev, depth_producer_cb);
    freenect_set_rgb_callback(dev, rgb_producer_cb);
    while(thread_running && freenect_process_events(ctx) >= 0)
	{}
    freenect_stop_depth(dev);
    freenect_stop_rgb(dev);
    freenect_close_device(dev);
    freenect_shutdown(ctx);
    return NULL;
}

void init_thread() {
    thread_running = 1;
    pthread_create(&thread, NULL, init, NULL);
}

int freenect_sync_get_depth(char **depth, uint32_t *timestamp) {
    *depth = malloc(FREENECT_DEPTH_11BIT_SIZE);
    if (!thread_running)
	init_thread();
    pthread_mutex_lock(&depth_mutex);
    pthread_cond_wait(&depth_cond, &depth_mutex);
    memcpy(*depth, depth_img, FREENECT_DEPTH_11BIT_SIZE);
    *timestamp = depth_timestamp;
    pthread_mutex_unlock(&depth_mutex);
    return 0;
}

int freenect_sync_get_rgb(char **rgb, uint32_t *timestamp) {
    *rgb = malloc(FREENECT_VIDEO_RGB_SIZE);
    if (!thread_running)
	init_thread();
    pthread_mutex_lock(&rgb_mutex);
    pthread_cond_wait(&rgb_cond, &rgb_mutex);
    memcpy(*rgb, rgb_img, FREENECT_VIDEO_RGB_SIZE);
    *timestamp = rgb_timestamp;
    pthread_mutex_unlock(&rgb_mutex);
    return 0;
}

void freenect_sync_stop() {
    if (thread_running) {
	thread_running = 0;
	pthread_join(thread, NULL);
    }
}
