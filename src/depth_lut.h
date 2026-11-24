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
#include "libfreenect.h"

/*
 * Raw 11-bit disparity -> millimetres, with Microsoft's runtime's formula
 * (§20.2.1, §20.2.2) and the device's own calibration:
 *   E    = fixed param 23, fDCmosEmitterDistance (cm)
 *   Zref = fixed param 25, fReferenceDistance (cm)
 *   P    = fixed param 26, fReferencePixelSize (cm)
 *   c    = const_shift (cmd 0x16, param 0)
 *
 *   n = k - 4c;  d = (n * 0.25) * P;  z = ((d * Zref) / (E - d) + Zref) * 10
 *   table[k] = trunc(z) if 0 < z <= 65535, else 0 ("no value");  table[0] = 0
 *
 * Then the range: values below lo or above hi become 0; lo and hi
 * themselves are kept. The default range, FREENECT_DEPTH_RANGE_DEFAULT, is
 * 800-4000 mm; FREENECT_DEPTH_RANGE_FULL keeps them all.
 *
 * Deliberate differences from the SDK:
 * - The SDK truncates Zref to whole centimetres. Here it's used as the
 *   device reports it, which is more accurate.
 * - Where z exceeds 65535 (right next to the formula's singularity, d -> E),
 *   the SDK stores only the low 16 bits, so a few raw values map to
 *   arbitrary small distances. Here they get 0, as libfreenect always did.
 * - Index 2047, the camera's "no data" value, is always 0.
 */

#define FN_DEPTH_LUT_SIZE 2048

static inline void fn_depth_range_limits(freenect_depth_range range, int *lo, int *hi)
{
	switch (range) {
		case FREENECT_DEPTH_RANGE_NEAR:
			*lo = 400;
			*hi = 3000;
			break;
		case FREENECT_DEPTH_RANGE_FULL:
			*lo = 1;
			*hi = 65535;
			break;
		case FREENECT_DEPTH_RANGE_DEFAULT:
		default:
			*lo = 800;
			*hi = 4000;
			break;
	}
}

static inline void fn_build_depth_lut(uint16_t *lut, float E_cm, float Zref_cm, float P_cm, uint16_t const_shift,
                                      freenect_depth_range range)
{
	const double E = (double)E_cm;
	const double Zref = (double)Zref_cm;
	const double P = (double)P_cm;
	int lo, hi, k;

	fn_depth_range_limits(range, &lo, &hi);
	lut[0] = 0;
	for (k = 1; k < FN_DEPTH_LUT_SIZE; k++) {
		int32_t n = k - 4 * (int32_t)const_shift;
		double d = ((double)n * 0.25) * P;
		double z = ((d * Zref) / (E - d) + Zref) * 10.0;
		int64_t v = (z > 0.0 && z < 65536.0) ? (int64_t)z : 0;
		lut[k] = (v >= lo && v <= hi) ? (uint16_t)v : 0;
	}
	lut[FREENECT_DEPTH_RAW_NO_VALUE] = FREENECT_DEPTH_MM_NO_VALUE;
}
