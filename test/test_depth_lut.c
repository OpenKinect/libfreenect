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
 * Host-only test of the raw -> mm table (src/depth_lut.h) against Microsoft's
 * runtime, as reimplemented in the reference's tools/kinect10_depth_ref.py
 * (shift_to_depth_table() and packed_lut()). The calibration values are the
 * tool's illustrative ones (E = 7.5 cm, Zref = 120 cm, P = 0.1042 cm,
 * const_shift = 200), not a real device's. libfreenect keeps the reference
 * distance unrounded; at 120.0 cm that makes no difference.
 */
#include <stdio.h>
#include "depth_lut.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

/* (raw, mm) from shift_to_depth_table() */
static const int golden[][2] = {
	{0, 0}, {41, 330}, {82, 343}, {123, 358}, {164, 373}, {205, 391}, {246, 410}, {287, 431},
	{328, 454}, {369, 480}, {410, 509}, {451, 542}, {492, 579}, {533, 622}, {574, 672}, {615, 730},
	{656, 799}, {697, 883}, {738, 987}, {779, 1118}, {799, 1195}, {800, 1200}, {801, 1204},
	{820, 1289}, {861, 1522}, {902, 1858}, {943, 2384}, {984, 3324}, {1000, 3930}, {1025, 5491},
	{1049, 8879}, {1050, 9113}, {1066, 15770}, {1080, 43689}, {1107, 0}, {1500, 0}, {2046, 0},
};

static uint16_t lut[FN_DEPTH_LUT_SIZE];

static long sum(void)
{
	long s = 0;
	int k;
	for (k = 0; k < FN_DEPTH_LUT_SIZE; k++)
		s += lut[k];
	return s;
}

/* What the range should leave of a golden value. */
static int in_range(int mm, int lo, int hi) { return mm >= lo && mm <= hi ? mm : 0; }

static int check_golden(int lo, int hi)
{
	unsigned i;
	for (i = 0; i < sizeof(golden) / sizeof(golden[0]); i++) {
		int want = in_range(golden[i][1], lo, hi);
		if (lut[golden[i][0]] != want) {
			printf("    raw %d: %u mm, want %d\n", golden[i][0], lut[golden[i][0]], want);
			return 0;
		}
	}
	return 1;
}

int main(void)
{
	printf("Raw depth -> mm (src/depth_lut.h)\n");

	fn_build_depth_lut(lut, 7.5f, 120.0f, 0.1042f, 200, FREENECT_DEPTH_RANGE_FULL);
	CHECK(check_golden(1, 65535), "full range (unlimited): every distance kept");
	CHECK(lut[1050] == 9113 && lut[1066] == 15770 && lut[1080] == 43689 && lut[41] == 330,
	      "  for example 330 mm, 9113 mm, 15770 mm and 43689 mm");
	CHECK(sum() == 1831331, "  and the whole table matches the SDK's");
	CHECK(lut[1083] == 0 && lut[1087] == 0, "  above 65535 mm (where the SDK's value wraps): no value");

	fn_build_depth_lut(lut, 7.5f, 120.0f, 0.1042f, 200, FREENECT_DEPTH_RANGE_DEFAULT);
	CHECK(check_golden(800, 4000), "default range: the SDK's values, 800-4000 mm kept");
	CHECK(sum() == 555365, "  and the whole table equals the SDK's packed table");
	CHECK(lut[656] == 0 && lut[1025] == 0, "  799 mm and 5491 mm are dropped");

	fn_build_depth_lut(lut, 7.5f, 120.0f, 0.1042f, 200, FREENECT_DEPTH_RANGE_NEAR);
	CHECK(check_golden(400, 3000), "near range: 400-3000 mm kept");
	CHECK(sum() == 694878, "  and the whole table equals the SDK's near-mode table");

	CHECK(lut[0] == 0 && lut[FREENECT_DEPTH_RAW_NO_VALUE] == FREENECT_DEPTH_MM_NO_VALUE,
	      "raw 0 and raw 2047 (no data) are always 0");

	/* Unlike the SDK, the reference distance is not truncated to whole cm */
	fn_build_depth_lut(lut, 7.5f, 120.9f, 0.1042f, 200, FREENECT_DEPTH_RANGE_FULL);
	CHECK(lut[800] == 1209 && lut[1000] == 3959, "the reference distance keeps its millimetres (120.9 cm)");

	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
