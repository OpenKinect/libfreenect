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
 * libfreenect_nui internals. Images inside the tracker are 320x240 and
 * horizontally mirrored, as Kinect10's (the forest was trained on that
 * orientation): map column u is depth column 638 - 2u / 639 - 2u. Points
 * are in the SDK's skeleton space, metres: x towards the mirrored image's
 * right, y up, z away from the Kinect (the coordinate mapper's camera space).
 */

#include <stdint.h>
#include "libfreenect_nui.h"
#include "forest.h"

#define NW FN_NUI_MAP_W            // 320
#define NH FN_NUI_MAP_H            // 240
#define NPIX (NW * NH)
#define NJ FREENECT_NUI_BODY_JOINTS // 20: the fit and the bones; hands are joints 20-31
#define NU FREENECT_NUI_MAX_USERS  // 6

// 320x240 nominal intrinsics (§30.5.4.1.1): f = 285.63 px, centre 159.5/119.5
#define FN_NUI_F 285.63f
#define FN_NUI_CX 159.5f
#define FN_NUI_CY 119.5f

typedef struct {
	float x, y, z;
} v3;

static inline v3 v3make(float x, float y, float z) { v3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline v3 v3add(v3 a, v3 b) { return v3make(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline v3 v3sub(v3 a, v3 b) { return v3make(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline v3 v3scale(v3 a, float s) { return v3make(a.x * s, a.y * s, a.z * s); }
static inline float v3dot(v3 a, v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline v3 v3cross(v3 a, v3 b) { return v3make(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
float fn_nui_v3len(v3 a);
v3 fn_nui_v3norm(v3 a); // unchanged if zero

// Map pixel (u, v) at depth z (mm) -> skeleton space (m)
static inline v3 fn_nui_unproject(float u, float v, float z_mm)
{
	float z = z_mm * 0.001f;
	return v3make((u - FN_NUI_CX) * z / FN_NUI_F, -(v - FN_NUI_CY) * z / FN_NUI_F, z);
}
// and back (pixel coordinates in the mirrored 320x240 map)
static inline void fn_nui_project(v3 p, float* u, float* v)
{
	*u = p.x * FN_NUI_F / p.z + FN_NUI_CX;
	*v = -p.y * FN_NUI_F / p.z + FN_NUI_CY;
}

/* ------------------------------------------------------------ segmentation */

typedef struct {
	int active;            // slot in use
	uint32_t id;           // tracking id (never 0)
	int index;             // player index 1..6 (slot + 1)
	int age, missing;      // frames seen, frames since last seen
	int verified;          // the forest found a body here at least once
	v3 centre;             // mean of its pixels (m)
	int pixels;
	float scale;           // body size factor for the forest (§30.4.7)
	int umin, umax, vmin, vmax;
} fn_nui_user_seg;

typedef struct {
	uint16_t* bg;          // background depth per map pixel (mm, 0 unknown)
	uint16_t* bgage;
	uint16_t* label;       // component per map pixel
	uint8_t* user;         // user slot + 1 per map pixel (0: none), this frame
	uint8_t* prev_user;    // the same, previous frame
	uint32_t* parent;      // union-find work
	float floor[4];        // A, B, C (unit), D (mm); all 0 if none
	int floor_valid;
	uint32_t rng;
	uint32_t next_id;
	int frames;
	fn_nui_user_seg u[NU];
} fn_nui_seg;

int fn_nui_seg_init(fn_nui_seg* s);
void fn_nui_seg_free(fn_nui_seg* s);
void fn_nui_seg_reset(fn_nui_seg* s);
// 640x480 mm depth (libfreenect orientation, 0 = none) -> mirrored 320x240 map
void fn_nui_make_map(const uint16_t* depth, uint16_t* map);
// Floor, background, components and user identities for one map.
void fn_nui_segment(fn_nui_seg* s, const uint16_t* map, int seated);
// Distance of a map point from the floor (mm); large if no floor
float fn_nui_floor_dist(const fn_nui_seg* s, v3 p_m);

/* ------------------------------------------------------------ proposals */

#define FN_NUI_MAX_CAND 8

typedef struct {
	v3 p;
	float conf;   // 0..1
} fn_nui_cand;

typedef struct {
	fn_nui_cand c[NJ][FN_NUI_MAX_CAND];
	int n[NJ];
} fn_nui_props;

// Joint proposals for one user from the part probabilities of its pixels
// (prob[(v * 320 + u) * 31 + part], only where mask == slot + 1).
struct fn_pool;
void fn_nui_proposals(const uint16_t* map, const uint8_t* mask, int slot, const uint8_t* prob,
                      float scale, fn_nui_props* out, struct fn_pool* pool);

/* ------------------------------------------------------------ fitting */

typedef struct {
	int state;                   // 0 none, 1 position only, 2 tracked
	int lock_frames;             // consecutive good fits
	int lost_frames;
	v3 joint[NJ], prev[NJ], vel[NJ];
	int jstate[NJ];
	int seen[NJ];                // frames since the joint had a candidate
	float bone[NJ];              // learned length of the bone ending at joint j
	float bone_acc[NJ][64];      // samples while learning
	int bone_n[NJ];
	int bones_locked;
	int learn_frames;             // good frames seen while learning the bones
	int has_prev;
	uint32_t quality;
} fn_nui_fit;

void fn_nui_fit_reset(fn_nui_fit* f, float scale);
// Returns 1 if the skeleton fits well enough this frame.
int fn_nui_fit_user(fn_nui_fit* f, const fn_nui_props* props, const uint16_t* map, const uint8_t* mask,
                    int slot, float scale);

/* ------------------------------------------------------------ output */

extern const int fn_nui_parent[NJ];
void fn_nui_holt(const freenect_nui_smoothing* params, void* state, freenect_nui_skeleton* s);
