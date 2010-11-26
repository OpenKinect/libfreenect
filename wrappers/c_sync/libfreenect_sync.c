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
char depth_img[DEPTH_BYTES];
char rgb_img[RGB_BYTES];
int thread_running = 0;
pthread_t thread;

void depth_producer_cb(freenect_device *dev, void *depth, uint32_t timestamp) {
    pthread_mutex_lock(&depth_mutex);
    memcpy(depth_img, depth, DEPTH_BYTES);
    pthread_cond_signal(&depth_cond);
    pthread_mutex_unlock(&depth_mutex);
}


void rgb_producer_cb(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp) {
    pthread_mutex_lock(&rgb_mutex);
    memcpy(rgb_img, rgb, RGB_BYTES);
    pthread_cond_signal(&rgb_cond);
    pthread_mutex_unlock(&rgb_mutex);
}

void *init(void *unused) {
    freenect_context *ctx;
    freenect_device *dev;
    freenect_init(&ctx, 0);
    freenect_open_device(ctx, &dev, 0);
    freenect_set_depth_format(dev, 0);
    freenect_start_depth(dev);
    freenect_set_rgb_format(dev, FREENECT_FORMAT_RGB);
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


char *freenect_get_depth() {
    char *depth_out = malloc(DEPTH_BYTES);
    if (!thread_running)
	init_thread();
    pthread_mutex_lock(&depth_mutex);
    pthread_cond_wait(&depth_cond, &depth_mutex);
    memcpy(depth_out, depth_img, DEPTH_BYTES);
    pthread_mutex_unlock(&depth_mutex);
    return depth_out;
}


char *freenect_get_rgb() {
    char *rgb_out = malloc(RGB_BYTES);
    if (!thread_running)
	init_thread();
    pthread_mutex_lock(&rgb_mutex);
    pthread_cond_wait(&rgb_cond, &rgb_mutex);
    memcpy(rgb_out, rgb_img, RGB_BYTES);
    pthread_mutex_unlock(&rgb_mutex);
    return rgb_out;
}

void freenect_sync_stop() {
    if (thread_running) {
	thread_running = 0;
	pthread_join(thread, NULL);
    }
}
