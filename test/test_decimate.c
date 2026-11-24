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
 * Host-only test of the 320x240 and 80x60 depth modes (src/depth_decimate.h):
 * they pick the same 640x480 pixels as Microsoft's runtime (§20.2.4), which
 * takes out[y][x] = src[k*y][(640-k) - k*x] from the unmirrored frame and so
 * delivers a mirrored image.
 */
#include <stdio.h>
#include <stdlib.h>
#include "depth_decimate.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

static uint16_t U[480 * 640], H[480 * 640], out[320 * 240];

/* Pixel (y, x) of the unmirrored frame gets a value that identifies it. */
static uint16_t id(int y, int x) { return (uint16_t)(y * 640 + x); } /* wraps; unique within 102 rows */

/* The SDK's sample for output (y, x), mirrored image (§20.2.4). */
static uint16_t sdk(int k, int y, int x) { return U[(k * y) * 640 + (640 - k) - k * x]; }

static int check(int k, int mirrored)
{
	int w = 640 / k, h = 480 / k, x, y;
	fn_decimate_depth(mirrored ? H : U, out, k, mirrored);
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			/* mirrored: the SDK's image as it is; unmirrored: the SDK's
			 * image mirrored back */
			uint16_t want = mirrored ? sdk(k, y, x) : sdk(k, y, w - 1 - x);
			if (out[y * w + x] != want) {
				printf("    k=%d mirrored=%d: out[%d][%d] = %u, want %u\n", k, mirrored, y, x, out[y * w + x], want);
				return 0;
			}
		}
	return 1;
}

int main(void)
{
	int x, y;
	for (y = 0; y < 480; y++)
		for (x = 0; x < 640; x++) {
			U[y * 640 + x] = id(y, x);
			H[y * 640 + x] = id(y, 639 - x); /* what the camera sends with FREENECT_MIRROR_DEPTH */
		}

	printf("Depth decimation (src/depth_decimate.h)\n");
	CHECK(check(2, 0), "320x240 unmirrored: the SDK's pixels, mirrored back");
	CHECK(check(2, 1), "320x240 with FREENECT_MIRROR_DEPTH: exactly the SDK's image");
	CHECK(check(8, 0), "80x60 unmirrored: the SDK's pixels, mirrored back");
	CHECK(check(8, 1), "80x60 with FREENECT_MIRROR_DEPTH: exactly the SDK's image");

	fn_decimate_depth(U, out, 2, 0);
	CHECK(out[0] == id(0, 0) && out[319] == id(0, 638) && out[320] == id(2, 0), "320x240: rows and columns 0, 2, 4, ...");
	fn_decimate_depth(U, out, 8, 0);
	CHECK(out[79] == id(0, 632) && out[80 * 59] == id(472, 0), "80x60: rows and columns 0, 8, ..., 472 / 632");

	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
