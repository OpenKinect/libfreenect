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
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <thread>
#include <mutex>
#include "libfreenect_registration.h"
#include "libfreenect_sync.h"

using std::cerr;
using std::endl;
using Mutex = std::mutex;
using UniqueLock = std::unique_lock<Mutex>;
using LockGuard = std::lock_guard<Mutex>;
using ConditionVariable = std::condition_variable;
using Thread = std::thread;

namespace
{
	void FN_WARN_INVALID_INDEX(const int index)
	{
		cerr << "Error: Invalid index [" << index << "]" << endl;
	}
}


typedef struct buffer_ring {
	Mutex mutex;
	ConditionVariable cb_cond;
	void *bufs[3];
	uint32_t timestamp;
	bool valid; // True if middle buffer is valid
	int fmt;
	int res;
} buffer_ring_t;

typedef struct sync_kinect {
	freenect_device *dev;
	buffer_ring_t video;
	buffer_ring_t depth;
} sync_kinect_t;

struct AutomaticShutdownThread : public Thread
{
	using Thread::thread;

	virtual ~AutomaticShutdownThread()
	{
		freenect_sync_stop();
	}

	AutomaticShutdownThread& operator=(Thread&& other)
	{
		Thread::operator=(std::move(other));
		return *this;
	}
};

typedef int (*set_buffer_t)(freenect_device *dev, void *buf);

#define MAX_KINECTS 64
static sync_kinect_t *kinects[MAX_KINECTS] = { 0 };
static freenect_context *ctx;
static bool thread_running = false;
static AutomaticShutdownThread thread;
static Mutex runloop_mutex;
static int pending_runloop_tasks = 0;
static Mutex pending_runloop_tasks_mutex;
static ConditionVariable pending_runloop_tasks_cond;

/* Lock Families:
       - pending_runloop_tasks_mutex
       - runloop_mutex, buffer_ring_t.mutex (NOTE: You may only have one)
*/

static int alloc_buffer_ring_video(freenect_resolution res, freenect_video_format fmt, buffer_ring_t *buf)
{
	int size;
	switch (fmt) {
		case FREENECT_VIDEO_RGB:
		case FREENECT_VIDEO_BAYER:
		case FREENECT_VIDEO_IR_8BIT:
		case FREENECT_VIDEO_IR_10BIT:
		case FREENECT_VIDEO_IR_10BIT_PACKED:
			size = freenect_find_video_mode(res, fmt).bytes;
			break;
		default:
			cerr << "Invalid video format " << fmt << endl;
			return -1;
	}
	for (short i = 0; i < 3; ++i) {
		buf->bufs[i] = malloc(size);
	}
	buf->timestamp = 0;
	buf->valid = false;
	buf->fmt = fmt;
	buf->res = res;
	return 0;
}

static int alloc_buffer_ring_depth(freenect_resolution res, freenect_depth_format fmt, buffer_ring_t *buf)
{
	int size;
	switch (fmt) {
		case FREENECT_DEPTH_11BIT:
		case FREENECT_DEPTH_10BIT:
		case FREENECT_DEPTH_11BIT_PACKED:
		case FREENECT_DEPTH_10BIT_PACKED:
		case FREENECT_DEPTH_REGISTERED:
		case FREENECT_DEPTH_MM:
			size = freenect_find_depth_mode(res, fmt).bytes;
			break;
		default:
			cerr << "Invalid depth format " << fmt << endl;
			return -1;
	}
	for (short i = 0; i < 3; ++i) {
		buf->bufs[i] = malloc(size);
	}
	buf->timestamp = 0;
	buf->valid = false;
	buf->fmt = fmt;
	buf->res = res;
	return 0;
}

static void free_buffer_ring(buffer_ring_t *buf)
{
	for (short i = 0; i < 3; ++i) {
		free(buf->bufs[i]);
		buf->bufs[i] = nullptr;
	}
	buf->timestamp = 0;
	buf->valid = false;
	buf->fmt = -1;
	buf->res = -1;
}

static void producer_cb_inner(freenect_device *dev, void *data, uint32_t timestamp, buffer_ring_t *buf, set_buffer_t set_buffer)
{
	UniqueLock buffer_lock(buf->mutex);
	assert(data == buf->bufs[2]);
	void *temp_buf = buf->bufs[1];
	buf->bufs[1] = buf->bufs[2];
	buf->bufs[2] = temp_buf;
	set_buffer(dev, temp_buf);
	buf->timestamp = timestamp;
	buf->valid = true;
	buf->cb_cond.notify_one();
}

static void video_producer_cb(freenect_device *dev, void *data, uint32_t timestamp)
{
	producer_cb_inner(dev, data, timestamp, &((sync_kinect_t *)freenect_get_user(dev))->video, freenect_set_video_buffer);
}

static void depth_producer_cb(freenect_device *dev, void *data, uint32_t timestamp)
{
	producer_cb_inner(dev, data, timestamp, &((sync_kinect_t *)freenect_get_user(dev))->depth, freenect_set_depth_buffer);
}

/* You should only use these functions to manipulate the pending_runloop_tasks_mutex*/
static void pending_runloop_tasks_inc(void)
{
	LockGuard lock(pending_runloop_tasks_mutex);
	assert(pending_runloop_tasks >= 0);
	++pending_runloop_tasks;
}

static void pending_runloop_tasks_dec(void)
{
	LockGuard lock(pending_runloop_tasks_mutex);
	--pending_runloop_tasks;
	assert(pending_runloop_tasks >= 0);
	if (!pending_runloop_tasks) {
		pending_runloop_tasks_cond.notify_one();
	}
}

static void pending_runloop_tasks_wait_zero(void)
{
	UniqueLock lock(pending_runloop_tasks_mutex);
	while (pending_runloop_tasks) {
		pending_runloop_tasks_cond.wait(lock);
	}
}

static void runloop()
{
	pending_runloop_tasks_wait_zero();
	UniqueLock runloop_lock(runloop_mutex);

	while (thread_running && freenect_process_events(ctx) >= 0) {
		// NOTE: This lets you run tasks while process_events isn't running
		runloop_lock.unlock();
		pending_runloop_tasks_wait_zero();
		runloop_lock.lock();
	}
	// Go through each device, call stop video, close device
	for (short i = 0; i < MAX_KINECTS; ++i) {
		if (kinects[i]) {
			freenect_stop_video(kinects[i]->dev);
			freenect_stop_depth(kinects[i]->dev);
			freenect_set_user(kinects[i]->dev, nullptr);
			freenect_close_device(kinects[i]->dev);
			free_buffer_ring(&kinects[i]->video);
			free_buffer_ring(&kinects[i]->depth);
			free(kinects[i]);
			kinects[i] = nullptr;
		}
	}
	freenect_shutdown(ctx);
}

static void init_thread(void)
{
	assert(!thread_running);
	thread_running = true;
	freenect_init(&ctx, 0);
	// We claim both the motor and the camera, because we can't know in advance
	// which devices the caller will want, and the c_sync interface doesn't
	// support audio, so there's no reason to claim the device needlessly.
	freenect_select_subdevices(ctx, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));
	thread = Thread(runloop);
}

static int change_video_format(sync_kinect_t *kinect, freenect_resolution res, freenect_video_format fmt)
{
	freenect_stop_video(kinect->dev);
	free_buffer_ring(&kinect->video);
	if (alloc_buffer_ring_video(res, fmt, &kinect->video))
		return -1;
	freenect_set_video_mode(kinect->dev, freenect_find_video_mode(res, fmt));
	freenect_set_video_buffer(kinect->dev, kinect->video.bufs[2]);
	freenect_start_video(kinect->dev);
	return 0;
}

static int change_depth_format(sync_kinect_t *kinect, freenect_resolution res, freenect_depth_format fmt)
{
	freenect_stop_depth(kinect->dev);
	free_buffer_ring(&kinect->depth);
	if (alloc_buffer_ring_depth(res, fmt, &kinect->depth))
		return -1;
	freenect_set_depth_mode(kinect->dev, freenect_find_depth_mode(res, fmt));
	freenect_set_depth_buffer(kinect->dev, kinect->depth.bufs[2]);
	freenect_start_depth(kinect->dev);
	return 0;
}

static sync_kinect_t *alloc_kinect(int index)
{
	sync_kinect_t* kinect = new sync_kinect_t();
	if (freenect_open_device(ctx, &kinect->dev, index)) {
		delete kinect;
		return nullptr;
	}
	for (short i = 0; i < 3; ++i) {
		kinect->video.bufs[i] = nullptr;
		kinect->depth.bufs[i] = nullptr;
	}
	kinect->video.fmt = -1;
	kinect->video.res = -1;
	kinect->depth.fmt = -1;
	kinect->depth.res = -1;
	freenect_set_video_callback(kinect->dev, video_producer_cb);
	freenect_set_depth_callback(kinect->dev, depth_producer_cb);
	return kinect;
}

static int setup_kinect(int index, int res, int fmt, int is_depth)
{
	pending_runloop_tasks_inc();
	UniqueLock runloop_lock(runloop_mutex);

	bool thread_running_prev = thread_running;
	if (!thread_running)
		init_thread();
	if (!kinects[index]) {
		kinects[index] = alloc_kinect(index);
	}
	if (!kinects[index]) {
		FN_WARN_INVALID_INDEX(index);
		// If we started the thread, we need to bring it back
		if (!thread_running_prev) {
			thread_running = false;
			runloop_lock.unlock();
			pending_runloop_tasks_dec();
			thread.join();
		} else {
			runloop_lock.unlock();
			pending_runloop_tasks_dec();
		}
		return -1;
	}
	freenect_set_user(kinects[index]->dev, kinects[index]);
	buffer_ring_t *buf = is_depth ? &kinects[index]->depth : &kinects[index]->video;

	UniqueLock buffer_lock(buf->mutex);
	if ((buf->fmt != fmt) || (buf->res != res))
	{
		if (is_depth)
			change_depth_format(kinects[index], (freenect_resolution)res, (freenect_depth_format)fmt);
		else
			change_video_format(kinects[index], (freenect_resolution)res, (freenect_video_format)fmt);
	}
	buffer_lock.unlock();
	pending_runloop_tasks_dec();
	return 0;
}

static int sync_get(void **data, uint32_t *timestamp, buffer_ring_t *buf)
{
	UniqueLock buffer_lock(buf->mutex);
	// If there isn't a frame ready for us
	while (!buf->valid) {
		buf->cb_cond.wait(buffer_lock);
	}
	void *temp_buf = buf->bufs[0];
	*data = buf->bufs[0] = buf->bufs[1];
	buf->bufs[1] = temp_buf;
	buf->valid = false;
	*timestamp = buf->timestamp;
	return 0;
}


/*
  Use this to make sure the runloop is locked and no one is in it. Then you can
  call arbitrary functions from libfreenect.h in a safe way. If the kinect with
  this index has not been initialized yet, then it will try to set it up. If
  this function is successful, then you can access kinects[index]. Don't forget
  to unlock the runloop when you're done.

  Returns 0 if successful, nonzero if kinect[index] is unvailable
 */
static int runloop_enter(int index)
{
	if (index < 0 || index >= MAX_KINECTS) {
		FN_WARN_INVALID_INDEX(index);
		return -1;
	}
	if (!thread_running || !kinects[index])
		if (setup_kinect(index, FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT, 1))
			return -1;

	pending_runloop_tasks_inc();
	runloop_mutex.lock();
	return 0;
}

static void runloop_exit()
{
	runloop_mutex.unlock();
	pending_runloop_tasks_dec();
}

int freenect_sync_get_video_with_res(void **video, uint32_t *timestamp, int index,
        freenect_resolution res, freenect_video_format fmt)
{
	if (index < 0 || index >= MAX_KINECTS) {
		FN_WARN_INVALID_INDEX(index);
		return -1;
	}
	if (!thread_running || !kinects[index] || kinects[index]->video.fmt != fmt || kinects[index]->video.res != res)
		if (setup_kinect(index, res, fmt, 0))
			return -1;
	sync_get(video, timestamp, &kinects[index]->video);
	return 0;
}

int freenect_sync_get_video(void **video, uint32_t *timestamp, int index, freenect_video_format fmt)
{
    return freenect_sync_get_video_with_res(video, timestamp, index, FREENECT_RESOLUTION_MEDIUM, fmt);
}

int freenect_sync_get_depth_with_res(void **depth, uint32_t *timestamp, int index,
        freenect_resolution res, freenect_depth_format fmt)
{
	if (index < 0 || index >= MAX_KINECTS) {
		FN_WARN_INVALID_INDEX(index);
		return -1;
	}
	if (!thread_running || !kinects[index] || kinects[index]->depth.fmt != fmt
            || kinects[index]->depth.res != res)
		if (setup_kinect(index, res, fmt, 1))
			return -1;
	sync_get(depth, timestamp, &kinects[index]->depth);
	return 0;
}

int freenect_sync_get_depth(void **depth, uint32_t *timestamp, int index, freenect_depth_format fmt)
{
    return freenect_sync_get_depth_with_res(depth, timestamp, index, FREENECT_RESOLUTION_MEDIUM, fmt);
}

int freenect_sync_get_tilt_state(freenect_raw_tilt_state **state, int index)
{
	if (runloop_enter(index)) return -1;
	freenect_update_tilt_state(kinects[index]->dev);
	*state = freenect_get_tilt_state(kinects[index]->dev);
	runloop_exit();
	return 0;
}

int freenect_sync_set_tilt_degs(int angle, int index) {
	if (runloop_enter(index)) return -1;
	freenect_set_tilt_degs(kinects[index]->dev, angle);
	runloop_exit();
	return 0;
}

int freenect_sync_set_led(freenect_led_options led, int index) {
	if (runloop_enter(index)) return -1;
	freenect_set_led(kinects[index]->dev, led);
	runloop_exit();
	return 0;
}

int freenect_sync_camera_to_world(int cx, int cy, int wz, double* wx, double* wy, int index) {
	if (runloop_enter(index)) return -1;
	freenect_camera_to_world(kinects[index]->dev, cx, cy, wz, wx, wy);
	runloop_exit();
	return 0;
}

void freenect_sync_stop(void)
{
	if (thread_running) {
		thread_running = false;
		thread.join();
	}
}
