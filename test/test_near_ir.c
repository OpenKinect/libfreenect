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
 * Host-only test of near mode and IR brightness (src/flags.c), which share
 * camera register 0x15 (§10C.3). A simulated camera answers the GM command
 * channel: cmd 0x03 writes a register, cmd 0x02 reads one.
 */
#include <stdio.h>
#include <string.h>
#include "freenect_internal.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

void fn_log(freenect_context *ctx, freenect_loglevel level, const char *fmt, ...)
{
	(void)ctx; (void)level; (void)fmt;
}
const char *fnusb_kinect_type_name(fn_kinect_type type) { (void)type; return "test"; }
static freenect_depth_range range_set = (freenect_depth_range)-1;
int freenect_set_depth_range(freenect_device *dev, freenect_depth_range range)
{
	dev->depth_range = range;
	range_set = range;
	return 0;
}

/* --- simulated camera --- */
static uint16_t regs[0x400];
static uint8_t reply[64];
static int reply_len;
static int writes_0x15;

int fnusb_control(fnusb_dev *d, uint8_t type, uint8_t req, uint16_t val, uint16_t idx, uint8_t *data, uint16_t len)
{
	(void)d; (void)req; (void)val; (void)idx; (void)len;
	if (type == 0x40) {
		/* "GM", length in words, command, tag, then the payload */
		uint16_t words = data[2] | (data[3] << 8), cmd = data[4] | (data[5] << 8);
		const uint8_t *p = data + 8;
		uint16_t reg = p[0] | (p[1] << 8);
		int n = 0;
		uint8_t out[8] = {0};
		(void)words;
		if (cmd == 0x03) {
			regs[reg & 0x3ff] = p[2] | (p[3] << 8);
			if (reg == 0x15)
				writes_0x15++;
			n = 2;
		} else if (cmd == 0x02) {
			out[2] = (uint8_t)(regs[reg & 0x3ff] & 0xff);
			out[3] = (uint8_t)(regs[reg & 0x3ff] >> 8);
			n = 4;
		} else {
			n = 2;
		}
		reply[0] = 'R'; reply[1] = 'B';
		reply[2] = (uint8_t)(n / 2); reply[3] = 0;
		memcpy(reply + 4, data + 4, 4); /* same command and tag */
		memcpy(reply + 8, out, n);
		reply_len = 8 + n;
		return 8 + 2 * words;
	}
	if (reply_len) {
		int n = reply_len;
		memcpy(data, reply, n);
		reply_len = 0;
		return n;
	}
	return 0; /* nothing pending */
}

int main(void)
{
	static freenect_context ctx;
	static freenect_device dev;

	printf("Near mode and IR brightness share register 0x15 (src/flags.c)\n");
	dev.parent = &ctx;
	regs[0x15] = 30;

	dev.device_type = FN_KINECT_XBOX_1414;
	CHECK(freenect_set_flag(&dev, FREENECT_NEAR_MODE, FREENECT_ON) < 0 && regs[0x15] == 30,
	      "near mode on an Xbox 360 Kinect: refused, register untouched");

	dev.device_type = FN_KINECT_WINDOWS;
	CHECK(freenect_set_ir_brightness(&dev, 40) == 0 && regs[0x15] == 40, "IR brightness 40: written");
	CHECK(freenect_get_ir_brightness(&dev) == 40, "  and read back");

	CHECK(freenect_set_flag(&dev, FREENECT_NEAR_MODE, FREENECT_ON) == 0 && regs[0x15] == 7 && regs[0x2ef] == 0,
	      "near mode on: 0x15 = 7, 0x2ef = 0");
	CHECK(range_set == FREENECT_DEPTH_RANGE_NEAR, "  and the near depth range (400-3000 mm)");
	CHECK(freenect_get_ir_brightness(&dev) == 40, "  IR brightness reports the 40 near mode off restores");

	writes_0x15 = 0;
	CHECK(freenect_set_ir_brightness(&dev, 20) == 0 && regs[0x15] == 7 && writes_0x15 == 0,
	      "IR brightness 20 in near mode: kept, near mode's 7 stays");

	CHECK(freenect_set_flag(&dev, FREENECT_NEAR_MODE, FREENECT_OFF) == 0 && regs[0x15] == 20 && regs[0x2ef] == 400,
	      "near mode off: 0x15 = 20 (the kept brightness), 0x2ef = 400");
	CHECK(range_set == FREENECT_DEPTH_RANGE_DEFAULT, "  and the default depth range again");
	CHECK(freenect_get_ir_brightness(&dev) == 20, "  IR brightness reads 20");

	dev.ir_brightness = 0; /* an application that never set a brightness */
	freenect_set_flag(&dev, FREENECT_NEAR_MODE, FREENECT_ON);
	freenect_set_flag(&dev, FREENECT_NEAR_MODE, FREENECT_OFF);
	CHECK(regs[0x15] == 30, "no brightness set: near mode off writes 30, as Microsoft's driver");

	dev.depth_range = FREENECT_DEPTH_RANGE_FULL;
	range_set = (freenect_depth_range)-1;
	freenect_set_flag(&dev, FREENECT_NEAR_MODE, FREENECT_ON);
	CHECK(range_set == (freenect_depth_range)-1 && dev.depth_range == FREENECT_DEPTH_RANGE_FULL,
	      "with the full depth range, near mode leaves the range alone");

	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
