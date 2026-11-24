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
 * The body-part forest (DATABASE.WSDF, reference §30.2): 3 randomized
 * decision trees of 20 levels over depth-difference features, sparse top-5
 * leaves, 31 body parts. Applied to every second pixel of the 320x240
 * classifier map (depth_mm << 2 | tracker slot 1..3) as Kinect10 does.
 */

#include <stddef.h>
#include <stdint.h>

#define FN_NUI_PARTS 31
#define FN_NUI_MAP_W 320
#define FN_NUI_MAP_H 240
#define FN_NUI_LABEL_W 160
#define FN_NUI_LABEL_H 120
#define FN_NUI_BACKGROUND_MM 6000   // what a probe off the user reads

typedef struct fn_forest fn_forest;

/* From the file as stored in Kinect10.dll (zlib) or already inflated.
 * NULL if it is not a WSDF forest. */
fn_forest* fn_forest_load(const uint8_t* data, size_t len);
void fn_forest_free(fn_forest* f);
int fn_forest_trees(const fn_forest* f);

/* The 31 part probabilities (1/255) of map pixel (x, y), as FUN_180099da0.
 * scale[slot - 1] is that user's body size factor (1.0 by default). */
void fn_forest_classify(const fn_forest* f, const uint16_t* map, const float scale[3], int x, int y,
                        uint8_t out[FN_NUI_PARTS]);

/* The whole 160x120 grid: out[(j * 160 + i) * 31 + part], 8 for pixels
 * without a user (as Kinect10). */
void fn_forest_label(const fn_forest* f, const uint16_t* map, const float scale[3], uint8_t* out);
