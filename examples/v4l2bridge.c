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
 * freenect-v4l2 (Linux): serve the Kinect's depth and colour streams as V4L2
 * cameras through v4l2loopback, so that ordinary camera programs (OpenCV,
 * GStreamer, ffmpeg, browsers, video calls) can use them. See
 * platform/linux/README.md.
 *
 *   sudo modprobe v4l2loopback devices=2 video_nr=10,11 \
 *        card_label="Kinect depth,Kinect video" exclusive_caps=1
 *   freenect-v4l2 --depth /dev/video10 --video /dev/video11
 *
 * Options:
 *   --depth DEV            write depth to DEV
 *   --depth-format FMT     y16 (default: 16-bit, the value is millimetres) or
 *                          yuyv (a grey image, near = bright, for viewers)
 *   --video DEV            write colour to DEV
 *   --video-format FMT     yuyv (default, what most programs accept) or rgb24
 *   --serial SERIAL        use this Kinect (default: the first one)
 *   --test-pattern         write moving test images instead of opening a
 *                          Kinect (to check the loopback setup)
 *   --frames N             stop after N frames per stream (0 = run until Ctrl+C)
 *
 * It only writes to V4L2 *output* devices (v4l2loopback), never to a camera.
 * When the Kinect is unplugged it waits for it to come back. Ctrl+C stops it.
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include "libfreenect.h"
#include "console_exit.h"
#include "v4l2_convert.h"

#define W 640
#define H 480

typedef struct {
	const char *path;
	int fd;
	uint32_t fourcc;
	size_t size;
	uint8_t *buf;
	unsigned long frames, dropped;
} sink;

static sink depth_sink = { NULL, -1, V4L2_PIX_FMT_Y16, 0, NULL, 0, 0 };
static sink video_sink = { NULL, -1, V4L2_PIX_FMT_YUYV, 0, NULL, 0, 0 };
static freenect_device *dev = NULL;
static const char *want_serial = NULL;
static unsigned long max_frames = 0;

static int open_sink(sink *s)
{
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_streamparm parm;
	int bpp = s->fourcc == V4L2_PIX_FMT_RGB24 ? 3 : 2;

	s->fd = open(s->path, O_RDWR | O_NONBLOCK);
	if (s->fd < 0) {
		fprintf(stderr, "%s: %s\n", s->path, strerror(errno));
		return -1;
	}
	memset(&cap, 0, sizeof(cap));
	if (ioctl(s->fd, VIDIOC_QUERYCAP, &cap) < 0) {
		fprintf(stderr, "%s: not a V4L2 device (%s)\n", s->path, strerror(errno));
		return -1;
	}
	{
		uint32_t caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
		if (!(caps & V4L2_CAP_VIDEO_OUTPUT)) {
			fprintf(stderr, "%s (%s) is not a video output device; use a v4l2loopback device\n", s->path, (char *)cap.card);
			return -1;
		}
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.width = W;
	fmt.fmt.pix.height = H;
	fmt.fmt.pix.pixelformat = s->fourcc;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	fmt.fmt.pix.bytesperline = (uint32_t)(W * bpp);
	fmt.fmt.pix.sizeimage = (uint32_t)(W * H * bpp);
	fmt.fmt.pix.colorspace = s->fourcc == V4L2_PIX_FMT_Y16 ? V4L2_COLORSPACE_RAW
	                       : s->fourcc == V4L2_PIX_FMT_RGB24 ? V4L2_COLORSPACE_SRGB : V4L2_COLORSPACE_SMPTE170M;
	if (ioctl(s->fd, VIDIOC_S_FMT, &fmt) < 0) {
		fprintf(stderr, "%s: cannot set %dx%d %.4s: %s\n", s->path, W, H, (char *)&s->fourcc, strerror(errno));
		return -1;
	}
	if (fmt.fmt.pix.width != W || fmt.fmt.pix.height != H || fmt.fmt.pix.pixelformat != s->fourcc) {
		fprintf(stderr, "%s: the device chose another format (%ux%u %.4s); is it in use?\n", s->path,
		        fmt.fmt.pix.width, fmt.fmt.pix.height, (char *)&fmt.fmt.pix.pixelformat);
		return -1;
	}
	memset(&parm, 0, sizeof(parm));
	parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	parm.parm.output.timeperframe.numerator = 1;
	parm.parm.output.timeperframe.denominator = 30;
	ioctl(s->fd, VIDIOC_S_PARM, &parm); /* not every loopback driver supports it */

	s->size = fmt.fmt.pix.sizeimage ? fmt.fmt.pix.sizeimage : (size_t)(W * H * bpp);
	s->buf = (uint8_t *)malloc(s->size);
	if (!s->buf)
		return -1;
	printf("%s: %s, %dx%d %.4s at 30 fps\n", s->path, (char *)cap.card, W, H, (char *)&s->fourcc);
	return 0;
}

static void write_sink(sink *s)
{
	ssize_t n = write(s->fd, s->buf, s->size);
	if (n == (ssize_t)s->size)
		s->frames++;
	else
		s->dropped++; /* EAGAIN: nobody reads, or the driver is busy */
}

static void depth_cb(freenect_device *d, void *data, uint32_t timestamp)
{
	(void)d; (void)timestamp;
	if (depth_sink.fourcc == V4L2_PIX_FMT_Y16)
		fn_depth_mm_to_y16((const uint16_t *)data, depth_sink.buf, W, H);
	else
		fn_depth_mm_to_yuyv((const uint16_t *)data, depth_sink.buf, W, H);
	write_sink(&depth_sink);
}

static void video_cb(freenect_device *d, void *data, uint32_t timestamp)
{
	(void)d; (void)timestamp;
	if (video_sink.fourcc == V4L2_PIX_FMT_RGB24)
		memcpy(video_sink.buf, data, (size_t)(W * H * 3));
	else
		fn_rgb24_to_yuyv((const uint8_t *)data, video_sink.buf, W, H);
	write_sink(&video_sink);
}

static void close_kinect(void)
{
	if (!dev)
		return;
	if (freenect_device_connected(dev)) {
		if (depth_sink.path) freenect_stop_depth(dev);
		if (video_sink.path) freenect_stop_video(dev);
	}
	freenect_close_device(dev);
	dev = NULL;
}

static void open_kinect(freenect_context *ctx, const char *serial)
{
	if (dev || (want_serial && strcmp(serial, want_serial) != 0))
		return;
	if (!serial[0] || freenect_open_device_by_camera_serial(ctx, &dev, serial) < 0) {
		fprintf(stderr, "Could not open the Kinect %s\n", serial[0] ? serial : "(serial unknown)");
		dev = NULL;
		return;
	}
	printf("Kinect %s: streaming\n", serial);
	fflush(stdout);
	if (depth_sink.path) {
		freenect_set_depth_mode(dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM));
		freenect_set_depth_callback(dev, depth_cb);
		freenect_start_depth(dev);
	}
	if (video_sink.path) {
		freenect_set_video_mode(dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
		freenect_set_video_callback(dev, video_cb);
		freenect_start_video(dev);
	}
}

static void hotplug_cb(freenect_context *ctx, freenect_hotplug_event event, const char *serial, void *user)
{
	(void)user;
	if (event == FREENECT_HOTPLUG_ARRIVED) {
		open_kinect(ctx, serial);
	} else if (dev && !freenect_device_connected(dev)) {
		printf("Kinect %s left; waiting for it to come back\n", serial);
		fflush(stdout);
		close_kinect();
	}
}

static int run_test_pattern(void)
{
	static uint8_t rgb[W * H * 3];
	static uint16_t mm[W * H];
	unsigned long t;
	for (t = 0; !fn_exit_requested && (max_frames == 0 || t < max_frames); t++) {
		int x, y;
		for (y = 0; y < H; y++) {
			for (x = 0; x < W; x++) {
				int bar = ((x + (int)t * 4) / 80) % 8;
				uint8_t *p = &rgb[(y * W + x) * 3];
				p[0] = (bar & 4) ? 255 : 0;
				p[1] = (bar & 2) ? 255 : 0;
				p[2] = (bar & 1) ? 255 : 0;
				mm[y * W + x] = (uint16_t)(FN_V4L2_DEPTH_NEAR_MM + ((x + y + (int)t * 8) % 800) * 5);
			}
		}
		if (depth_sink.path) depth_cb(NULL, mm, 0);
		if (video_sink.path) video_cb(NULL, rgb, 0);
		fn_sleep_ms(33);
	}
	return 0;
}

static void usage(const char *argv0)
{
	printf("Usage: %s [--depth DEV [--depth-format y16|yuyv]] [--video DEV [--video-format yuyv|rgb24]]\n"
	       "          [--serial SERIAL] [--test-pattern] [--frames N]\n"
	       "Serves the Kinect's depth and colour as V4L2 cameras through v4l2loopback devices.\n", argv0);
}

int main(int argc, char **argv)
{
	freenect_context *ctx;
	int i, test_pattern = 0, ret = 0;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
		if (strcmp(a, "--depth") == 0 && v) { depth_sink.path = v; i++; }
		else if (strcmp(a, "--video") == 0 && v) { video_sink.path = v; i++; }
		else if (strcmp(a, "--depth-format") == 0 && v && (strcmp(v, "y16") == 0 || strcmp(v, "yuyv") == 0)) {
			depth_sink.fourcc = strcmp(v, "y16") == 0 ? V4L2_PIX_FMT_Y16 : V4L2_PIX_FMT_YUYV; i++;
		} else if (strcmp(a, "--video-format") == 0 && v && (strcmp(v, "yuyv") == 0 || strcmp(v, "rgb24") == 0)) {
			video_sink.fourcc = strcmp(v, "yuyv") == 0 ? V4L2_PIX_FMT_YUYV : V4L2_PIX_FMT_RGB24; i++;
		} else if (strcmp(a, "--serial") == 0 && v) { want_serial = v; i++; }
		else if (strcmp(a, "--frames") == 0 && v) { max_frames = strtoul(v, NULL, 10); i++; }
		else if (strcmp(a, "--test-pattern") == 0) test_pattern = 1;
		else { usage(argv[0]); return strcmp(a, "--help") == 0 ? 0 : 1; }
	}
	if (!depth_sink.path && !video_sink.path) {
		usage(argv[0]);
		return 1;
	}

	fn_install_exit_handlers();
	if ((depth_sink.path && open_sink(&depth_sink) < 0) || (video_sink.path && open_sink(&video_sink) < 0))
		return 1;

	if (test_pattern) {
		ret = run_test_pattern();
	} else {
		if (freenect_init(&ctx, NULL) < 0) {
			fprintf(stderr, "freenect_init() failed\n");
			return 1;
		}
		freenect_set_log_level(ctx, FREENECT_LOG_NOTICE);
		freenect_select_subdevices(ctx, FREENECT_DEVICE_CAMERA);
		freenect_set_hotplug_callback(ctx, hotplug_cb, NULL);
		printf("Waiting for a Kinect (Ctrl+C to stop)...\n");
		fflush(stdout);
		while (!fn_exit_requested &&
		       (max_frames == 0 || (depth_sink.path && depth_sink.frames + depth_sink.dropped < max_frames) ||
		                           (video_sink.path && video_sink.frames + video_sink.dropped < max_frames))) {
			struct timeval tv = { 0, 100000 };
			freenect_process_events_timeout(ctx, &tv);
		}
		close_kinect();
		freenect_shutdown(ctx);
	}

	if (depth_sink.path) printf("%s: %lu frames written, %lu not taken\n", depth_sink.path, depth_sink.frames, depth_sink.dropped);
	if (video_sink.path) printf("%s: %lu frames written, %lu not taken\n", video_sink.path, video_sink.frames, video_sink.dropped);
	if (depth_sink.fd >= 0) close(depth_sink.fd);
	if (video_sink.fd >= 0) close(video_sink.fd);
	free(depth_sink.buf);
	free(video_sink.buf);
	return fn_exit_done(ret);
}
