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
 * The model loading and the body-part forest (nui/inflate.c, nui/forest.c).
 * The inflate part always runs; the forest part needs Microsoft's
 * DATABASE.WSDF (fwfetcher.py --nui, or LIBFREENECT_NUI_MODELS=DIR) and is
 * skipped (exit code 77) without it. The classifier's equivalence with
 * Kinect10 was checked separately against the reference's
 * tools/parse_wsdf.py classify() with float32 arithmetic: 0 of 19200 pixels
 * different on a synthetic two-user map. No output of Microsoft's model is
 * stored here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inflate.h"
#include "forest.h"
#include "models.h"
#include "test_inflate_vectors.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

static int inflates_to(const uint8_t* z, size_t n, const uint8_t* want, size_t want_n)
{
	uint8_t* out;
	size_t len;
	int ok;
	if (fn_zlib_inflate(z, n, &out, &len) != 0)
		return 0;
	ok = len == want_n && memcmp(out, want, len) == 0;
	free(out);
	return ok;
}

int main(void)
{
	uint8_t raw[3400], bad[sizeof(z_dynamic)], *db, *out;
	size_t i, n, len;
	char path[1024];

	printf("Model inflate (nui/inflate.c)\n");
	for (i = 0; i < 90; i++)
		raw[i] = (uint8_t)"libfreenect stored block test "[i % 30];
	CHECK(inflates_to(z_stored, sizeof(z_stored), raw, 90), "a stored block");
	CHECK(inflates_to(z_fixed, sizeof(z_fixed), (const uint8_t*)"abcabcabcabcabcabc hello hello hello", 36),
	      "fixed Huffman codes with back references");
	for (i = 0; i < 2000; i++)
		raw[i] = (uint8_t)((i * 7 + (i >> 3)) & 0xff);
	for (i = 0; i < 200; i++)
		memcpy(raw + 2000 + 7 * i, "Kinect ", 7);
	CHECK(inflates_to(z_dynamic, sizeof(z_dynamic), raw, 3400), "dynamic Huffman codes");
	memcpy(bad, z_dynamic, sizeof(bad));
	bad[sizeof(bad) - 1] ^= 1;
	CHECK(fn_zlib_inflate(bad, sizeof(bad), &out, &len) == -1, "a wrong Adler-32 is refused");
	CHECK(fn_zlib_inflate(z_dynamic, sizeof(z_dynamic) / 2, &out, &len) == -1, "a truncated stream is refused");
	CHECK(fn_zlib_inflate((const uint8_t*)"not zlib", 8, &out, &len) == -1, "not a zlib stream: refused");

	printf("Body-part forest (nui/forest.c)\n");
	db = fn_nui_read_model(NULL, FN_NUI_FOREST_FILE, &n, path, sizeof(path));
	if (!db) {
		printf("  DATABASE.WSDF not installed (fwfetcher.py --nui); the forest tests are skipped\n");
		printf(pass ? "Inflate passed; forest skipped\n" : "FAILURES\n");
		return pass ? 77 : 1;
	}
	printf("  (%s)\n", path);
	{
		fn_forest* f = fn_forest_load(db, n);
		static uint16_t map[FN_NUI_MAP_W * FN_NUI_MAP_H];
		static uint8_t lab[FN_NUI_LABEL_W * FN_NUI_LABEL_H * FN_NUI_PARTS], lab2[sizeof(lab)];
		float scale[3] = { 1.0f, 1.0f, 1.0f };
		int x, y, ok = 1, users = 0;
		CHECK(f && fn_forest_trees(f) == 3, "loads from the compressed resource: 3 trees");
		db[0] = 'X';
		CHECK(fn_forest_load(db, n) == NULL, "a damaged file is refused");
		if (f) {
			// a box-shaped "user" at about 2 m in slot 1
			memset(map, 0, sizeof(map));
			for (y = 60; y < 200; y++)
				for (x = 130; x < 190; x++)
					map[y * FN_NUI_MAP_W + x] = (uint16_t)(((2000 + (x - 160) * (x - 160) / 4) << 2) | 1);
			fn_forest_label(f, map, scale, lab);
			for (i = 0; i < FN_NUI_LABEL_W * FN_NUI_LABEL_H; i++) {
				int user = (map[(i / FN_NUI_LABEL_W) * 2 * FN_NUI_MAP_W + (i % FN_NUI_LABEL_W) * 2] & 3) != 0, c, sum = 0;
				for (c = 0; c < FN_NUI_PARTS; c++)
					sum += lab[i * FN_NUI_PARTS + c];
				if (user) {
					users++;
					ok &= sum >= 150 && sum <= 255;
				} else {
					for (c = 0; c < FN_NUI_PARTS; c++)
						ok &= lab[i * FN_NUI_PARTS + c] == 8;
				}
			}
			printf("    (%d user pixels classified)\n", users);
			CHECK(ok && users == 30 * 70, "user pixels get a part distribution, the rest 8 (as Kinect10)");
			fn_forest_label(f, map, scale, lab2);
			CHECK(memcmp(lab, lab2, sizeof(lab)) == 0, "the same frame gives the same labels");
			fn_forest_free(f);
		}
	}
	free(db);
	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
