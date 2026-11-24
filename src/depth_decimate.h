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

#pragma once

#include <stdint.h>

/*
 * Depth at 320x240 and 80x60, taken from the 640x480 frame (§20.2.4).
 *
 * Microsoft's runtime takes the nearest pixel, with no averaging: it keeps
 * every k-th row from row 0, and from the unmirrored frame the columns
 * 640-k, 640-2k, ... 0, which it then mirrors. libfreenect's frames are
 * unmirrored unless FREENECT_MIRROR_DEPTH is on (the camera then mirrors
 * them), so the same source pixels are:
 *   unmirrored: out[y][x] = src[k*y][k*x]
 *   mirrored:   out[y][x] = src[k*y][k*x + k-1]
 * k is 2 for 320x240 and 8 for 80x60. src is 640x480.
 */
static inline void fn_decimate_depth(const uint16_t *src, uint16_t *out, int k, int mirrored)
{
	const int w = 640 / k, h = 480 / k, x0 = mirrored ? k - 1 : 0;
	int x, y;
	for (y = 0; y < h; y++) {
		const uint16_t *row = src + (k * y) * 640 + x0;
		for (x = 0; x < w; x++)
			out[y * w + x] = row[k * x];
	}
}
