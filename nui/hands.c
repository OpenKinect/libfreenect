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
#include "gestures.h"

#define F640 571.26f
#define WIN 256                // window side in 640x480 pixels (the whole reach from 0.8 m)
#define REACH 0.18f            // m from the hand joint
#define CUT 0.02f              // outline points this close to the reach limit are cuts, not tips
#define STEP_MM 15
#define MIN_PIXELS 40
#define FINGER_PIXELS 300
#define FINGER_MAX_Z 1.6f
#define CLASP_APART 0.16f      // m between the hand joints: the hands are together

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

/*
 * Which fingertip is which (into slots thumb, index, middle, ring, little):
 * - the thumb is the tip that leaves the hand's axis (wrist -> palm) at the
 *   widest angle, at least 35 degrees and 15 degrees wider than the next;
 *   a single tip is the thumb when it is more than 50 degrees off the axis
 *   (a thumbs-up), else the index;
 * - the others are counted from the thumb's side, or, without a thumb, from
 *   the side facing the body's midline (as when the palm faces the Kinect):
 *   index, middle, ring, little; with fewer than four, the first slots.
 */
static void name_fingers(freenect_nui_hand* out, v3 wrist, v3 midline)
{
	v3 tips[5], palm = v3make(out->palm.x, out->palm.y, out->palm.z), axis, side, d[5];
	float ang[5], comp[5];
	int n = out->num_fingers, i, thumb = -1, order[5], k;
	for (i = 0; i < n; i++)
		tips[i] = v3make(out->fingertips[i].x, out->fingertips[i].y, out->fingertips[i].z);
	memset(out->fingertips, 0, sizeof(out->fingertips));
	memset(out->finger_found, 0, sizeof(out->finger_found));
	if (n == 0)
		return;
	// In the image plane (x, y): the fingers are found on the silhouette, and
	// the palm centre lies on the hand's front while the wrist joint is
	// inside the arm, which would tilt a 3-D axis towards the Kinect.
	axis = v3sub(palm, wrist);
	axis.z = 0.0f;
	axis = fn_nui_v3norm(axis);
	for (i = 0; i < n; i++) {
		float c;
		d[i] = v3sub(tips[i], palm);
		d[i].z = 0.0f;
		d[i] = fn_nui_v3norm(d[i]);
		c = v3dot(d[i], axis);
		ang[i] = acosf(c > 1.0f ? 1.0f : c < -1.0f ? -1.0f : c) * 57.29578f;
	}
	if (n == 1) {
		if (ang[0] > 50.0f)
			thumb = 0;
	} else {
		// signed angles around the axis, then the fan in order: the four
		// fingers spread evenly, the thumb stands apart at one end
		float th[5] = { 0 }, gap_lo, gap_hi, other = 0.0f;
		int ord[5] = { 0 }, a, b;
		for (i = 0; i < n; i++) {
			th[i] = atan2f(axis.x * d[i].y - axis.y * d[i].x, v3dot(axis, d[i])) * 57.29578f;
			ord[i] = i;
		}
		for (a = 1; a < n; a++)
			for (b = a; b > 0 && th[ord[b - 1]] > th[ord[b]]; b--) {
				int t = ord[b];
				ord[b] = ord[b - 1];
				ord[b - 1] = t;
			}
		gap_lo = th[ord[1]] - th[ord[0]];
		gap_hi = th[ord[n - 1]] - th[ord[n - 2]];
		if (n == 2) {
			// index and thumb (a gun) are far apart, two fingers (a V) are not;
			// the thumb is the one farther off the axis
			if (gap_lo >= 50.0f)
				thumb = ang[ord[0]] > ang[ord[1]] ? ord[0] : ord[1];
		} else {
			for (a = 1; a < n - 2; a++)
				if (th[ord[a + 1]] - th[ord[a]] > other)
					other = th[ord[a + 1]] - th[ord[a]];
			if (gap_hi >= gap_lo && gap_hi >= 25.0f && gap_hi >= 1.4f * (other > gap_lo ? other : gap_lo))
				thumb = ord[n - 1];
			else if (gap_lo > gap_hi && gap_lo >= 25.0f && gap_lo >= 1.4f * (other > gap_hi ? other : gap_hi))
				thumb = ord[0];
		}
	}
	if (thumb >= 0) {
		side = v3sub(d[thumb], v3scale(axis, v3dot(d[thumb], axis)));
	} else {
		v3 m = v3sub(midline, palm);
		m.z = 0.0f;
		side = v3sub(m, v3scale(axis, v3dot(m, axis)));
	}
	side = fn_nui_v3norm(side);
	for (i = 0, k = 0; i < n; i++)
		if (i != thumb) {
			comp[i] = v3dot(d[i], side);
			order[k++] = i;
		}
	// nearest the thumb's side first
	{
		int a, b;
		for (a = 1; a < k; a++)
			for (b = a; b > 0 && comp[order[b - 1]] < comp[order[b]]; b--) {
				int t2 = order[b];
				order[b] = order[b - 1];
				order[b - 1] = t2;
			}
	}
	if (thumb >= 0) {
		out->fingertips[0].x = tips[thumb].x;
		out->fingertips[0].y = tips[thumb].y;
		out->fingertips[0].z = tips[thumb].z;
		out->finger_found[0] = 1;
	}
	for (i = 0; i < k && i < 4; i++) {
		out->fingertips[1 + i].x = tips[order[i]].x;
		out->fingertips[1 + i].y = tips[order[i]].y;
		out->fingertips[1 + i].z = tips[order[i]].z;
		out->finger_found[1 + i] = 1;
	}
}

/*
 * Orientation:
 * - direction: from where the forearm enters the hand region (the centroid
 *   of its pixels within 1.5 cm of the wrist) to the palm centre. Both lie
 *   on the visible surface, so the direction does not tilt towards the
 *   Kinect, and the extended thumb does not pull it sideways;
 * - normal: of the plane through the pixels around the palm (their least
 *   spread). A hand seen edge-on shows only its narrow side, whose pixels
 *   lie flat towards the Kinect; the thinner the hand is across the
 *   direction in the image, the more the normal turns to the image-plane
 *   perpendicular of the direction.
 */
static void orient(const uint16_t* depth, int x0, int y0, const uint8_t* in, v3 wrist, v3 axis, v3 midline, freenect_nui_hand* out)
{
	v3 palm = v3make(out->palm.x, out->palm.y, out->palm.z), vp[3], dir, nrm, view, edge, wc = v3make(0, 0, 0), hc = v3make(0, 0, 0);
	double sp[3] = { 0, 0, 0 }, Cp[3][3], ep[3], across = 0.0;
	double spp[3][3] = { { 0 } };
	float rp = 1.5f * out->palm_radius, thin;
	int np = 0, nw = 0, nh = 0, x, y, i, j;
	if (rp < 0.03f)
		rp = 0.03f;
	for (y = 0; y < WIN; y++)
		for (x = 0; x < WIN; x++) {
			int X = x0 + x, Y = y0 + y;
			v3 p;
			double c[3];
			if (!in[y * WIN + x])
				continue;
			p = from_pixel(X, Y, depth[Y * 640 + X]);
			c[0] = p.x; c[1] = p.y; c[2] = p.z;
			if (fn_nui_v3len(v3sub(p, palm)) < rp && v3dot(v3sub(p, wrist), axis) >= 0.025f) {
				for (i = 0; i < 3; i++) {
					sp[i] += c[i];
					for (j = 0; j < 3; j++)
						spp[i][j] += c[i] * c[j];
				}
				np++;
			}
			if (v3dot(v3sub(p, wrist), axis) < 0.015f) {
				wc = v3add(wc, p);
				nw++;
			} else if (v3dot(v3sub(p, wrist), axis) >= 0.025f) {
				hc = v3add(hc, p);
				nh++;
			}
		}
	if (np < 10 || nh < 10)
		return;
	hc = v3scale(hc, 1.0f / nh);
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			Cp[i][j] = spp[i][j] / np - (sp[i] / np) * (sp[j] / np);
	fn_nui_eig3(Cp, ep, vp);
	view = fn_nui_v3norm(palm);
	wc = nw >= 5 ? v3scale(wc, 1.0f / nw) : wrist;
	dir = v3sub(hc, wc);
	if (fn_nui_v3len(dir) < 0.02f)
		dir = v3sub(hc, wrist);
	dir = fn_nui_v3norm(dir);
	nrm = vp[2];
	if (v3dot(nrm, view) > 0.0f)
		nrm = v3scale(nrm, -1.0f);
	// edge-on: the hand's spread across the direction in the image (a flat
	// hand's standard deviation there is about 2 cm, its edge's under 1 cm)
	edge = fn_nui_v3norm(v3cross(dir, view));
	{
		double e[3] = { edge.x, edge.y, edge.z };
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++)
				across += e[i] * Cp[i][j] * e[j];
	}
	thin = (float)((sqrt(across > 0 ? across : 0) - 0.009) / (0.015 - 0.009));
	thin = thin < 0.0f ? 0.0f : thin > 1.0f ? 1.0f : thin;
	if (v3dot(edge, v3sub(palm, midline)) < 0.0f)
		edge = v3scale(edge, -1.0f);
	if (thin < 1.0f) {
		// the fitted normal leans towards the edge's side; keep its sign
		if (v3dot(nrm, edge) < 0.0f && thin > 0.5f)
			edge = v3scale(edge, -1.0f);
		nrm = fn_nui_v3norm(v3add(v3scale(nrm, thin), v3scale(edge, 1.0f - thin)));
	}
	// the direction in the hand's plane
	dir = fn_nui_v3norm(v3sub(dir, v3scale(nrm, v3dot(dir, nrm))));
	out->direction.x = dir.x; out->direction.y = dir.y; out->direction.z = dir.z;
	out->normal.x = nrm.x; out->normal.y = nrm.y; out->normal.z = nrm.z;
	fn_nui_rotation_from_axes(dir, nrm, &out->orientation);
}

/*
 * A finger pointing at the Kinect does not stick out of the outline, only in
 * depth: the region's nearest pixel lies more than 5 cm in front of the
 * region's mean depth, and the pixels within 3 cm behind it stay within
 * 3.5 cm of it (a finger, not a whole hand tipped towards the Kinect).
 */
static int forward_tip(const uint16_t* depth, int x0, int y0, const uint8_t* in, v3* tip)
{
	int x, y, best = 1 << 30, bx = -1, by = -1, n = 0, far = 0;
	double mean = 0.0;
	int count = 0;
	v3 t;
	for (y = 0; y < WIN; y++)
		for (x = 0; x < WIN; x++)
			if (in[y * WIN + x]) {
				int d = depth[(y0 + y) * 640 + x0 + x];
				mean += d;
				count++;
				if (d < best) {
					best = d;
					bx = x;
					by = y;
				}
			}
	if (bx < 0 || mean / count - best < 50.0)
		return 0;
	t = from_pixel(x0 + bx, y0 + by, best);
	for (y = 0; y < WIN; y++)
		for (x = 0; x < WIN; x++)
			if (in[y * WIN + x]) {
				int d = depth[(y0 + y) * 640 + x0 + x];
				if (d > best + 30)
					continue;
				n++;
				if (fn_nui_v3len(v3sub(from_pixel(x0 + x, y0 + y, d), t)) > 0.035f)
					far++;
			}
	if (n < 8 || far > n / 10)
		return 0;
	*tip = t;
	return 1;
}

void fn_nui_clasp_analyze(const uint16_t* depth, v3 hand_l, v3 hand_r, v3 wrist_l, v3 wrist_r, v3 midline, freenect_nui_clasp* out)
{
	freenect_nui_hand h;
	v3 hand = v3scale(v3add(hand_l, hand_r), 0.5f), wrist = v3scale(v3add(wrist_l, wrist_r), 0.5f), arm, best_tip = hand;
	float best = 0.0f;
	int f;
	memset(out, 0, sizeof(*out));
	if (hand_l.z < 0.3f || hand_r.z < 0.3f || fn_nui_v3len(v3sub(hand_l, hand_r)) > CLASP_APART)
		return;
	fn_nui_hand_analyze(depth, hand, wrist, midline, &h);
	if (h.state == FREENECT_NUI_HAND_UNKNOWN)
		return;
	out->clasped = 1;
	out->center = h.palm;
	out->state = FREENECT_NUI_HAND_CLOSED;
	arm = fn_nui_v3norm(v3sub(hand, wrist));
	// the fingers out along the arms; a thumb pointing up is not the aim
	for (f = 1; f < 5; f++) {
		v3 t = v3make(h.fingertips[f].x, h.fingertips[f].y, h.fingertips[f].z), d;
		float along;
		if (!h.finger_found[f])
			continue;
		d = v3sub(t, wrist);
		along = v3dot(fn_nui_v3norm(d), arm);
		if ((along > 0.7f || (f == 1 && h.finger_towards_kinect)) && fn_nui_v3len(d) > best) {
			best = fn_nui_v3len(d);
			best_tip = t;
		}
	}
	if (best > 0.0f) {
		v3 dir = fn_nui_v3norm(v3sub(best_tip, wrist));
		out->state = h.finger_found[0] ? FREENECT_NUI_HAND_GUN : FREENECT_NUI_HAND_POINTING;
		out->tip.x = best_tip.x;
		out->tip.y = best_tip.y;
		out->tip.z = best_tip.z;
		out->direction.x = dir.x;
		out->direction.y = dir.y;
		out->direction.z = dir.z;
		out->towards_kinect = h.finger_towards_kinect;
	}
}

void fn_nui_hand_analyze(const uint16_t* depth, v3 hand, v3 wrist, v3 midline, freenect_nui_hand* out)
{
	static uint8_t in[WIN * WIN];
	static uint16_t dist[WIN * WIN];
	static int queue[WIN * WIN];
	float hx, hy;
	int x0, y0, sx = -1, sy = -1, best = 1 << 30, x, y, head = 0, tail = 0, n = 0;
	v3 axis = fn_nui_v3norm(v3sub(hand, wrist));
	memset(out, 0, sizeof(*out));
	fn_nui_rotation_from_axes(v3make(0.0f, 1.0f, 0.0f), v3make(0.0f, 0.0f, 1.0f), &out->orientation);
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
	orient(depth, x0, y0, in, wrist, axis, midline, out);

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
			// k-curvature over about 3 cm of outline: past a fingertip's
			// rounded end, down its sides
			int K = no / 24, i, Kmin = (int)(0.03f * F640 / hand.z);
			if (K < Kmin)
				K = Kmin;
			if (K < 4)
				K = 4;
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
			name_fingers(out, wrist, midline);
		}
	}

	// a finger pointing at the Kinect: the index; one other finger out of the
	// outline with it is the thumb (a gun aimed at the Kinect)
	if (out->fingers_valid && !out->holding && out->num_fingers <= 1) {
		v3 tip;
		int f, other = -1;
		for (f = 0; f < 5; f++)
			if (out->finger_found[f])
				other = f;
		if (forward_tip(depth, x0, y0, in, &tip) &&
		    (other < 0 || fn_nui_v3len(v3sub(tip, v3make(out->fingertips[other].x, out->fingertips[other].y,
		                                                 out->fingertips[other].z))) > 0.03f)) {
			if (other > 0) {
				out->fingertips[0] = out->fingertips[other];
				out->finger_found[0] = 1;
				out->finger_found[other] = 0;
			}
			out->finger_towards_kinect = 1;
			out->fingertips[1].x = tip.x;
			out->fingertips[1].y = tip.y;
			out->fingertips[1].z = tip.z;
			out->finger_found[1] = 1;
			out->num_fingers++;
		}
	}

	// state
	{
		float spread = out->palm_radius > 0.0f ? out->extent / out->palm_radius : 0.0f;
		if (out->holding) {
			out->state = FREENECT_NUI_HAND_CLOSED; // gripping something
		} else if (out->fingers_valid) {
			if (out->num_fingers >= 3) out->state = FREENECT_NUI_HAND_OPEN;
			else if (out->num_fingers == 1) out->state = FREENECT_NUI_HAND_POINTING;
			else if (out->num_fingers == 2)
				out->state = out->finger_found[0] && out->finger_found[1] ? FREENECT_NUI_HAND_GUN : FREENECT_NUI_HAND_OPEN;
			else if (out->num_fingers == 0 && spread < 2.2f) out->state = FREENECT_NUI_HAND_CLOSED;
			else out->state = spread >= 2.5f ? FREENECT_NUI_HAND_OPEN : FREENECT_NUI_HAND_CLOSED;
		} else {
			out->state = spread >= 2.6f ? FREENECT_NUI_HAND_OPEN : FREENECT_NUI_HAND_CLOSED;
		}
		out->confidence = n >= FINGER_PIXELS ? 1.0f : (float)n / FINGER_PIXELS;
	}
}
