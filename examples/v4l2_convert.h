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
 * Pixel conversions of freenect-v4l2 (v4l2bridge.c), kept apart so that the
 * unit tests can check them without a Kinect or a V4L2 device.
 */

#pragma once

#include <stdint.h>
#include <string.h>

// Depth range shown by the visual depth image: nearer is brighter.
#define FN_V4L2_DEPTH_NEAR_MM 400
#define FN_V4L2_DEPTH_FAR_MM  4500

static inline uint8_t fn_clamp_u8(int v)
{
	return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}

// RGB24 to YUYV (YUY2), BT.601 limited range, as most V4L2 consumers expect.
// width must be even.
static inline void fn_rgb24_to_yuyv(const uint8_t *rgb, uint8_t *yuyv, int width, int height)
{
	int i, n = width * height / 2;
	for (i = 0; i < n; i++, rgb += 6, yuyv += 4) {
		int r0 = rgb[0], g0 = rgb[1], b0 = rgb[2];
		int r1 = rgb[3], g1 = rgb[4], b1 = rgb[5];
		int r = (r0 + r1 + 1) >> 1, g = (g0 + g1 + 1) >> 1, b = (b0 + b1 + 1) >> 1;
		yuyv[0] = fn_clamp_u8(16 + ((66 * r0 + 129 * g0 + 25 * b0 + 128) >> 8));
		yuyv[1] = fn_clamp_u8(128 + ((-38 * r - 74 * g + 112 * b + 128) >> 8));
		yuyv[2] = fn_clamp_u8(16 + ((66 * r1 + 129 * g1 + 25 * b1 + 128) >> 8));
		yuyv[3] = fn_clamp_u8(128 + ((112 * r - 94 * g - 18 * b + 128) >> 8));
	}
}

// Luma of one depth value in millimetres for the visual image: no data is
// black (16), FN_V4L2_DEPTH_NEAR_MM and nearer white (235), far fades to 16.
static inline uint8_t fn_depth_mm_to_luma(uint16_t mm)
{
	if (mm == 0)
		return 16;
	if (mm <= FN_V4L2_DEPTH_NEAR_MM)
		return 235;
	if (mm >= FN_V4L2_DEPTH_FAR_MM)
		return 16;
	return (uint8_t)(235 - (int)(mm - FN_V4L2_DEPTH_NEAR_MM) * 219 / (FN_V4L2_DEPTH_FAR_MM - FN_V4L2_DEPTH_NEAR_MM));
}

// Depth in millimetres to a grey YUYV image.
static inline void fn_depth_mm_to_yuyv(const uint16_t *mm, uint8_t *yuyv, int width, int height)
{
	int i, n = width * height / 2;
	for (i = 0; i < n; i++, mm += 2, yuyv += 4) {
		yuyv[0] = fn_depth_mm_to_luma(mm[0]);
		yuyv[1] = 128;
		yuyv[2] = fn_depth_mm_to_luma(mm[1]);
		yuyv[3] = 128;
	}
}

// Depth in millimetres to Y16 (16-bit little-endian grey, the value is mm).
static inline void fn_depth_mm_to_y16(const uint16_t *mm, uint8_t *y16, int width, int height)
{
	int i, n = width * height;
	for (i = 0; i < n; i++) {
		y16[2 * i] = (uint8_t)(mm[i] & 0xff);
		y16[2 * i + 1] = (uint8_t)(mm[i] >> 8);
	}
}
