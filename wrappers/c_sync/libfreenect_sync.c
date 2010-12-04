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
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "libfreenect_sync.h"

typedef struct buffer_ring {
    pthread_mutex_t lock;
    pthread_cond_t cb_cond;
    void *bufs[3];
    uint32_t timestamp;
    int valid; // True if middle buffer is valid
    int fmt;
} buffer_ring_t;

typedef struct sync_kinect {
    freenect_device *dev;
    buffer_ring_t video;
    buffer_ring_t depth;
} sync_kinect_t;

#define MAX_KINECTS 64
static sync_kinect_t *kinects[MAX_KINECTS] = {};
static freenect_context *ctx;
static int thread_running = 0;
static pthread_t thread;
static pthread_mutex_t runloop_lock = PTHREAD_MUTEX_INITIALIZER;

int alloc_buffer_ring_video(freenect_video_format fmt, buffer_ring_t *buf) {
    int sz, i;
    switch (fmt) {
    case FREENECT_VIDEO_RGB:
	sz = FREENECT_VIDEO_RGB_SIZE;
	break;
    case FREENECT_VIDEO_BAYER:
	sz = FREENECT_VIDEO_BAYER_SIZE;
	break;
    case FREENECT_VIDEO_IR_8BIT:
	sz = FREENECT_VIDEO_IR_8BIT_SIZE;
	break;
    case FREENECT_VIDEO_IR_10BIT:
	sz = FREENECT_VIDEO_IR_10BIT;
	break;
    case FREENECT_VIDEO_IR_10BIT_PACKED:
	sz = FREENECT_VIDEO_IR_10BIT_PACKED_SIZE;
	break;
    default:
	printf("Invalid video format %d\n", fmt);
	return -1;
    }
    for (i = 0; i < 3; ++i)
	buf->bufs[i] = malloc(sz);
    buf->timestamp = 0;
    buf->valid = 0;
    buf->fmt = fmt;
    return 0;
}

int alloc_buffer_ring_depth(freenect_depth_format fmt, buffer_ring_t *buf) {
    int sz, i;
    switch (fmt) {
    case FREENECT_DEPTH_11BIT:
	sz = FREENECT_DEPTH_11BIT_SIZE;
	break;
    case FREENECT_DEPTH_10BIT:
	sz = FREENECT_DEPTH_10BIT_SIZE;
	break;
    case FREENECT_DEPTH_11BIT_PACKED:
	sz = FREENECT_DEPTH_11BIT_PACKED_SIZE;
	break;
    case FREENECT_DEPTH_10BIT_PACKED:
	sz = FREENECT_DEPTH_10BIT_PACKED_SIZE;
	break;
    default:
	printf("Invalid depth format %d\n", fmt);
	return -1;
    }
    for (i = 0; i < 3; ++i)
	buf->bufs[i] = malloc(sz);
    buf->timestamp = 0;
    buf->valid = 0;
    buf->fmt = fmt;
    return 0;
}

void free_buffer_ring(buffer_ring_t *buf) {
    int i;
    for (i = 0; i < 3; ++i) {
	free(buf->bufs[i]);
	buf->bufs[i] = NULL;
    }
    buf->timestamp = 0;
    buf->valid = 0;
    buf->fmt = -1;
}

sync_kinect_t *find_kinect(freenect_device *dev) {
    // This is a nasty hack until we can get the index from the dev
    int i;
    for (i = 0; i < MAX_KINECTS; ++i) {
	if (kinects[i] && kinects[i]->dev == dev)
	    return kinects[i];
    }
    printf("Error: Couldn't find kinect in find_kinect!\n");
    return NULL;
}

void video_producer_cb(freenect_device *dev, void *data, uint32_t timestamp) {
    sync_kinect_t *kinect = find_kinect(dev);
    pthread_mutex_lock(&kinect->video.lock);
    assert(data == kinect->video.bufs[2]);
    void *temp_buf = kinect->video.bufs[1];
    kinect->video.bufs[1] = kinect->video.bufs[2];
    kinect->video.bufs[2] = temp_buf;
    freenect_set_video_buffer(dev, temp_buf);
    kinect->video.timestamp = timestamp;
    kinect->video.valid = 1;
    pthread_cond_signal(&kinect->video.cb_cond);
    pthread_mutex_unlock(&kinect->video.lock);
}

void depth_producer_cb(freenect_device *dev, void *data, uint32_t timestamp) {
    sync_kinect_t *kinect = find_kinect(dev);
    pthread_mutex_lock(&kinect->depth.lock);
    assert(data == kinect->depth.bufs[2]);
    void *temp_buf = kinect->depth.bufs[1];
    kinect->depth.bufs[1] = kinect->depth.bufs[2];
    kinect->depth.bufs[2] = temp_buf;
    freenect_set_depth_buffer(dev, temp_buf);
    kinect->depth.timestamp = timestamp;
    kinect->depth.valid = 1;
    pthread_cond_signal(&kinect->depth.cb_cond);
    pthread_mutex_unlock(&kinect->depth.lock);
}

void *init(void *unused) {
    pthread_mutex_lock(&runloop_lock);
    while(thread_running && freenect_process_events(ctx) >= 0) {
	pthread_mutex_unlock(&runloop_lock);
	// NOTE: This lets other things run here
	pthread_mutex_lock(&runloop_lock);
    }
    // Go through each device, call stop video, close device
    int i;
    for (i = 0; i < MAX_KINECTS; ++i) {
	if (kinects[i]) {
	    freenect_stop_video(kinects[i]->dev);
	    freenect_stop_depth(kinects[i]->dev);
	    freenect_close_device(kinects[i]->dev);
	    free_buffer_ring(&kinects[i]->video);
	    free_buffer_ring(&kinects[i]->depth);
	    free(kinects[i]);
	    kinects[i] = NULL;
	}
    }
    freenect_shutdown(ctx);
    pthread_mutex_unlock(&runloop_lock);
    return NULL;
}

void init_thread() {
    thread_running = 1;
    freenect_init(&ctx, 0);
    pthread_create(&thread, NULL, init, NULL);
}

void change_video_format(sync_kinect_t *kinect, freenect_video_format fmt) {
    freenect_stop_video(kinect->dev);
    free_buffer_ring(&kinect->video);
    alloc_buffer_ring_video(fmt, &kinect->video);
    freenect_set_video_format(kinect->dev, fmt);
    freenect_set_video_buffer(kinect->dev, kinect->video.bufs[2]);
    freenect_start_video(kinect->dev);
}

void change_depth_format(sync_kinect_t *kinect, freenect_depth_format fmt) {
    freenect_stop_depth(kinect->dev);
    free_buffer_ring(&kinect->depth);
    alloc_buffer_ring_depth(fmt, &kinect->depth);
    freenect_set_depth_format(kinect->dev, fmt);
    freenect_set_depth_buffer(kinect->dev, kinect->depth.bufs[2]);
    freenect_start_depth(kinect->dev);
}

sync_kinect_t *alloc_kinect(int index) {
    sync_kinect_t *kinect = malloc(sizeof(sync_kinect_t));
    int i;
    for (i = 0; i < 3; ++i) {
	kinect->video.bufs[i] = NULL;
	kinect->depth.bufs[i] = NULL;
    }
    kinect->video.fmt = -1;
    kinect->depth.fmt = -1;
    freenect_open_device(ctx, &kinect->dev, index);
    freenect_set_video_callback(kinect->dev, video_producer_cb);
    freenect_set_depth_callback(kinect->dev, depth_producer_cb);
    pthread_mutex_init(&kinect->video.lock, NULL);
    pthread_mutex_init(&kinect->depth.lock, NULL);
    pthread_cond_init(&kinect->video.cb_cond, NULL);
    pthread_cond_init(&kinect->depth.cb_cond, NULL);
    return kinect;
}

int freenect_sync_get_video(void **video, uint32_t *timestamp, int index, freenect_video_format fmt) {
    if (index < 0 || index >= MAX_KINECTS) {
	printf("Error: Invalid index [%d]\n", index);
	return -1;
    }
    if (!thread_running || !kinects[index] || kinects[index]->video.fmt != fmt) {
	pthread_mutex_lock(&runloop_lock);
	if (!thread_running)
	    init_thread();
	if (!kinects[index])
	    kinects[index] = alloc_kinect(index);
	pthread_mutex_lock(&kinects[index]->video.lock);
	if (kinects[index]->video.fmt != fmt)
	    change_video_format(kinects[index], fmt);	    
	pthread_mutex_unlock(&kinects[index]->video.lock);
	pthread_mutex_unlock(&runloop_lock);
    }
    sync_kinect_t *kinect;
    kinect = kinects[index];
    pthread_mutex_lock(&kinect->video.lock);
    // If there isn't a frame ready for us
    while (!kinect->video.valid)
	pthread_cond_wait(&kinect->video.cb_cond, &kinect->video.lock);
    void *temp_buf = kinect->video.bufs[0];
    *video = kinect->video.bufs[0] = kinect->video.bufs[1];
    kinect->video.bufs[1] = temp_buf;
    kinect->video.valid = 0;
    *timestamp = kinect->video.timestamp;
    pthread_mutex_unlock(&kinect->video.lock);
    return 0;
}

int freenect_sync_get_depth(void **depth, uint32_t *timestamp, int index, freenect_depth_format fmt) {
    if (index < 0 || index >= MAX_KINECTS) {
	printf("Error: Invalid index [%d]\n", index);
	return -1;
    }
    if (!thread_running || !kinects[index] || kinects[index]->depth.fmt != fmt) {
	pthread_mutex_lock(&runloop_lock);
	if (!thread_running)
	    init_thread();
	if (!kinects[index])
	    kinects[index] = alloc_kinect(index);
	pthread_mutex_lock(&kinects[index]->depth.lock);
	if (kinects[index]->depth.fmt != fmt)
	    change_depth_format(kinects[index], fmt);	    
	pthread_mutex_unlock(&kinects[index]->depth.lock);
	pthread_mutex_unlock(&runloop_lock);
    }
    sync_kinect_t *kinect;
    kinect = kinects[index];
    pthread_mutex_lock(&kinect->depth.lock);
    // If there isn't a frame ready for us
    while (!kinect->depth.valid)
	pthread_cond_wait(&kinect->depth.cb_cond, &kinect->depth.lock);
    void *temp_buf = kinect->depth.bufs[0];
    *depth = kinect->depth.bufs[0] = kinect->depth.bufs[1];
    kinect->depth.bufs[1] = temp_buf;
    kinect->depth.valid = 0;
    *timestamp = kinect->depth.timestamp;
    pthread_mutex_unlock(&kinect->depth.lock);
    return 0;
}

/*
void freenect_sync_stop() {
    if (thread_running) {
	thread_running = 0;
	pthread_join(thread, NULL);
    }
}

*/
