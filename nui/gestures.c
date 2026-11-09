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
 * Two-hand manipulation of a virtual object (turn, zoom, move), and the
 * small geometry it and the hand orientation need.
 *
 * The object's frame is built from the hands: u along the line from the left
 * to the right palm, v the hands' common direction (wrist -> fingers) made
 * perpendicular to u, w = u x v. Turning the object about a vertical or a
 * viewing axis moves the palms (u); tipping it forward or back about the line
 * between the hands turns the hands themselves (v). The rotation between two
 * such frames is the object's rotation; the palms' distance its scale.
 */

#include <math.h>
#include <string.h>
#include "gestures.h"

#define BEGIN_FRAMES 3
#define END_FRAMES 4
#define IN_FRONT 0.15f        // m in front of the shoulder centre
#define MIN_APART 0.08f
#define MAX_APART 1.2f
#define FACING 0.57f          // |cos| of the palm normal to the hand line (55 degrees)
#define SMOOTH 0.5f           // weight of the new frame
#define MIN_ANGLE 0.0087f     // rad (0.5 degrees)
#define MIN_SCALE 0.003f
#define MIN_MOVE 0.003f       // m

void fn_nui_eig3(const double A[3][3], double val[3], v3 vec[3])
{
	double a[3][3], V[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
	int i, j, k, p, q, sweep, ord[3] = { 0, 1, 2 };
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			a[i][j] = A[i][j];
	for (sweep = 0; sweep < 30; sweep++) {
		double off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2];
		if (off < 1e-24)
			break;
		for (p = 0; p < 2; p++)
			for (q = p + 1; q < 3; q++) {
				double theta, t, c, s;
				if (fabs(a[p][q]) < 1e-30)
					continue;
				theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
				t = (theta >= 0 ? 1.0 : -1.0) / (fabs(theta) + sqrt(theta * theta + 1.0));
				c = 1.0 / sqrt(t * t + 1.0);
				s = t * c;
				for (k = 0; k < 3; k++) { // A J
					double akp = a[k][p], akq = a[k][q];
					a[k][p] = c * akp - s * akq;
					a[k][q] = s * akp + c * akq;
				}
				for (k = 0; k < 3; k++) { // J^T (A J)
					double apk = a[p][k], aqk = a[q][k];
					a[p][k] = c * apk - s * aqk;
					a[q][k] = s * apk + c * aqk;
				}
				for (k = 0; k < 3; k++) {
					double vkp = V[k][p], vkq = V[k][q];
					V[k][p] = c * vkp - s * vkq;
					V[k][q] = s * vkp + c * vkq;
				}
			}
	}
	for (i = 1; i < 3; i++)
		for (j = i; j > 0 && a[ord[j - 1]][ord[j - 1]] < a[ord[j]][ord[j]]; j--) {
			int t = ord[j];
			ord[j] = ord[j - 1];
			ord[j - 1] = t;
		}
	for (i = 0; i < 3; i++) {
		val[i] = a[ord[i]][ord[i]];
		vec[i] = fn_nui_v3norm(v3make((float)V[0][ord[i]], (float)V[1][ord[i]], (float)V[2][ord[i]]));
	}
}

// quaternion of a rotation matrix R[row][col] (column vectors: p' = R p)
static void quat_of(const double R[3][3], float q[4])
{
	double tr = R[0][0] + R[1][1] + R[2][2], s, x, y, z, w, l;
	if (tr > 0.0) {
		s = sqrt(tr + 1.0) * 2.0;
		w = 0.25 * s; x = (R[2][1] - R[1][2]) / s; y = (R[0][2] - R[2][0]) / s; z = (R[1][0] - R[0][1]) / s;
	} else if (R[0][0] > R[1][1] && R[0][0] > R[2][2]) {
		s = sqrt(1.0 + R[0][0] - R[1][1] - R[2][2]) * 2.0;
		w = (R[2][1] - R[1][2]) / s; x = 0.25 * s; y = (R[0][1] + R[1][0]) / s; z = (R[0][2] + R[2][0]) / s;
	} else if (R[1][1] > R[2][2]) {
		s = sqrt(1.0 + R[1][1] - R[0][0] - R[2][2]) * 2.0;
		w = (R[0][2] - R[2][0]) / s; x = (R[0][1] + R[1][0]) / s; y = 0.25 * s; z = (R[1][2] + R[2][1]) / s;
	} else {
		s = sqrt(1.0 + R[2][2] - R[0][0] - R[1][1]) * 2.0;
		w = (R[1][0] - R[0][1]) / s; x = (R[0][2] + R[2][0]) / s; y = (R[1][2] + R[2][1]) / s; z = 0.25 * s;
	}
	if (w < 0.0) {
		x = -x; y = -y; z = -z; w = -w;
	}
	l = sqrt(x * x + y * y + z * z + w * w);
	if (l <= 0.0)
		l = 1.0;
	q[0] = (float)(x / l);
	q[1] = (float)(y / l);
	q[2] = (float)(z / l);
	q[3] = (float)(w / l);
}

void fn_nui_rotation_from_axes(v3 y, v3 z, freenect_nui_rotation* r)
{
	v3 X, Y = fn_nui_v3norm(y), Z;
	double R[3][3];
	int i;
	Z = fn_nui_v3norm(v3sub(z, v3scale(Y, v3dot(z, Y))));
	X = v3cross(Y, Z);
	r->matrix[0][0] = X.x; r->matrix[0][1] = X.y; r->matrix[0][2] = X.z;
	r->matrix[1][0] = Y.x; r->matrix[1][1] = Y.y; r->matrix[1][2] = Y.z;
	r->matrix[2][0] = Z.x; r->matrix[2][1] = Z.y; r->matrix[2][2] = Z.z;
	// the rotation taking the camera axes onto X, Y, Z: its columns are X, Y, Z
	for (i = 0; i < 3; i++) {
		R[i][0] = r->matrix[0][i];
		R[i][1] = r->matrix[1][i];
		R[i][2] = r->matrix[2][i];
	}
	quat_of(R, r->quaternion);
}

static float comp(v3 a, int i) { return i == 0 ? a.x : i == 1 ? a.y : a.z; }

float fn_nui_quat_between(const v3 b0[3], const v3 b1[3], float q[4])
{
	double R[3][3];
	int r, c, k;
	for (r = 0; r < 3; r++)
		for (c = 0; c < 3; c++) {
			double s = 0.0;
			for (k = 0; k < 3; k++)
				s += (double)comp(b1[k], r) * comp(b0[k], c);
			R[r][c] = s;
		}
	quat_of(R, q);
	return 2.0f * acosf(q[3] > 1.0f ? 1.0f : q[3]);
}

freenect_nui_grip fn_nui_grip_of(freenect_nui_grip allowed, int tracked_l, int tracked_r, freenect_nui_hand_state state_l,
                                 freenect_nui_hand_state state_r, v3 L, v3 R, v3 normal_l, v3 normal_r, v3 thumb_l, v3 thumb_r,
                                 v3 shoulder)
{
	v3 d = v3sub(R, L);
	float apart = fn_nui_v3len(d);
	v3 u;
	if (!tracked_l || !tracked_r || L.z > shoulder.z - IN_FRONT || R.z > shoulder.z - IN_FRONT || apart < MIN_APART ||
	    apart > MAX_APART)
		return FREENECT_NUI_GRIP_NONE;
	u = v3scale(d, 1.0f / apart);
	if (state_l == FREENECT_NUI_HAND_CLOSED && state_r == FREENECT_NUI_HAND_CLOSED)
		return (allowed & FREENECT_NUI_GRIP_CLOSED) ? FREENECT_NUI_GRIP_CLOSED : FREENECT_NUI_GRIP_NONE;
	// the triangle: thumb and index out on both hands, the thumbs pointing
	// towards each other
	if (state_l == FREENECT_NUI_HAND_GUN && state_r == FREENECT_NUI_HAND_GUN &&
	    v3dot(fn_nui_v3norm(v3sub(thumb_l, L)), u) > 0.3f && v3dot(fn_nui_v3norm(v3sub(thumb_r, R)), u) < -0.3f)
		return (allowed & FREENECT_NUI_GRIP_TRIANGLE) ? FREENECT_NUI_GRIP_TRIANGLE : FREENECT_NUI_GRIP_NONE;
	if (state_l != FREENECT_NUI_HAND_UNKNOWN && state_l != FREENECT_NUI_HAND_CLOSED && state_r != FREENECT_NUI_HAND_UNKNOWN &&
	    state_r != FREENECT_NUI_HAND_CLOSED && fabsf(v3dot(normal_l, u)) >= FACING && fabsf(v3dot(normal_r, u)) >= FACING)
		return (allowed & FREENECT_NUI_GRIP_HOLD) ? FREENECT_NUI_GRIP_HOLD : FREENECT_NUI_GRIP_NONE;
	return FREENECT_NUI_GRIP_NONE;
}

void fn_nui_manip_reset(fn_nui_manip* m)
{
	memset(m, 0, sizeof(*m));
}

static void identity(float q[4])
{
	q[0] = q[1] = q[2] = 0.0f;
	q[3] = 1.0f;
}

static v3 lerp(v3 a, v3 b, float t)
{
	return v3add(a, v3scale(v3sub(b, a), t));
}

// the object's frame from the smoothed hands; prev: the frame to keep the
// twist from when the hands give no direction across the line between them
static void basis(const fn_nui_manip* m, const v3* prev, v3 b[3])
{
	v3 u = fn_nui_v3norm(v3sub(m->R, m->L)), s = m->dir, v;
	v = v3sub(s, v3scale(u, v3dot(s, u)));
	if (fn_nui_v3len(v) < 0.3f) {
		v3 p = prev ? prev[1] : v3make(0.0f, 1.0f, 0.0f);
		v = v3sub(p, v3scale(u, v3dot(p, u)));
		if (fn_nui_v3len(v) < 1e-3f)
			v = v3sub(v3make(0.0f, 0.0f, -1.0f), v3scale(u, -u.z));
	}
	v = fn_nui_v3norm(v);
	b[0] = u;
	b[1] = v;
	b[2] = v3cross(u, v);
}

static freenect_nui_point P(v3 a)
{
	freenect_nui_point p;
	p.x = a.x;
	p.y = a.y;
	p.z = a.z;
	return p;
}

void fn_nui_manip_step(fn_nui_manip* m, freenect_nui_grip grip, v3 L, v3 R, v3 dir_l, v3 dir_r,
                       freenect_nui_manipulation* out)
{
	v3 b[3], c;
	float d;
	memset(out, 0, sizeof(*out));
	out->scale = out->total_scale = 1.0f;
	identity(out->rotation);
	identity(out->total_rotation);
	// one grip from start to end: another grip counts as none
	if (m->on && grip != m->grip)
		grip = FREENECT_NUI_GRIP_NONE;
	if (!m->on && grip != m->grip) {
		m->grip = grip; // a new grip starts its count
		m->hits = 0;
	}
	if (grip != FREENECT_NUI_GRIP_NONE) {
		m->hits++;
		m->misses = 0;
	} else {
		m->misses++;
		m->hits = 0;
	}
	if (!m->on) {
		if (m->hits < BEGIN_FRAMES)
			return;
		m->on = 1;
		m->grip = grip;
		m->L = L;
		m->R = R;
		m->dir = v3add(dir_l, dir_r);
		basis(m, NULL, m->b0);
		memcpy(m->b_ref, m->b0, sizeof(m->b0));
		m->d0 = m->d_ref = fn_nui_v3len(v3sub(R, L));
		m->c0 = m->c_ref = v3scale(v3add(L, R), 0.5f);
		out->began = 1;
	} else if (m->misses >= END_FRAMES) {
		m->on = 0;
		out->ended = 1;
		out->grip = m->grip;
		out->center = P(v3scale(v3add(m->L, m->R), 0.5f));
		out->distance = fn_nui_v3len(v3sub(m->R, m->L));
		return;
	} else if (grip != FREENECT_NUI_GRIP_NONE) {
		// while the grip flickers off for a frame or two, hold still
		m->L = lerp(m->L, L, SMOOTH);
		m->R = lerp(m->R, R, SMOOTH);
		m->dir = lerp(m->dir, v3add(dir_l, dir_r), SMOOTH);
	}
	basis(m, m->b_ref, b);
	d = fn_nui_v3len(v3sub(m->R, m->L));
	c = v3scale(v3add(m->L, m->R), 0.5f);
	out->active = 1;
	out->grip = m->grip;
	out->center = P(c);
	out->distance = d;
	{
		float q[4];
		if (fn_nui_quat_between(m->b_ref, b, q) >= MIN_ANGLE) {
			memcpy(out->rotation, q, sizeof(q));
			memcpy(m->b_ref, b, sizeof(b));
		}
	}
	if (m->d_ref > 0.0f && fabsf(d / m->d_ref - 1.0f) >= MIN_SCALE) {
		out->scale = d / m->d_ref;
		m->d_ref = d;
	}
	if (fn_nui_v3len(v3sub(c, m->c_ref)) >= MIN_MOVE) {
		out->translation = P(v3sub(c, m->c_ref));
		m->c_ref = c;
	}
	fn_nui_quat_between(m->b0, m->b_ref, out->total_rotation);
	out->total_scale = m->d0 > 0.0f ? m->d_ref / m->d0 : 1.0f;
	out->total_translation = P(v3sub(m->c_ref, m->c0));
}

/*
 * One hand as a pointer, as the SDK's interaction stream: the cursor maps
 * the hand's position in a box in front of that side's shoulder onto the
 * screen; a press is a push towards the Kinect relative to the reach the
 * arm rests at (which follows slow changes, so leaning in is not a press); a
 * grip is the hand closing; a swipe is a fast move along one axis.
 */
#define ZONE_W 0.60f          // m
#define ZONE_H 0.45f
#define ZONE_OUT 0.10f        // the zone's centre outside the shoulder
#define ZONE_DOWN 0.15f       // and below the shoulder centre
#define ACTIVE_REACH 0.20f    // m in front of the shoulder to become active
#define STAY_REACH 0.15f      // and to stay active
#define PRESS_DEPTH 0.12f     // m of push for press = 1
#define SWIPE_DIST 0.30f
#define SWIPE_RATIO 2.0f      // along the axis vs across it
#define SWIPE_PAUSE 15        // frames

void fn_nui_pointer_reset(fn_nui_pointer* p)
{
	memset(p, 0, sizeof(*p));
}

static float clamp01(float v)
{
	return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v;
}

void fn_nui_pointer_step(fn_nui_pointer* p, int right, int tracked, v3 hand, freenect_nui_hand_state state, v3 shoulder_side,
                         v3 shoulder_center, v3 hip_center, int quiet, freenect_nui_hand_pointer* out)
{
	float side = right ? 1.0f : -1.0f, x, y, reach, speed;
	int closed, k;
	memset(out, 0, sizeof(*out));
	if (!tracked) {
		if (p->gripped)
			out->events |= FREENECT_NUI_EVENT_GRIP_END;
		if (p->pressed)
			out->events |= FREENECT_NUI_EVENT_PRESS_END;
		if (quiet)
			out->events = 0;
		fn_nui_pointer_reset(p);
		return;
	}
	reach = shoulder_side.z - hand.z;
	x = 0.5f + (hand.x - (shoulder_side.x + side * ZONE_OUT)) / ZONE_W;
	y = 0.5f - (hand.y - (shoulder_center.y - ZONE_DOWN)) / ZONE_H;
	if (p->active)
		p->active = reach >= STAY_REACH && x > -0.4f && x < 1.4f && y > -0.4f && y < 1.4f && hand.y > hip_center.y - 0.1f;
	else
		p->active = reach >= ACTIVE_REACH && x > -0.25f && x < 1.25f && y > -0.25f && y < 1.25f && hand.y > hip_center.y;

	// cursor: smoothed, the more the slower the hand moves
	speed = p->nh ? fn_nui_v3len(v3sub(hand, p->last)) * 30.0f : 0.0f;
	if (!p->active || p->nh == 0) {
		p->cx = x;
		p->cy = y;
	} else {
		float a = 0.3f + 0.7f * clamp01(speed / 1.0f);
		p->cx += a * (x - p->cx);
		p->cy += a * (y - p->cy);
	}
	p->active_frames = p->active ? p->active_frames + 1 : 0;
	out->active = p->active;
	out->x = clamp01(p->cx);
	out->y = clamp01(p->cy);

	// press: the push beyond the resting reach
	if (!p->active) {
		if (p->pressed)
			out->events |= FREENECT_NUI_EVENT_PRESS_END;
		p->pressed = 0;
		p->rest_set = 0;
	} else {
		float press;
		if (!p->rest_set) {
			p->rest = reach;
			p->rest_set = 1;
		}
		press = (reach - p->rest) / PRESS_DEPTH;
		if (p->active_frames < 10) {
			// still settling after the hand came up: follow it, no press
			p->rest += 0.3f * (reach - p->rest);
			press = (reach - p->rest) / PRESS_DEPTH;
		} else if (!p->pressed) {
			// follow slow changes; pulling back resets quickly
			p->rest += (reach < p->rest ? 0.3f : 0.03f) * (reach - p->rest);
			if (press >= 1.0f) {
				p->pressed = 1;
				out->events |= FREENECT_NUI_EVENT_PRESS;
			}
		} else if (press < 0.5f) {
			p->pressed = 0;
			out->events |= FREENECT_NUI_EVENT_PRESS_END;
		}
		out->press = clamp01(press);
	}
	out->pressed = p->pressed;

	// grip: the hand closing while active (an unseen frame changes nothing)
	closed = state == FREENECT_NUI_HAND_CLOSED || (state == FREENECT_NUI_HAND_UNKNOWN && p->gripped);
	if (p->active && closed && !p->gripped) {
		p->gripped = 1;
		out->events |= FREENECT_NUI_EVENT_GRIP;
	} else if (p->gripped && (!closed || !p->active)) {
		p->gripped = 0;
		out->events |= FREENECT_NUI_EVENT_GRIP_END;
	}
	out->gripped = p->gripped;

	// swipe: 30 cm within the last half second, mostly along one axis,
	// starting in the zone, and neither pressing nor gripping
	if (p->nh == FN_NUI_POINTER_HISTORY) {
		memmove(p->hist, p->hist + 1, (FN_NUI_POINTER_HISTORY - 1) * sizeof(v3));
		memmove(p->hist_active, p->hist_active + 1, (FN_NUI_POINTER_HISTORY - 1) * sizeof(int));
		p->nh--;
	}
	p->hist_active[p->nh] = p->active;
	p->hist[p->nh++] = hand;
	if (p->cooldown > 0)
		p->cooldown--;
	if (p->cooldown == 0 && reach >= 0.10f && !p->pressed && !p->gripped)
		for (k = 0; k + 3 < p->nh; k++) {
			v3 d = v3sub(hand, p->hist[k]);
			float ax = fabsf(d.x), ay = fabsf(d.y);
			uint32_t ev = 0;
			if (!p->hist_active[k]) // started in the zone, not while raising the hand
				continue;
			if (ax >= SWIPE_DIST && ax >= SWIPE_RATIO * ay)
				ev = d.x > 0 ? FREENECT_NUI_EVENT_SWIPE_RIGHT : FREENECT_NUI_EVENT_SWIPE_LEFT;
			else if (ay >= SWIPE_DIST && ay >= SWIPE_RATIO * ax)
				ev = d.y > 0 ? FREENECT_NUI_EVENT_SWIPE_UP : FREENECT_NUI_EVENT_SWIPE_DOWN;
			if (ev) {
				out->events |= ev;
				p->cooldown = SWIPE_PAUSE;
				p->nh = 0;
				break;
			}
		}
	p->last = hand;
	if (quiet)
		out->events = 0;
}
