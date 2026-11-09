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
 * Hands and fingers, from the full 640x480 depth around a tracked hand
 * (Kinect10 has nothing like it; the SDK Toolkit only knew grip/release):
 *
 * 1. Region: grown from the depth pixel nearest the hand joint through
 *    depth-continuous neighbours (steps under 15 mm), within 18 cm of the
 *    hand joint and on the hand's side of the wrist.
 * 2. Palm: the region pixel farthest from the region's edge (city-block
 *    distance transform); that distance is the palm radius.
 * 3. Fingertips: points of the region's outline that stick out beyond
 *    1.7 palm radii from the palm centre, are sharp convex corners
 *    (k-curvature), do not point back towards the wrist and are not where
 *    the 18 cm limit cut the region; neighbours within 2 cm are one finger.
 *    Only for hands within 1.6 m (a finger is then at least 6 pixels wide)
 *    and at least 300 pixels big.
 * 4. Holding: a region that runs into the 18 cm limit other than towards
 *    the wrist continues past the hand, a held object (stick, handle).
 * 5. State: open (3 or more fingers, or a spread outline), gun (two
 *    fingers at more than 50 degrees: index and thumb), pointing (one
 *    finger), closed (no finger and compact, or holding); unknown when the
 *    hand is not seen well enough.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "nui_internal.h"
#include "hands.h"

#define F640 571.26f
#define WIN 256                // window side in 640x480 pixels (the whole reach from 0.8 m)
#define REACH 0.18f            // m from the hand joint
#define CUT 0.02f              // outline points this close to the reach limit are cuts, not tips
#define STEP_MM 15
#define MIN_PIXELS 40
#define FINGER_PIXELS 300
#define FINGER_MAX_Z 1.6f

// skeleton space <-> libfreenect's 640x480 (not mirrored)
static void to_pixel(v3 p, float* X, float* Y)
{
	float xm = p.x * F640 / p.z + 319.5f;
	*X = 639.0f - xm;
	*Y = -p.y * F640 / p.z + 239.5f;
}

static v3 from_pixel(int X, int Y, int d_mm)
{
	float z = d_mm * 0.001f, xm = (float)(639 - X);
	return v3make((xm - 319.5f) * z / F640, -((float)Y - 239.5f) * z / F640, z);
}

void fn_nui_hand_analyze(const uint16_t* depth, v3 hand, v3 wrist, freenect_nui_hand* out)
{
	static uint8_t in[WIN * WIN];
	static uint16_t dist[WIN * WIN];
	static int queue[WIN * WIN];
	float hx, hy;
	int x0, y0, sx = -1, sy = -1, best = 1 << 30, x, y, head = 0, tail = 0, n = 0;
	v3 axis = fn_nui_v3norm(v3sub(hand, wrist));
	memset(out, 0, sizeof(*out));
	if (hand.z < 0.3f)
		return;
	to_pixel(hand, &hx, &hy);
	x0 = (int)hx - WIN / 2;
	y0 = (int)hy - WIN / 2;
	// seed: the window pixel nearest (in 3-D) to the hand joint
	for (y = WIN / 2 - 8; y <= WIN / 2 + 8; y++)
		for (x = WIN / 2 - 8; x <= WIN / 2 + 8; x++) {
			int X = x0 + x, Y = y0 + y, d;
			v3 p;
			float e;
			if (X < 0 || X >= 640 || Y < 0 || Y >= 480 || !(d = depth[Y * 640 + X]))
				continue;
			p = from_pixel(X, Y, d);
			e = fn_nui_v3len(v3sub(p, hand));
			if (e < 0.08f && (int)(e * 1e5f) < best) {
				best = (int)(e * 1e5f);
				sx = x;
				sy = y;
			}
		}
	if (sx < 0)
		return;
	memset(in, 0, sizeof(in));
	in[sy * WIN + sx] = 1;
	queue[tail++] = sy * WIN + sx;
	while (head < tail) {
		int q = queue[head++], qx = q % WIN, qy = q / WIN, k, dq = depth[(y0 + qy) * 640 + x0 + qx];
		static const int dx4[4] = { 1, -1, 0, 0 }, dy4[4] = { 0, 0, 1, -1 };
		n++;
		for (k = 0; k < 4; k++) {
			int ax = qx + dx4[k], ay = qy + dy4[k], X = x0 + ax, Y = y0 + ay, d;
			v3 p;
			if (ax < 0 || ax >= WIN || ay < 0 || ay >= WIN || in[ay * WIN + ax])
				continue;
			if (X < 0 || X >= 640 || Y < 0 || Y >= 480 || !(d = depth[Y * 640 + X]) || abs(d - dq) > STEP_MM)
				continue;
			p = from_pixel(X, Y, d);
			if (fn_nui_v3len(v3sub(p, hand)) > REACH || v3dot(v3sub(p, wrist), axis) < -0.01f)
				continue;
			in[ay * WIN + ax] = 1;
			queue[tail++] = ay * WIN + ax;
		}
	}
	out->pixels = n;
	if (n < MIN_PIXELS)
		return;

	// palm: distance transform (two passes, city block)
	for (y = 0; y < WIN; y++)
		for (x = 0; x < WIN; x++) {
			int i = y * WIN + x, v = in[i] ? 10000 : 0;
			if (v) {
				if (x > 0 && dist[i - 1] + 1 < v) v = dist[i - 1] + 1;
				if (y > 0 && dist[i - WIN] + 1 < v) v = dist[i - WIN] + 1;
				if (x == 0 || y == 0) v = 1;
			}
			dist[i] = (uint16_t)v;
		}
	{
		int bi = -1, bd = 0;
		for (y = WIN - 1; y >= 0; y--)
			for (x = WIN - 1; x >= 0; x--) {
				int i = y * WIN + x, v = dist[i];
				if (!v)
					continue;
				if (x < WIN - 1 && dist[i + 1] + 1 < v) v = dist[i + 1] + 1;
				if (y < WIN - 1 && dist[i + WIN] + 1 < v) v = dist[i + WIN] + 1;
				if (x == WIN - 1 || y == WIN - 1) v = 1;
				dist[i] = (uint16_t)v;
				if (v > bd) {
					bd = v;
					bi = i;
				}
			}
		if (bi < 0)
			return;
		{
			int X = x0 + bi % WIN, Y = y0 + bi / WIN;
			v3 c = from_pixel(X, Y, depth[Y * 640 + X]);
			out->palm.x = c.x;
			out->palm.y = c.y;
			out->palm.z = c.z;
			out->palm_radius = (float)bd * c.z / F640;
		}
	}

	// outline: region pixels with a 4-neighbour outside, traced in order
	{
		static int ox[4 * WIN * WIN / 2], oy[4 * WIN * WIN / 2];
		int no = 0, cx = -1, cy = -1, dir = 0, steps = 0, k2;
		v3 palm = v3make(out->palm.x, out->palm.y, out->palm.z), towrist = fn_nui_v3norm(v3sub(wrist, palm));
		float farthest = 0.0f;
		static const int ndx[8] = { 1, 1, 0, -1, -1, -1, 0, 1 }, ndy[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
		for (y = 0; y < WIN && cx < 0; y++)
			for (x = 0; x < WIN; x++)
				if (in[y * WIN + x]) {
					cx = x;
					cy = y;
					break;
				}
		// Moore neighbour tracing
		{
			int sxo = cx, syo = cy, k;
			do {
				if (no < (int)(sizeof(ox) / sizeof(ox[0]))) {
					ox[no] = cx;
					oy[no] = cy;
					no++;
				}
				for (k = 0; k < 8; k++) {
					int d2 = (dir + 6 + k) & 7, ax = cx + ndx[d2], ay = cy + ndy[d2];
					if (ax >= 0 && ax < WIN && ay >= 0 && ay < WIN && in[ay * WIN + ax]) {
						cx = ax;
						cy = ay;
						dir = d2;
						break;
					}
				}
				if (k == 8)
					break; // a single pixel
			} while ((cx != sxo || cy != syo) && ++steps < 4 * WIN * WIN);
		}
		// extent of the outline from the palm (open/closed estimate), and
		// where the reach limit cut the region other than at the wrist (a
		// held object continues there)
		{
			int cut = 0;
			for (k2 = 0; k2 < no; k2++) {
				int X = x0 + ox[k2], Y = y0 + oy[k2];
				v3 p = from_pixel(X, Y, depth[Y * 640 + X]);
				float e = fn_nui_v3len(v3sub(p, palm));
				if (fn_nui_v3len(v3sub(p, hand)) >= REACH - CUT) {
					if (v3dot(fn_nui_v3norm(v3sub(p, palm)), towrist) < 0.5f)
						cut++;
					continue;
				}
				if (e > farthest)
					farthest = e;
			}
			out->holding = cut >= 6;
		}
		out->extent = farthest;
		// fingertips
		if (n >= FINGER_PIXELS && no > 24 && hand.z <= FINGER_MAX_Z) {
			int K = no / 24 < 4 ? 4 : no / 24, i;
			float minext = 1.7f * out->palm_radius;
			for (i = 0; i < no; i++) {
				int a = (i - K + no) % no, b = (i + K) % no, X = x0 + ox[i], Y = y0 + oy[i], f2, dup = 0;
				float ux = (float)(ox[a] - ox[i]), uy = (float)(oy[a] - oy[i]), vx = (float)(ox[b] - ox[i]), vy = (float)(oy[b] - oy[i]);
				float cosang = (ux * vx + uy * vy) / (sqrtf(ux * ux + uy * uy) * sqrtf(vx * vx + vy * vy) + 1e-6f);
				float mx = 0.5f * (ox[a] + ox[b]) - ox[i], my = 0.5f * (oy[a] + oy[b]) - oy[i];
				v3 p, dirv;
				if (cosang < 0.3f) // angle wider than ~73 degrees: not a tip
					continue;
				// convex: the midpoint of the two neighbours lies inside the region
				{
					int qx = ox[i] + (int)(mx * 0.5f), qy = oy[i] + (int)(my * 0.5f);
					if (qx < 0 || qx >= WIN || qy < 0 || qy >= WIN || !in[qy * WIN + qx])
						continue;
				}
				p = from_pixel(X, Y, depth[Y * 640 + X]);
				dirv = v3sub(p, palm);
				if (fn_nui_v3len(dirv) < minext || fn_nui_v3len(v3sub(p, hand)) >= REACH - CUT)
					continue;
				if (v3dot(fn_nui_v3norm(dirv), towrist) > 0.5f) // pointing back at the wrist
					continue;
				for (f2 = 0; f2 < out->num_fingers; f2++) {
					v3 q = v3make(out->fingertips[f2].x, out->fingertips[f2].y, out->fingertips[f2].z);
					if (fn_nui_v3len(v3sub(q, p)) < 0.02f) {
						dup = 1;
						// keep the one farther from the palm
						if (fn_nui_v3len(dirv) > fn_nui_v3len(v3sub(q, palm))) {
							out->fingertips[f2].x = p.x;
							out->fingertips[f2].y = p.y;
							out->fingertips[f2].z = p.z;
						}
						break;
					}
				}
				if (!dup && out->num_fingers < 5) {
					out->fingertips[out->num_fingers].x = p.x;
					out->fingertips[out->num_fingers].y = p.y;
					out->fingertips[out->num_fingers].z = p.z;
					out->num_fingers++;
				}
			}
			out->fingers_valid = 1;
		}
	}

	// state
	{
		float spread = out->palm_radius > 0.0f ? out->extent / out->palm_radius : 0.0f;
		v3 palm = v3make(out->palm.x, out->palm.y, out->palm.z);
		if (out->holding) {
			out->state = FREENECT_NUI_HAND_CLOSED; // gripping something
		} else if (out->fingers_valid) {
			if (out->num_fingers >= 3) out->state = FREENECT_NUI_HAND_OPEN;
			else if (out->num_fingers == 1) out->state = FREENECT_NUI_HAND_POINTING;
			else if (out->num_fingers == 2) {
				v3 a = fn_nui_v3norm(v3sub(v3make(out->fingertips[0].x, out->fingertips[0].y, out->fingertips[0].z), palm));
				v3 b = fn_nui_v3norm(v3sub(v3make(out->fingertips[1].x, out->fingertips[1].y, out->fingertips[1].z), palm));
				out->state = v3dot(a, b) < 0.643f ? FREENECT_NUI_HAND_GUN : FREENECT_NUI_HAND_OPEN; // 50 degrees
			}
			else if (out->num_fingers == 0 && spread < 2.2f) out->state = FREENECT_NUI_HAND_CLOSED;
			else out->state = spread >= 2.5f ? FREENECT_NUI_HAND_OPEN : FREENECT_NUI_HAND_CLOSED;
		} else {
			out->state = spread >= 2.6f ? FREENECT_NUI_HAND_OPEN : FREENECT_NUI_HAND_CLOSED;
		}
		out->confidence = n >= FINGER_PIXELS ? 1.0f : (float)n / FINGER_PIXELS;
	}
}
