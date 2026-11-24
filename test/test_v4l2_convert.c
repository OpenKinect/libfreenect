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

/* Host-only test of freenect-v4l2's pixel conversions (examples/v4l2_convert.h). */
#include <stdio.h>
#include <string.h>
#include "v4l2_convert.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

static void yuyv_of(uint8_t r, uint8_t g, uint8_t b, uint8_t out[4])
{
	uint8_t rgb[6] = { r, g, b, r, g, b };
	fn_rgb24_to_yuyv(rgb, out, 2, 1);
}

int main(void)
{
	uint8_t p[4];
	uint16_t mm[4] = { 0, 400, 2450, 6000 };
	uint8_t out[8];

	printf("1. RGB24 -> YUYV (BT.601, limited range)\n");
	yuyv_of(0, 0, 0, p);
	CHECK(p[0] == 16 && p[1] == 128 && p[2] == 16 && p[3] == 128, "black = Y 16, U/V 128");
	yuyv_of(255, 255, 255, p);
	CHECK(p[0] == 235 && p[2] == 235 && p[1] >= 127 && p[1] <= 129 && p[3] >= 127 && p[3] <= 129, "white = Y 235, U/V 128");
	yuyv_of(255, 0, 0, p);
	CHECK(p[0] >= 81 && p[0] <= 82 && p[1] >= 89 && p[1] <= 91 && p[3] >= 239 && p[3] <= 241, "red = Y 81, U 90, V 240");
	yuyv_of(0, 0, 255, p);
	CHECK(p[0] >= 40 && p[0] <= 42 && p[1] >= 239 && p[1] <= 241 && p[3] >= 109 && p[3] <= 111, "blue = Y 41, U 240, V 110");
	{
		uint8_t rgb[6] = { 0, 0, 0, 255, 255, 255 };
		fn_rgb24_to_yuyv(rgb, p, 2, 1);
		CHECK(p[0] == 16 && p[2] == 235, "each pixel keeps its own luma");
	}

	printf("2. depth (mm) -> visual YUYV\n");
	CHECK(fn_depth_mm_to_luma(0) == 16, "no data is black");
	CHECK(fn_depth_mm_to_luma(300) == 235 && fn_depth_mm_to_luma(400) == 235, "near limit and nearer are white");
	CHECK(fn_depth_mm_to_luma(4500) == 16 && fn_depth_mm_to_luma(9000) == 16, "far limit and farther are black");
	CHECK(fn_depth_mm_to_luma(2450) >= 125 && fn_depth_mm_to_luma(2450) <= 127, "half way is mid grey");
	CHECK(fn_depth_mm_to_luma(1000) > fn_depth_mm_to_luma(2000), "nearer is brighter");
	fn_depth_mm_to_yuyv(mm, out, 4, 1);
	CHECK(out[0] == 16 && out[2] == 235 && out[1] == 128 && out[3] == 128 && out[6] == 16, "pixels and grey chroma");

	printf("3. depth (mm) -> Y16\n");
	mm[0] = 1234; mm[1] = 0; mm[2] = 0xffff;
	fn_depth_mm_to_y16(mm, out, 3, 1);
	CHECK(out[0] == 0xd2 && out[1] == 0x04, "1234 mm = d2 04 (little endian)");
	CHECK(out[2] == 0 && out[3] == 0 && out[4] == 0xff && out[5] == 0xff, "0 and 65535 kept");

	printf("\n%s\n", pass ? "ALL PASS" : "SOME FAILED");
	return pass ? 0 : 1;
}
