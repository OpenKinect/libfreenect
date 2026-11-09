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
 * Two-hand manipulation (nui/gestures.c): two hands hold an object 1.5 m
 * from the Kinect, half a metre in front of the shoulders, and turn, tip,
 * zoom and move it. The hands' paths are exact or with depth-like noise.
 * Needs no model.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gestures.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

#define DEG 0.017453293f

static void qmul(const float a[4], const float b[4], float r[4])
{
	float x = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
	float y = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
	float z = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
	float w = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
	r[0] = x; r[1] = y; r[2] = z; r[3] = w;
}

// rotate p by the unit quaternion q
static v3 qrot(const float q[4], v3 p)
{
	float pq[4] = { p.x, p.y, p.z, 0.0f }, qc[4] = { -q[0], -q[1], -q[2], q[3] }, t[4], r[4];
	qmul(q, pq, t);
	qmul(t, qc, r);
	return v3make(r[0], r[1], r[2]);
}

static void qaxis(v3 axis, float angle, float q[4])
{
	axis = fn_nui_v3norm(axis);
	q[0] = axis.x * sinf(angle / 2);
	q[1] = axis.y * sinf(angle / 2);
	q[2] = axis.z * sinf(angle / 2);
	q[3] = cosf(angle / 2);
}

static float qangle(const float q[4])
{
	float w = fabsf(q[3]);
	return 2.0f * acosf(w > 1.0f ? 1.0f : w) / DEG;
}

// the angle between two rotations, degrees
static float qdiff(const float a[4], const float b[4])
{
	float d = fabsf(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
	return 2.0f * acosf(d > 1.0f ? 1.0f : d) / DEG;
}

static float noise_mm;

static float jitter(void)
{
	return noise_mm * 0.001f * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
}

/* One frame of an object at centre c, size d, turned by q: the palms on its
 * sides along the turned x axis, the fingers along the turned y axis. */
static void frame(fn_nui_manip* m, freenect_nui_grip g, v3 c, float d, const float q[4], freenect_nui_manipulation* out, float prod[4])
{
	v3 u = qrot(q, v3make(1, 0, 0)), v = qrot(q, v3make(0, 1, 0));
	v3 L = v3sub(c, v3scale(u, d / 2)), R = v3add(c, v3scale(u, d / 2));
	L = v3add(L, v3make(jitter(), jitter(), jitter()));
	R = v3add(R, v3make(jitter(), jitter(), jitter()));
	fn_nui_manip_step(m, g, L, R, v, v, out);
	if (prod && out->active) {
		float t[4];
		float l;
		qmul(out->rotation, prod, t);
		l = sqrtf(t[0] * t[0] + t[1] * t[1] + t[2] * t[2] + t[3] * t[3]);
		prod[0] = t[0] / l;
		prod[1] = t[1] / l;
		prod[2] = t[2] / l;
		prod[3] = t[3] / l;
	}
}

int main(void)
{
	const v3 C = v3make(0.0f, 0.0f, 1.5f), shoulder = v3make(0.0f, 0.3f, 2.0f);
	const float I[4] = { 0, 0, 0, 1 };
	fn_nui_manip m;
	freenect_nui_manipulation o;
	float prod[4], q[4], scale_prod;
	int f, began_at = -1;
	char what[128];

	printf("Two-hand manipulation (nui/gestures.c)\n");

	{
		// a symmetric matrix with eigenvalues 9, 4, 1 along rotated axes
		v3 a0 = fn_nui_v3norm(v3make(1, 1, 0)), a1 = fn_nui_v3norm(v3make(-1, 1, 1)), a2 = v3cross(a0, a1), vec[3];
		double A[3][3], val[3];
		int i, j;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 3; j++) {
				float x[3][3] = { { a0.x, a0.y, a0.z }, { a1.x, a1.y, a1.z }, { a2.x, a2.y, a2.z } };
				A[i][j] = 9.0 * x[0][i] * x[0][j] + 4.0 * x[1][i] * x[1][j] + 1.0 * x[2][i] * x[2][j];
			}
		fn_nui_eig3(A, val, vec);
		CHECK(fabs(val[0] - 9) < 1e-5 && fabs(val[1] - 4) < 1e-5 && fabs(val[2] - 1) < 1e-5 && fabsf(v3dot(vec[0], a0)) > 0.9999f &&
		      fabsf(v3dot(vec[2], a2)) > 0.9999f, "eigen decomposition: values 9, 4, 1 and their axes");
	}
	{
		freenect_nui_rotation r;
		float want[4];
		v3 y;
		fn_nui_rotation_from_axes(v3make(1, 0, 0), v3make(0, 0, 1), &r);
		qaxis(v3make(0, 0, 1), -90 * DEG, want);
		CHECK(qdiff(r.quaternion, want) < 0.1f && fabsf(r.matrix[0][1] + 1.0f) < 1e-6f,
		      "rotation from axes: Y along x is -90 degrees about z, X = -y");
		y = qrot(r.quaternion, v3make(0, 1, 0));
		CHECK(fabsf(y.x - 1.0f) < 1e-5f, "  its quaternion turns the camera's y onto Y");
	}

	{
		v3 L = v3make(-0.2f, 0.0f, 1.5f), R = v3make(0.2f, 0.0f, 1.5f), cam = v3make(0, 0, -1), side = v3make(1, 0, 0);
		CHECK(fn_nui_grip_of(FREENECT_NUI_GRIP_ANY, 1, 1, FREENECT_NUI_HAND_CLOSED, FREENECT_NUI_HAND_CLOSED, L, R, cam, cam, shoulder) ==
		          FREENECT_NUI_GRIP_CLOSED, "two fists in front: closed grip");
		CHECK(fn_nui_grip_of(FREENECT_NUI_GRIP_ANY, 1, 1, FREENECT_NUI_HAND_OPEN, FREENECT_NUI_HAND_OPEN, L, R, side, v3scale(side, -1), shoulder) ==
		          FREENECT_NUI_GRIP_HOLD, "two open hands, palms facing each other: holding");
		CHECK(fn_nui_grip_of(FREENECT_NUI_GRIP_ANY, 1, 1, FREENECT_NUI_HAND_OPEN, FREENECT_NUI_HAND_OPEN, L, R, cam, cam, shoulder) ==
		          FREENECT_NUI_GRIP_NONE, "open hands facing the Kinect: no grip");
		CHECK(fn_nui_grip_of(FREENECT_NUI_GRIP_ANY, 1, 1, FREENECT_NUI_HAND_CLOSED, FREENECT_NUI_HAND_CLOSED, v3make(-0.3f, -0.3f, 1.95f),
		                     v3make(0.3f, -0.3f, 1.95f), cam, cam, shoulder) == FREENECT_NUI_GRIP_NONE, "fists down at the sides: no grip");
		CHECK(fn_nui_grip_of(FREENECT_NUI_GRIP_CLOSED, 1, 1, FREENECT_NUI_HAND_OPEN, FREENECT_NUI_HAND_OPEN, L, R, side, side, shoulder) ==
		          FREENECT_NUI_GRIP_NONE, "holding while only fists are allowed: no grip");
		CHECK(fn_nui_grip_of(FREENECT_NUI_GRIP_ANY, 1, 0, FREENECT_NUI_HAND_CLOSED, FREENECT_NUI_HAND_CLOSED, L, R, cam, cam, shoulder) ==
		          FREENECT_NUI_GRIP_NONE, "one hand not seen: no grip");
	}

	// begin
	fn_nui_manip_reset(&m);
	for (f = 0; f < 5; f++) {
		frame(&m, FREENECT_NUI_GRIP_HOLD, C, 0.4f, I, &o, NULL);
		if (o.began)
			began_at = f;
	}
	CHECK(began_at == 2 && o.active && o.grip == FREENECT_NUI_GRIP_HOLD && fabsf(o.distance - 0.4f) < 1e-4f,
	      "begins on the third frame of a grip, 40 cm apart");

	// zoom: apart from 40 to 60 cm over a second
	scale_prod = 1.0f;
	for (f = 1; f <= 40; f++) {
		frame(&m, FREENECT_NUI_GRIP_HOLD, C, 0.4f + 0.2f * (f < 30 ? f / 30.0f : 1.0f), I, &o, NULL);
		scale_prod *= o.scale;
	}
	snprintf(what, sizeof(what), "zoom: hands 40 -> 60 cm apart, total scale %.3f", o.total_scale);
	CHECK(fabsf(o.total_scale - 1.5f) < 0.01f && fabsf(scale_prod - o.total_scale) < 1e-3f, what);
	CHECK(qangle(o.total_rotation) < 0.1f && fabsf(o.total_translation.x) < 1e-3f, "  no turn, no movement");

	// turn about the vertical axis by 40 degrees (the right hand comes forward)
	fn_nui_manip_reset(&m);
	memcpy(prod, I, sizeof(prod));
	for (f = 0; f < 60; f++) {
		qaxis(v3make(0, 1, 0), 40 * DEG * (f < 45 ? (f > 3 ? (f - 3) / 42.0f : 0.0f) : 1.0f), q);
		frame(&m, FREENECT_NUI_GRIP_CLOSED, C, 0.4f, q, &o, prod);
	}
	snprintf(what, sizeof(what), "turn 40 degrees about the vertical: %.1f degrees", qangle(o.total_rotation));
	CHECK(qdiff(o.total_rotation, q) < 1.0f, what);
	CHECK(qdiff(prod, o.total_rotation) < 0.1f, "  the frame deltas add up to the total");

	// tip forward by 30 degrees about the line between the hands
	fn_nui_manip_reset(&m);
	memcpy(prod, I, sizeof(prod));
	for (f = 0; f < 60; f++) {
		qaxis(v3make(1, 0, 0), -30 * DEG * (f < 45 ? (f > 3 ? (f - 3) / 42.0f : 0.0f) : 1.0f), q);
		frame(&m, FREENECT_NUI_GRIP_HOLD, C, 0.4f, q, &o, prod);
	}
	snprintf(what, sizeof(what), "tip 30 degrees about the hands' line: %.1f degrees", qangle(o.total_rotation));
	CHECK(qdiff(o.total_rotation, q) < 1.0f && qdiff(prod, o.total_rotation) < 0.1f, what);

	// a turn about the viewing axis (a steering wheel) combined with a tip
	fn_nui_manip_reset(&m);
	memcpy(prod, I, sizeof(prod));
	for (f = 0; f < 70; f++) {
		float a[4], b[4], t = f < 50 ? (f > 3 ? (f - 3) / 46.0f : 0.0f) : 1.0f;
		qaxis(v3make(0, 0, 1), 60 * DEG * t, a);
		qaxis(v3make(1, 0, 0), 20 * DEG * t, b);
		qmul(a, b, q);
		frame(&m, FREENECT_NUI_GRIP_HOLD, C, 0.4f, q, &o, prod);
	}
	snprintf(what, sizeof(what), "steer 60 degrees and tip 20: off by %.2f degrees (deltas %.2f)", qdiff(o.total_rotation, q), qdiff(prod, o.total_rotation));
	CHECK(qdiff(o.total_rotation, q) < 1.0f && qdiff(prod, o.total_rotation) < 0.1f, what);

	// slow: 0.2 degrees a frame, under the reporting step
	fn_nui_manip_reset(&m);
	memcpy(prod, I, sizeof(prod));
	for (f = 0; f < 110; f++) {
		qaxis(v3make(0, 1, 0), 0.2f * DEG * (f > 3 ? (f < 104 ? f - 3 : 100) : 0), q);
		frame(&m, FREENECT_NUI_GRIP_HOLD, C, 0.4f, q, &o, prod);
	}
	snprintf(what, sizeof(what), "a slow 20 degree turn is not lost: %.1f degrees reported", qangle(prod));
	CHECK(fabsf(qangle(prod) - 20.0f) < 1.0f, what);

	// move
	fn_nui_manip_reset(&m);
	for (f = 0; f < 50; f++)
		frame(&m, FREENECT_NUI_GRIP_CLOSED, v3add(C, v3make(0.2f * (f < 35 ? f / 35.0f : 1.0f), 0.0f, 0.0f)), 0.4f, I, &o, NULL);
	snprintf(what, sizeof(what), "move 20 cm to the right (from the grip): %.3f m", o.total_translation.x);
	CHECK(fabsf(o.total_translation.x - 0.2f * (1.0f - 2.0f / 35.0f)) < 0.005f && o.total_scale > 0.99f && o.total_scale < 1.01f, what);

	// still, with 3 mm of noise on each palm
	srand(1);
	noise_mm = 3.0f;
	fn_nui_manip_reset(&m);
	memcpy(prod, I, sizeof(prod));
	{
		float worst = 0.0f, worst_s = 0.0f;
		for (f = 0; f < 150; f++) {
			frame(&m, FREENECT_NUI_GRIP_HOLD, C, 0.4f, I, &o, prod);
			if (o.active && qangle(o.total_rotation) > worst)
				worst = qangle(o.total_rotation);
			if (o.active && fabsf(o.total_scale - 1.0f) > worst_s)
				worst_s = fabsf(o.total_scale - 1.0f);
		}
		snprintf(what, sizeof(what), "held still with 3 mm noise: at most %.1f degrees, %.1f %% scale", worst, 100 * worst_s);
		CHECK(worst < 2.0f && worst_s < 0.02f, what);
	}
	noise_mm = 0.0f;

	// a grip that flickers off for 3 frames holds; 4 frames end it
	fn_nui_manip_reset(&m);
	for (f = 0; f < 5; f++)
		frame(&m, FREENECT_NUI_GRIP_HOLD, C, 0.4f, I, &o, NULL);
	for (f = 0; f < 3; f++)
		frame(&m, FREENECT_NUI_GRIP_NONE, C, 0.6f, I, &o, NULL);
	CHECK(o.active && !o.ended && fabsf(o.distance - 0.4f) < 1e-3f, "3 frames without the grip: still held, nothing moves");
	frame(&m, FREENECT_NUI_GRIP_HOLD, C, 0.4f, I, &o, NULL);
	for (f = 0; f < 4; f++)
		frame(&m, FREENECT_NUI_GRIP_NONE, C, 0.4f, I, &o, NULL);
	CHECK(!o.active && o.ended, "4 frames without: ended");
	frame(&m, FREENECT_NUI_GRIP_NONE, C, 0.4f, I, &o, NULL);
	CHECK(!o.active && !o.ended && o.scale == 1.0f && o.rotation[3] == 1.0f, "  then nothing held, no change");

	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
