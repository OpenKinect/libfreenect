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
 * The microphone array descriptor (§40.5.3): the audio firmware answers the
 * USB Audio class request GET_MEM (bmRequestType 0xA1, bRequest 0x85) with
 * an 84-byte Microsoft microphone array descriptor, the same in audios.bin
 * and in UAC.BIN. It does not check the other setup fields.
 */

#include <stdint.h>
#include <string.h>
#include "libfreenect_audio.h"

#define FN_MIC_GEOMETRY_REQUEST_TYPE 0xA1
#define FN_MIC_GEOMETRY_REQUEST 0x85
#define FN_MIC_GEOMETRY_SIZE 84

static const uint8_t fn_mic_array_guid[16] = {
	0x07, 0xFE, 0x86, 0xC1, 0x89, 0x48, 0x4D, 0xB5, 0xB1, 0x84, 0xC5, 0x16, 0x2D, 0x4A, 0xD3, 0x14
};

static inline uint16_t fn_mic_u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// What the firmware reports: four cardioid microphones on a line, at
// y = -113, 36, 76 and 113 mm; working range +-50 degrees, 200-7200 Hz.
static inline void fn_mic_geometry_default(freenect_mic_geometry* g)
{
	static const int16_t y[4] = { -113, 36, 76, 113 };
	int i;
	memset(g, 0, sizeof(*g));
	g->array_type = 0;
	g->horizontal_angle_begin = -8726;
	g->horizontal_angle_end = 8726;
	g->freq_low_hz = 200;
	g->freq_high_hz = 7200;
	g->num_mics = 4;
	for (i = 0; i < 4; i++) {
		g->mics[i].type = 2;
		g->mics[i].y_mm = y[i];
	}
}

// Parses a GET_MEM reply of n bytes. Returns 0, or -1 if it is not a
// microphone array descriptor (g is then unchanged).
static inline int fn_mic_geometry_parse(const uint8_t* d, int n, freenect_mic_geometry* g)
{
	int i, mics, len;
	if (n < 36 || memcmp(d, fn_mic_array_guid, 16) != 0)
		return -1;
	len = fn_mic_u16(d + 16);
	mics = fn_mic_u16(d + 34);
	if (len > n || mics < 1 || mics > FREENECT_MAX_MICS || 36 + 12 * mics > len)
		return -1;
	memset(g, 0, sizeof(*g));
	g->array_type = fn_mic_u16(d + 20);
	g->vertical_angle_begin = (int16_t)fn_mic_u16(d + 22);
	g->vertical_angle_end = (int16_t)fn_mic_u16(d + 24);
	g->horizontal_angle_begin = (int16_t)fn_mic_u16(d + 26);
	g->horizontal_angle_end = (int16_t)fn_mic_u16(d + 28);
	g->freq_low_hz = fn_mic_u16(d + 30);
	g->freq_high_hz = fn_mic_u16(d + 32);
	g->num_mics = mics;
	for (i = 0; i < mics; i++) {
		const uint8_t* m = d + 36 + 12 * i;
		g->mics[i].type = fn_mic_u16(m);
		g->mics[i].x_mm = (int16_t)fn_mic_u16(m + 2);
		g->mics[i].y_mm = (int16_t)fn_mic_u16(m + 4);
		g->mics[i].z_mm = (int16_t)fn_mic_u16(m + 6);
		g->mics[i].vertical_angle = (int16_t)fn_mic_u16(m + 8);
		g->mics[i].horizontal_angle = (int16_t)fn_mic_u16(m + 10);
	}
	return 0;
}
