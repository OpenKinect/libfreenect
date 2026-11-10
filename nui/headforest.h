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

#include <stddef.h>
#include <stdint.h>
#include "nui_internal.h"

#define FN_HD_W 80
#define FN_HD_H 60
#define FN_HD_POOL 40   // candidates and tracks
#define FN_HD_SLOTS 16  // output records

/* HEADPOSITION.MSHDF, decoded (§30.3.2) */
typedef struct {
	float ax, ay, bx, by; // probe offsets in mm*cells: (byte - 127) * 256
	int16_t thr;          // mm
	uint8_t two;          // B is a probe (else the centre depth)
	uint8_t leaf_r, leaf_l;
} fn_hd_node;

typedef struct {
	fn_hd_node* nodes; // ntrees * nodes_per_tree
	int ntrees, nodes_per_tree, depth;
} fn_hd_forest;

/* Loads the file (as shipped: big-endian, or native). 0, or < 0 if it is
 * not a version 2 MSHDF with the known format id. */
int fn_hd_forest_load(fn_hd_forest* f, const uint8_t* data, size_t len);
void fn_hd_forest_free(fn_hd_forest* f);

typedef struct {
	v3 pos;               // m, skeleton space
	float score;
	int box[4];           // 80x60 cells: left, top, right, bottom (inclusive)
	float mw, mh;         // the box in metres
} fn_hd_candidate;

typedef struct {
	int state;            // 0 free, 1 tentative, 2 confirmed, 3 lost
	uint32_t id;          // confirmed and lost tracks
	int slot;             // output slot of confirmed and lost tracks, -1 otherwise
	int hits, misses;
	float score, hist[15];
	int nhist, hpos;
	v3 pos;
	int box[4];
	int matched;
} fn_hd_track;

typedef struct {
	fn_hd_forest forest;
	uint16_t D[FN_HD_W * FN_HD_H]; // mm, 6000 for no depth
	uint8_t P[FN_HD_W * FN_HD_H];  // head probability 0..255
	uint8_t B[FN_HD_W * FN_HD_H];  // blurred
	fn_hd_candidate cand[FN_HD_POOL];
	int ncand;
	fn_hd_track track[FN_HD_POOL];
	uint32_t next_id;
} fn_hd;

/* One frame of the mirrored 320x240 depth map (mm): detection (§30.3.4)
 * and tracking (§30.3.5). */
void fn_hd_detect(fn_hd* h, const uint16_t* map320);
void fn_hd_track_update(fn_hd* h);
/* The output records (§30.3.5): confirmed and lost tracks by slot, ranked. */
int fn_hd_output(const fn_hd* h, freenect_nui_head out[FN_HD_SLOTS]);
