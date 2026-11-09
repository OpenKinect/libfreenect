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

#include "nui_internal.h"

/* Eigen decomposition of a symmetric 3x3 matrix (Jacobi), eigenvalues in
 * descending order with their unit eigenvectors. */
void fn_nui_eig3(const double A[3][3], double val[3], v3 vec[3]);

/* A rotation from its axes (Y and Z, made orthonormal; X = Y x Z). */
void fn_nui_rotation_from_axes(v3 y, v3 z, freenect_nui_rotation* r);

/* The rotation taking the orthonormal basis (u0, v0, w0) onto (u1, v1, w1),
 * as a quaternion (x, y, z, w) with w >= 0; returns its angle in radians. */
float fn_nui_quat_between(const v3 b0[3], const v3 b1[3], float q[4]);

/* Two-hand manipulation state of one user. */
typedef struct {
	int on, hits, misses;
	freenect_nui_grip grip;
	v3 L, R, dir;          // smoothed palms and the sum of the hand directions
	v3 b_ref[3];           // basis, distance, centre at the last reported delta
	float d_ref;
	v3 c_ref;
	v3 b0[3];              // and when the grip began
	float d0;
	v3 c0;
} fn_nui_manip;

/* Which grip two hands show (FREENECT_NUI_GRIP_NONE if none that `allowed`
 * accepts): palms tracked, both at least 15 cm in front of the shoulder
 * centre, 8 cm to 1.2 m apart; closed fists, or open hands whose palms face
 * each other (normals within about 55 degrees of the line between them), or
 * the triangle (both hands thumb and index out, thumbs towards each other;
 * thumb_l, thumb_r: the thumb tips). */
freenect_nui_grip fn_nui_grip_of(freenect_nui_grip allowed, int tracked_l, int tracked_r, freenect_nui_hand_state state_l,
                                 freenect_nui_hand_state state_r, v3 L, v3 R, v3 normal_l, v3 normal_r, v3 thumb_l, v3 thumb_r,
                                 v3 shoulder);

void fn_nui_manip_reset(fn_nui_manip* m);
/* One frame: grip (NONE if not held), the palms and the hands' directions. */
void fn_nui_manip_step(fn_nui_manip* m, freenect_nui_grip grip, v3 L, v3 R, v3 dir_l, v3 dir_r,
                       freenect_nui_manipulation* out);

/* One hand as a pointer (cursor, press, grip, swipes). */
#define FN_NUI_POINTER_HISTORY 15 /* frames: half a second */
typedef struct {
	v3 hist[FN_NUI_POINTER_HISTORY]; // raw hand positions, newest last
	int hist_active[FN_NUI_POINTER_HISTORY]; // and whether the hand was active then
	int nh;
	int cooldown;                    // frames until the next swipe
	float rest;                      // the resting reach towards the Kinect
	int rest_set;
	int active, pressed, gripped;
	int active_frames;               // since the hand entered the zone
	float cx, cy;                    // smoothed cursor
	v3 last;
} fn_nui_pointer;

void fn_nui_pointer_reset(fn_nui_pointer* p);
/* One frame of one hand (right: 1 for the right hand). hand: the hand
 * joint; quiet: report no events (two-hand gestures are on). */
void fn_nui_pointer_step(fn_nui_pointer* p, int right, int tracked, v3 hand, freenect_nui_hand_state state, v3 shoulder_side,
                         v3 shoulder_center, v3 hip_center, int quiet, freenect_nui_hand_pointer* out);
