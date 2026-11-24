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
 * Host-only test of the Kinect for Windows colour setting encodings
 * (src/color_settings.h) against the reference's §10C.2 table.
 */
#include <math.h>
#include <stdio.h>
#include "color_settings.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

static fn_color_write w[2];

/* One write of `width` bits to `reg` with value `val`. */
static int one(freenect_color_setting s, double v, int width, uint16_t reg, uint16_t val)
{
	int n = fn_color_encode(s, v, w);
	if (n != 1 || w[0].width != width || w[0].reg != reg || w[0].val != val) {
		printf("    setting %d value %g: n=%d width=%d reg=%04x val=%04x\n", s, v, n, w[0].width, w[0].reg, w[0].val);
		return 0;
	}
	return 1;
}

static int two(freenect_color_setting s, double v, uint16_t reg_hi, uint16_t hi, uint16_t lo)
{
	int n = fn_color_encode(s, v, w);
	return n == 2 && w[0].reg == reg_hi && w[0].val == hi && w[1].reg == reg_hi + 2 && w[1].val == lo;
}

int main(void)
{
	int s, ok;
	printf("Kinect for Windows colour settings (src/color_settings.h)\n");

	CHECK(one(FREENECT_COLOR_AUTO_EXPOSURE, 1, 8, 0xCC00, 2) && w[0].retries == 0x10 &&
	      one(FREENECT_COLOR_AUTO_EXPOSURE, 0, 8, 0xCC00, 1), "auto exposure: 8-bit 0xCC00 = 2 on, 1 off, verified");
	CHECK(two(FREENECT_COLOR_EXPOSURE_TIME, 4000, 0xCC04, 0, 4000) && two(FREENECT_COLOR_EXPOSURE_TIME, 1, 0xCC04, 0, 1),
	      "exposure time: 0xCC04/06, 32-bit whole units");
	CHECK(two(FREENECT_COLOR_FRAME_INTERVAL, 0, 0xCC1C, 0, 1) && two(FREENECT_COLOR_FRAME_INTERVAL, 33.3, 0xCC1C, 0, 33),
	      "frame interval: 0xCC1C/1E, at least 1");
	CHECK(one(FREENECT_COLOR_GAIN, 1.0, 16, 0xCC0E, 32) && one(FREENECT_COLOR_GAIN, 16.0, 16, 0xCC0E, 512),
	      "gain: 0xCC0E = (g*100+1)*0.32 (1.0 -> 32, 16.0 -> 512)");
	CHECK(one(FREENECT_COLOR_BRIGHTNESS, 0.2156863, 16, 0xCC0A, 55) && one(FREENECT_COLOR_BRIGHTNESS, 1.0, 16, 0xCC0A, 255),
	      "brightness: 0xCC0A, default 55 of 255");
	CHECK(one(FREENECT_COLOR_POWER_LINE_FREQUENCY, 2, 8, 0xCC03, 2) && one(FREENECT_COLOR_POWER_LINE_FREQUENCY, 0, 8, 0xCC03, 0),
	      "power line: 8-bit 0xCC03 = 0, 1 or 2");
	CHECK(one(FREENECT_COLOR_BACKLIGHT_COMPENSATION, 4, 16, 0xCC08, 4) && one(FREENECT_COLOR_BACKLIGHT_COMPENSATION, 7, 16, 0xCC08, 4),
	      "backlight: 0xCC08, at most 4");
	CHECK(one(FREENECT_COLOR_AUTO_WHITE_BALANCE, 1, 8, 0xCC01, 1) && one(FREENECT_COLOR_AUTO_WHITE_BALANCE, 0, 8, 0xCC01, 0),
	      "auto white balance: 8-bit 0xCC01");
	CHECK(one(FREENECT_COLOR_WHITE_BALANCE, 6500, 16, 0xCC18, 6500), "white balance: 0xCC18 = kelvin");
	CHECK(one(FREENECT_COLOR_CONTRAST, 1.0, 16, 0xCC0C, 32) && one(FREENECT_COLOR_CONTRAST, 0.5, 16, 0xCC0C, 16) &&
	      one(FREENECT_COLOR_CONTRAST, 2.0, 16, 0xCC0C, 64), "contrast: 0xCC0C, 16..64 (default 32)");
	CHECK(one(FREENECT_COLOR_HUE, -22, 16, 0xCC10, (uint16_t)-2200) && one(FREENECT_COLOR_HUE, 22, 16, 0xCC10, 2200) &&
	      one(FREENECT_COLOR_HUE, 0, 16, 0xCC10, 0), "hue: 0xCC10 = h*100, two's complement");
	CHECK(one(FREENECT_COLOR_SATURATION, 1.0, 16, 0xCC12, 129) && one(FREENECT_COLOR_SATURATION, 2.0, 16, 0xCC12, 255),
	      "saturation: 0xCC12 (1.0 -> 129, at most 255)");
	CHECK(one(FREENECT_COLOR_GAMMA, 2.2, 16, 0xCC16, 220) && one(FREENECT_COLOR_GAMMA, 2.8, 16, 0xCC16, 280) &&
	      one(FREENECT_COLOR_GAMMA, 1.0, 16, 0xCC16, 100), "gamma: 0xCC16 = g*100");
	CHECK(one(FREENECT_COLOR_SHARPNESS, 0.5, 16, 0xCC14, 0) && one(FREENECT_COLOR_SHARPNESS, 0.0, 16, 0xCC14, (uint16_t)-7) &&
	      one(FREENECT_COLOR_SHARPNESS, 1.0, 16, 0xCC14, 7), "sharpness: 0xCC14, -7..7 (default 0)");

	CHECK(fn_color_encode(FREENECT_COLOR_GAIN, 0.5, w) == -EINVAL && fn_color_encode(FREENECT_COLOR_HUE, 23, w) == -EINVAL &&
	      fn_color_encode(FREENECT_COLOR_WHITE_BALANCE, 2000, w) == -EINVAL &&
	      fn_color_encode(FREENECT_COLOR_EXPOSURE_TIME, 0.5, w) == -EINVAL, "values outside the SDK's ranges are refused");
	CHECK(fn_color_encode(FREENECT_COLOR_GAMMA, NAN, w) == -EINVAL &&
	      fn_color_encode((freenect_color_setting)99, 1, w) == -EINVAL, "  as are NaN and unknown settings");

	ok = 1;
	for (s = 0; s < FREENECT_COLOR_SETTING_COUNT; s++) {
		const fn_color_info *info = fn_color_setting_info((freenect_color_setting)s);
		if (!info || fn_color_encode((freenect_color_setting)s, info->def, w) < 1)
			ok = 0;
	}
	CHECK(ok, "every default is within its range");
	CHECK(fn_color_setting_info(FREENECT_COLOR_EXPOSURE_TIME)->needs_ae < 0 &&
	      fn_color_setting_info(FREENECT_COLOR_BRIGHTNESS)->needs_ae > 0 &&
	      fn_color_setting_info(FREENECT_COLOR_WHITE_BALANCE)->needs_awb < 0, "preconditions: exposure manual, brightness auto, WB manual");

	CHECK(fn_color_awb_regs[0] == 0xc8c8 && fn_color_awb_regs[26] == 0xc8d8 &&
	      fn_color_awb_off_vals[0] == 0x006a && fn_color_awb_on_vals[0] == 0x001b &&
	      fn_color_awb_off_vals[24] == 2500 && fn_color_awb_on_vals[26] == 6500, "white balance colour-matrix tables");

	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
