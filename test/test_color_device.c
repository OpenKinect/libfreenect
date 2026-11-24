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
 * Host-only test of src/color.c against a simulated Kinect for Windows camera
 * that answers GM command 0x96 like the device (§10C.1): writes go into a
 * register file, reads return it with the "valid" bit set.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freenect_internal.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

void fn_log(freenect_context *ctx, freenect_loglevel level, const char *fmt, ...)
{
	(void)ctx; (void)level; (void)fmt;
}

static uint16_t regs[0x10000];
static int stuck_reg = -1;   /* a register that ignores writes */
typedef struct { int width; uint16_t reg, val; } wr;
static wr log_[4096];
static int nlog, nreads;

int send_cmd(freenect_device *dev, uint16_t cmd, void *cmdbuf, unsigned int cmd_len, void *replybuf, int reply_len)
{
	uint16_t c[5] = {0};
	uint8_t *r = (uint8_t*)replybuf;
	(void)dev;
	if (cmd != 0x96 || cmd_len > sizeof(c) || reply_len < 8)
		return -1;
	memcpy(c, cmdbuf, cmd_len);
	if (c[0] == 1 && (c[1] == 0xFF82 || c[1] == 0xFF83)) {
		uint16_t v = regs[c[2]];
		nreads++;
		memset(r, 0, 8);
		r[2] = 1;
		r[6] = (uint8_t)(v & 0xff);
		r[7] = (uint8_t)(c[1] == 0xFF83 ? 0 : v >> 8);
		return 8;
	}
	if (c[0] == 1) {
		log_[nlog++] = (wr){ 16, c[1], c[2] };
		if (c[1] != stuck_reg)
			regs[c[1]] = c[2];
		return 2;
	}
	if (c[0] == 2 && c[1] == 0xFF80 && c[3] == 0xFF81) {
		log_[nlog++] = (wr){ 8, c[2], (uint16_t)(c[4] & 0xff) };
		if (c[2] != stuck_reg)
			regs[c[2]] = c[4] & 0xff;
		return 2;
	}
	return -1;
}

/* Index of the first logged write to reg (with value val if val >= 0). */
static int find(uint16_t reg, int val)
{
	int i;
	for (i = 0; i < nlog; i++)
		if (log_[i].reg == reg && (val < 0 || log_[i].val == val))
			return i;
	return -1;
}

static void reset_log(void) { nlog = 0; nreads = 0; }

int main(void)
{
	static freenect_context ctx;
	static freenect_device dev;
	double v = 0;

	printf("Kinect for Windows colour settings on a simulated camera (src/color.c)\n");
	dev.parent = &ctx;
	fn_color_init(&dev);

	dev.device_type = FN_KINECT_XBOX_1414;
	CHECK(freenect_set_color_setting(&dev, FREENECT_COLOR_GAIN, 2) == -ENOTSUP &&
	      freenect_get_color_setting(&dev, FREENECT_COLOR_GAIN, &v) == -ENOTSUP, "Xbox 360 Kinect: -ENOTSUP, as in the SDK");

	dev.device_type = FN_KINECT_WINDOWS;
	CHECK(freenect_get_color_setting(&dev, FREENECT_COLOR_GAMMA, &v) == 0 && v == 2.2, "defaults before anything is set (gamma 2.2)");
	CHECK(freenect_set_color_setting(&dev, FREENECT_COLOR_GAIN, 20) == -EINVAL, "out-of-range value: -EINVAL");

	/* nothing reaches the camera before video frames arrive */
	CHECK(freenect_set_color_setting(&dev, FREENECT_COLOR_CONTRAST, 2.0) == 0, "set contrast 2.0");
	fn_color_apply_pending(&dev);
	CHECK(nlog == 0 && dev.color_pending, "  not sent while video isn't streaming");
	dev.video.running = 1;
	fn_color_apply_pending(&dev);
	CHECK(nlog == 0, "  nor before the first video frame");
	dev.video.valid_frames = 1;
	fn_color_apply_pending(&dev);
	CHECK(find(0xCC0C, 64) >= 0 && regs[0xCC0C] == 64 && !dev.color_pending, "  sent once a frame arrived: 0xCC0C = 64");
	CHECK(freenect_get_color_setting(&dev, FREENECT_COLOR_CONTRAST, &v) == 0 && v == 2.0, "  and read back by get");

	/* a precondition: exposure time only applies with auto exposure off */
	reset_log();
	freenect_set_color_setting(&dev, FREENECT_COLOR_EXPOSURE_TIME, 300);
	fn_color_apply_pending(&dev);
	CHECK(find(0xCC06, -1) < 0, "exposure time with auto exposure on: not written");
	reset_log();
	freenect_set_color_setting(&dev, FREENECT_COLOR_AUTO_EXPOSURE, 0);
	fn_color_apply_pending(&dev);
	CHECK(regs[0xCC00] == 1 && find(0xCC00, 1) == 0, "auto exposure off: 8-bit 0xCC00 = 1 first");
	CHECK(nreads > 0, "  verified by reading it back");
	CHECK(find(0xCC1C, -1) > 0 && find(0xCC06, 300) > find(0xCC1E, -1) && find(0xCC0E, 32) > 0,
	      "  then frame interval, exposure time (300) and gain re-applied");

	/* power line: read-modify-write of the enable bit, only with AE on */
	reset_log();
	regs[0xCC20] = 0x41;
	freenect_set_color_setting(&dev, FREENECT_COLOR_POWER_LINE_FREQUENCY, 1);
	fn_color_apply_pending(&dev);
	CHECK(find(0xCC03, -1) < 0, "power line with auto exposure off: not written");
	freenect_set_color_setting(&dev, FREENECT_COLOR_AUTO_EXPOSURE, 1);
	fn_color_apply_pending(&dev);
	CHECK(regs[0xCC20] == 0x43 && regs[0xCC03] == 1, "auto exposure on: 0xCC20 bit 1 set (0x41 -> 0x43), 0xCC03 = 1");
	CHECK(find(0xCC0A, 55) > 0, "  and brightness re-applied (55)");

	/* auto white balance off: table, then 0xCC01 = 0, then the white balance */
	reset_log();
	freenect_set_color_setting(&dev, FREENECT_COLOR_WHITE_BALANCE, 5000);
	freenect_set_color_setting(&dev, FREENECT_COLOR_AUTO_WHITE_BALANCE, 0);
	fn_color_apply_pending(&dev);
	CHECK(find(0xc8c8, 0x006a) >= 0 && find(0xFF01, 0x28) > find(0xc8d8, -1) &&
	      find(0xCC01, 0) > find(0xFF01, 0x28) && find(0xCC18, 5000) > find(0xCC01, 0),
	      "auto white balance off: table, commit, 0xCC01 = 0, then 5000 K");

	/* settings are sent again after every video start */
	reset_log();
	dev.video.valid_frames = 0;
	fn_color_on_video_start(&dev);
	fn_color_apply_pending(&dev);
	CHECK(nlog == 0, "after a video start: waits for a frame");
	dev.video.valid_frames = 1;
	fn_color_apply_pending(&dev);
	CHECK(find(0xCC0C, 64) >= 0 && find(0xCC18, 5000) >= 0, "  then everything set so far is sent again");

	/* a register that never takes the value: bounded retries, no hang */
	reset_log();
	stuck_reg = 0xCC08;
	freenect_set_color_setting(&dev, FREENECT_COLOR_BACKLIGHT_COMPENSATION, 2);
	fn_color_apply_pending(&dev);
	CHECK(nlog == 18 && log_[0].reg == 0xCC08, "a verified write that never takes: 18 tries (retries 0x10), then gives up");

	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
