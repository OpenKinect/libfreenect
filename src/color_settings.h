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

/*
 * Kinect for Windows colour camera settings (§10C.2): ranges, defaults and
 * the register writes each value becomes. Kept free of device code so that
 * the encodings can be tested on the host.
 *
 * Registers are colour-sensor registers, written with GM command 0x96
 * (§10C.1). "retries" follows Kinect10: >= 0 means write and read back, up
 * to retries+2 times, until the value reads back; < 0 means write
 * |retries|+2 times without reading back.
 */

#include <errno.h>
#include <stdint.h>
#include "libfreenect.h"

#define FN_COLOR_REG_AE          0xCC00 // 8-bit: 1 manual, 2 auto; reads bit 1 = auto active
#define FN_COLOR_REG_AWB         0xCC01 // 8-bit: 1 auto white balance on
#define FN_COLOR_REG_POWERLINE   0xCC03 // 8-bit: 0, 1 = 50 Hz, 2 = 60 Hz
#define FN_COLOR_REG_EXPOSURE_HI 0xCC04
#define FN_COLOR_REG_EXPOSURE_LO 0xCC06
#define FN_COLOR_REG_BACKLIGHT   0xCC08
#define FN_COLOR_REG_BRIGHTNESS  0xCC0A
#define FN_COLOR_REG_CONTRAST    0xCC0C
#define FN_COLOR_REG_GAIN        0xCC0E
#define FN_COLOR_REG_HUE         0xCC10
#define FN_COLOR_REG_SATURATION  0xCC12
#define FN_COLOR_REG_SHARPNESS   0xCC14
#define FN_COLOR_REG_GAMMA       0xCC16
#define FN_COLOR_REG_WB          0xCC18
#define FN_COLOR_REG_INTERVAL_HI 0xCC1C
#define FN_COLOR_REG_INTERVAL_LO 0xCC1E
#define FN_COLOR_REG_PL_ENABLE   0xCC20 // 8-bit: bit 1 enables power-line filtering
#define FN_COLOR_REG_COMMIT      0xFF01 // written 0x28 after a colour-matrix table

typedef struct {
	uint8_t width;   // 8 or 16 bits
	uint16_t reg;
	uint16_t val;
	int8_t retries;
} fn_color_write;

typedef struct {
	double min, max, def;
	int needs_ae;    // 1: only with auto exposure on, -1: only with it off, 0: either
	int needs_awb;   // -1: only with auto white balance off, 0: either
} fn_color_info;

static inline const fn_color_info *fn_color_setting_info(freenect_color_setting s)
{
	static const fn_color_info info[FREENECT_COLOR_SETTING_COUNT] = {
		/* AUTO_EXPOSURE */          { 0, 1, 1, 0, 0 },
		/* EXPOSURE_TIME */          { 1, 4000, 4000, -1, 0 },
		/* FRAME_INTERVAL */         { 0, 4000, 0, -1, 0 },
		/* GAIN */                   { 1, 16, 1, -1, 0 },
		/* BRIGHTNESS */             { 0, 1, 0.2156863, 1, 0 },
		/* POWER_LINE_FREQUENCY */   { 0, 2, 0, 1, 0 },
		/* BACKLIGHT_COMPENSATION */ { 0, 255, 0, 1, 0 },
		/* AUTO_WHITE_BALANCE */     { 0, 1, 1, 0, 0 },
		/* WHITE_BALANCE */          { 2700, 6500, 2700, 0, -1 },
		/* CONTRAST */               { 0.5, 2, 1, 0, 0 },
		/* HUE */                    { -22, 22, 0, 0, 0 },
		/* SATURATION */             { 0, 2, 1, 0, 0 },
		/* GAMMA */                  { 1, 2.8, 2.2, 0, 0 },
		/* SHARPNESS */              { 0, 1, 0.5, 0, 0 },
	};
	if ((int)s < 0 || s >= FREENECT_COLOR_SETTING_COUNT)
		return 0;
	return &info[s];
}

static inline int fn_color_clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/*
 * The register writes for one setting (at most 2), as Kinect10 encodes them.
 * Returns their number, or -EINVAL. Auto exposure's and auto white balance's
 * side effects, the colour-matrix tables and the power-line enable bit are
 * the caller's (src/color.c).
 */
static inline int fn_color_encode(freenect_color_setting s, double value, fn_color_write *w)
{
	const fn_color_info *info = fn_color_setting_info(s);
	int v, i;
	if (!info || !(value >= info->min && value <= info->max)) // also rejects NaN
		return -EINVAL;

	switch (s) {
		case FREENECT_COLOR_AUTO_EXPOSURE:
			w[0] = (fn_color_write){ 8, FN_COLOR_REG_AE, (uint16_t)(value != 0 ? 2 : 1), 0x10 };
			return 1;
		case FREENECT_COLOR_EXPOSURE_TIME:
		case FREENECT_COLOR_FRAME_INTERVAL: {
			// in steps of 1/100, clamped to 1..600000, then whole units
			int64_t x = (int64_t)(value * 100);
			uint32_t u;
			if (x < 100) x = 100;
			if (x > 60000000) x = 60000000;
			u = (uint32_t)(x / 100);
			uint16_t hi = s == FREENECT_COLOR_EXPOSURE_TIME ? FN_COLOR_REG_EXPOSURE_HI : FN_COLOR_REG_INTERVAL_HI;
			w[0] = (fn_color_write){ 16, hi, (uint16_t)(u >> 16), -8 };
			w[1] = (fn_color_write){ 16, (uint16_t)(hi + 2), (uint16_t)(u & 0xffff), -8 };
			return 2;
		}
		case FREENECT_COLOR_GAIN:
			v = (int)(value * 100);
			i = v + 1 < 204700 ? v + 1 : 204700;
			w[0] = (fn_color_write){ 16, FN_COLOR_REG_GAIN, (uint16_t)(int)(i * 0.32), -8 };
			return 1;
		case FREENECT_COLOR_BRIGHTNESS:
			v = (int)(value * 10000);
			w[0] = (fn_color_write){ 16, FN_COLOR_REG_BRIGHTNESS, (uint16_t)((v + 1) * 255 / 10000), -8 };
			return 1;
		case FREENECT_COLOR_POWER_LINE_FREQUENCY:
			w[0] = (fn_color_write){ 8, FN_COLOR_REG_POWERLINE, (uint16_t)(int)value, 0x10 };
			return 1;
		case FREENECT_COLOR_BACKLIGHT_COMPENSATION:
			v = (int)value;
			w[0] = (fn_color_write){ 16, FN_COLOR_REG_BACKLIGHT, (uint16_t)(v < 4 ? v : 4), 0x10 };
			return 1;
		case FREENECT_COLOR_AUTO_WHITE_BALANCE:
			w[0] = (fn_color_write){ 8, FN_COLOR_REG_AWB, (uint16_t)(value != 0 ? 1 : 0), 0x10 };
			return 1;
		case FREENECT_COLOR_WHITE_BALANCE:
			w[0] = (fn_color_write){ 16, FN_COLOR_REG_WB, (uint16_t)(int)value, -8 };
			return 1;
		case FREENECT_COLOR_CONTRAST:
			v = fn_color_clamp((int)(value * 100), 50, 200);
			w[0] = (fn_color_write){ 16, FN_COLOR_REG_CONTRAST, (uint16_t)(int)(v * 0.32), -8 };
			return 1;
		case FREENECT_COLOR_HUE:
			v = fn_color_clamp((int)(value * 100), -2200, 2200);
			w[0] = (fn_color_write){ 16, FN_COLOR_REG_HUE, (uint16_t)(int16_t)v, -8 };
			return 1;
		case FREENECT_COLOR_SATURATION: {
			unsigned t;
			v = (int)(value * 100);
			i = v + 1 < 0xC79C ? v + 1 : 0xC79C;
			t = (uint16_t)(i * 1.28);
			w[0] = (fn_color_write){ 16, FN_COLOR_REG_SATURATION, (uint16_t)(t < 255 ? t : 255), -8 };
			return 1;
		}
		case FREENECT_COLOR_GAMMA:
			v = fn_color_clamp((int)(value * 100), 100, 280);
			w[0] = (fn_color_write){ 16, FN_COLOR_REG_GAMMA, (uint16_t)v, -8 };
			return 1;
		case FREENECT_COLOR_SHARPNESS:
			v = (int)(value * 100);
			v = fn_color_clamp(v * 15 / 100 - 7, -7, 7);
			w[0] = (fn_color_write){ 16, FN_COLOR_REG_SHARPNESS, (uint16_t)(int16_t)v, -8 };
			return 1;
		default:
			return -EINVAL;
	}
}

/* Colour-matrix tables written when auto white balance is switched on or
 * off (§10C.2), each followed by 0xFF01 = 0x28. */
#define FN_COLOR_AWB_TABLE_LEN 27
static const uint16_t fn_color_awb_regs[FN_COLOR_AWB_TABLE_LEN] = {
	0xc8c8, 0xc8ca, 0xc8a4, 0xc8a6, 0xc8a8, 0xc8aa, 0xc8ac, 0xc8ae, 0xc8b0, 0xc8b2, 0xc8b4, 0xc8cc, 0xc8ce,
	0xc8b6, 0xc8b8, 0xc8ba, 0xc8bc, 0xc8be, 0xc8c0, 0xc8c2, 0xc8c4, 0xc8c6, 0xc8d0, 0xc8d2, 0xc8d4, 0xc8d6, 0xc8d8,
};
static const uint16_t fn_color_awb_off_vals[FN_COLOR_AWB_TABLE_LEN] = {
	0x006a, 0x0119, 0x020c, 0xfed9, 0x001b, 0xffb0, 0x0171, 0xffde, 0xffbc, 0xff1c, 0x0228, 0x0094, 0x00ff,
	0x01cb, 0xff86, 0xffaf, 0xffa8, 0x0165, 0xfff2, 0xffee, 0xff7d, 0x0195, 0x00a9, 0x008d, 0x09c4, 0x0d67, 0x1964,
};
static const uint16_t fn_color_awb_on_vals[FN_COLOR_AWB_TABLE_LEN] = {
	0x001b, 0x004f, 0x01bf, 0xff01, 0xfff3, 0xff75, 0x0198, 0xfffd, 0xff9a, 0xfee7, 0x02a8, 0x0024, 0x0047,
	0x01d9, 0xff26, 0xfff3, 0xffb3, 0x0132, 0xffe8, 0xffda, 0xfecd, 0x02c2, 0x002b, 0x0027, 0x09c4, 0x0d67, 0x1964,
};
