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
 * Host-only test of the microphone array descriptor (src/mic_geometry.h):
 * the 84 bytes the audio firmware answers GET_MEM with (§40.5.3).
 */
#include <stdio.h>
#include <string.h>
#include "mic_geometry.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

static void put(uint8_t* p, int v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }

/* The descriptor as the firmware builds it (FUN_00081840). */
static void firmware_reply(uint8_t d[84])
{
	static const int y[4] = { -113, 36, 76, 113 };
	int i;
	memset(d, 0, 84);
	memcpy(d, fn_mic_array_guid, 16);
	put(d + 16, 0x54);   /* wDescriptorLength */
	put(d + 18, 0x0100); /* bcdVersion */
	put(d + 20, 0);      /* linear */
	put(d + 26, 0xDDEA); /* -50 degrees */
	put(d + 28, 0x2216); /* +50 degrees */
	put(d + 30, 200);
	put(d + 32, 0x1C20);
	put(d + 34, 4);
	for (i = 0; i < 4; i++) {
		put(d + 36 + 12 * i, 2);
		put(d + 36 + 12 * i + 4, y[i]);
	}
}

int main(void)
{
	uint8_t d[84];
	freenect_mic_geometry g, def;
	int ok, i;

	printf("Microphone array descriptor (src/mic_geometry.h)\n");
	firmware_reply(d);
	CHECK(fn_mic_geometry_parse(d, 84, &g) == 0, "the firmware's reply is read");
	CHECK(g.num_mics == 4 && g.array_type == 0, "  four microphones on a line");
	CHECK(g.mics[0].y_mm == -113 && g.mics[1].y_mm == 36 && g.mics[2].y_mm == 76 && g.mics[3].y_mm == 113,
	      "  at y = -113, 36, 76, 113 mm");
	ok = 1;
	for (i = 0; i < 4; i++)
		ok &= g.mics[i].type == 2 && g.mics[i].x_mm == 0 && g.mics[i].z_mm == 0;
	CHECK(ok, "  cardioid, x = z = 0");
	CHECK(g.horizontal_angle_begin == -8726 && g.horizontal_angle_end == 8726, "  working range -50..+50 degrees");
	CHECK(g.freq_low_hz == 200 && g.freq_high_hz == 7200, "  200-7200 Hz");

	fn_mic_geometry_default(&def);
	CHECK(memcmp(&g, &def, sizeof(g)) == 0, "the built-in values are the firmware's");

	memcpy(&def, &g, sizeof(g));
	CHECK(fn_mic_geometry_parse(d, 83, &g) == -1, "a short reply is refused");
	d[3] ^= 1;
	CHECK(fn_mic_geometry_parse(d, 84, &g) == -1, "another descriptor (GUID) is refused");
	d[3] ^= 1;
	put(d + 34, 5);
	CHECK(fn_mic_geometry_parse(d, 84, &g) == -1, "more microphones than fit is refused");
	put(d + 34, 0);
	CHECK(fn_mic_geometry_parse(d, 84, &g) == -1, "no microphones is refused");
	CHECK(memcmp(&g, &def, sizeof(g)) == 0, "  and a refused reply leaves the geometry unchanged");

	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
