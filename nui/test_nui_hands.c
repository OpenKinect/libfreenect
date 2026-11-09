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
 * Hands and fingers (nui/hands.c) on rendered depth: an open hand, a fist
 * and a pointing hand on a forearm, facing the Kinect, in front of a wall,
 * at 1.0 m (fingers resolved) and 2.5 m (only open/closed). Needs no model.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "hands.h"
#include "gestures.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

#define F640 571.26f

typedef struct {
	v3 a, b;
	float r;
} capsule;

static int ncaps;
static capsule caps[16];

static void cap(v3 a, v3 b, float r)
{
	caps[ncaps].a = a;
	caps[ncaps].b = b;
	caps[ncaps].r = r;
	ncaps++;
}

enum { OPEN, FIST, POINT, GUN, STICK };

static v3 wrist_at; // where make_hand put the wrist

/*
 * A right hand at (x0, y0, z0), palm towards the Kinect: the body's midline
 * is at -x (the user's right hand is at +x in skeleton space), so the thumb
 * points to -x. Fingers up, forearm down; except the gun, which points
 * forward along the forearm (to +x) with the thumb up.
 */
static void make_hand(int kind, float x0, float y0, float z0)
{
	ncaps = 0;
	wrist_at = v3make(x0, y0 - 0.08f, z0 + 0.01f);
	if (kind == GUN) {
		wrist_at = v3make(x0 - 0.08f, y0, z0 + 0.01f);
		cap(wrist_at, v3make(x0 - 0.40f, y0 - 0.02f, z0 + 0.03f), 0.03f);                  // forearm, level
		cap(v3make(x0 - 0.005f, y0, z0), v3make(x0 + 0.005f, y0, z0), 0.045f);             // fist
		cap(v3make(x0 + 0.035f, y0 + 0.01f, z0), v3make(x0 + 0.125f, y0 + 0.012f, z0), 0.009f); // index, forward
		cap(v3make(x0 + 0.0f, y0 + 0.035f, z0), v3make(x0 + 0.008f, y0 + 0.095f, z0), 0.0105f);  // thumb, up
		return;
	}
	cap(wrist_at, v3make(x0, y0 - 0.40f, z0 + 0.03f), 0.03f); // forearm
	if (kind == OPEN) {
		static const float bx[4] = { -0.027f, -0.009f, 0.009f, 0.027f }, tx[4] = { -0.055f, -0.019f, 0.019f, 0.055f };
		static const float len[4] = { 0.065f, 0.08f, 0.085f, 0.075f };
		int i;
		for (i = 0; i < 4; i++) // the palm: flat, 2.2 cm thick, as wide as the four fingers
			cap(v3make(x0 + bx[i], y0 - 0.06f, z0), v3make(x0 + bx[i], y0 + 0.025f, z0), 0.011f);
		for (i = 0; i < 4; i++) // index (nearest the thumb) .. little
			cap(v3make(x0 + bx[i], y0 + 0.03f, z0), v3make(x0 + tx[i], y0 + 0.03f + len[i], z0), 0.0085f);
		cap(v3make(x0 - 0.035f, y0 - 0.02f, z0), v3make(x0 - 0.09f, y0 + 0.025f, z0), 0.01f); // thumb
	} else {
		cap(v3make(x0, y0 - 0.005f, z0), v3make(x0, y0 + 0.005f, z0), 0.045f); // fist
		if (kind == POINT)
			cap(v3make(x0 + 0.01f, y0 + 0.03f, z0), v3make(x0 + 0.012f, y0 + 0.125f, z0), 0.009f);
		if (kind == STICK) // a stick held across the fist, 60 cm long, 3 cm thick
			cap(v3make(x0 - 0.30f, y0, z0 - 0.02f), v3make(x0 + 0.30f, y0, z0 - 0.02f), 0.015f);
	}
}

static void render(uint16_t* depth)
{
	int X, Y, k;
	for (Y = 0; Y < 480; Y++)
		for (X = 0; X < 640; X++) {
			float xm = (float)(639 - X), t = 3.0f, s;
			v3 d = v3make((xm - 319.5f) / F640, -((float)Y - 239.5f) / F640, 1.0f);
			for (k = 0; k < ncaps; k++) {
				v3 ab = v3sub(caps[k].b, caps[k].a);
				float ab2 = v3dot(ab, ab), tc = 0.5f * (caps[k].a.z + caps[k].b.z);
				for (s = tc - 0.12f; s < tc + 0.12f && s < t; s += 0.001f) {
					v3 p = v3scale(d, s), q;
					float h = ab2 > 0 ? v3dot(v3sub(p, caps[k].a), ab) / ab2 : 0.0f;
					if (h < 0) h = 0;
					if (h > 1) h = 1;
					q = v3sub(p, v3add(caps[k].a, v3scale(ab, h)));
					if (v3dot(q, q) <= caps[k].r * caps[k].r) {
						t = s;
						break;
					}
				}
			}
			depth[Y * 640 + X] = (uint16_t)(t * 1000.0f + 0.5f);
		}
}

static void analyze(int kind, float z, freenect_nui_hand* h)
{
	static uint16_t depth[640 * 480];
	make_hand(kind, 0.15f, 0.1f, z);
	render(depth);
	// the shoulder centre: 30 cm towards the body's midline, 30 cm up
	fn_nui_hand_analyze(depth, v3make(0.15f, 0.1f, z), wrist_at, v3make(-0.15f, 0.4f, z), h);
}

// turn p about the hand's centre c: first about the vertical (yaw, the palm
// turning towards the body's midline for > 0), then about x (pitch, the
// fingers coming towards the Kinect for > 0)
static v3 turn(v3 p, v3 c, float yaw, float pitch)
{
	float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
	v3 d = v3sub(p, c), e, f;
	e = v3make(d.x * cy + d.z * sy, d.y, -d.x * sy + d.z * cy);
	f = v3make(e.x, e.y * cp + e.z * sp, -e.y * sp + e.z * cp);
	return v3add(c, f);
}

static void analyze_turned(int kind, float z, float yaw_deg, float pitch_deg, freenect_nui_hand* h, v3* want_dir, v3* want_normal)
{
	static uint16_t depth[640 * 480];
	float yaw = yaw_deg * 0.017453293f, pitch = pitch_deg * 0.017453293f;
	v3 c = v3make(0.15f, 0.1f, z), o = v3make(0, 0, 0);
	int k;
	make_hand(kind, 0.15f, 0.1f, z);
	for (k = 0; k < ncaps; k++) {
		caps[k].a = turn(caps[k].a, c, yaw, pitch);
		caps[k].b = turn(caps[k].b, c, yaw, pitch);
	}
	wrist_at = turn(wrist_at, c, yaw, pitch);
	*want_dir = turn(v3make(0, 1, 0), o, yaw, pitch);
	*want_normal = turn(v3make(0, 0, -1), o, yaw, pitch);
	render(depth);
	fn_nui_hand_analyze(depth, c, wrist_at, v3make(-0.15f, 0.4f, z), h);
}

static float angle_to(freenect_nui_point a, v3 b, int unsigned_)
{
	float c = a.x * b.x + a.y * b.y + a.z * b.z;
	if (unsigned_)
		c = fabsf(c);
	return acosf(c > 1.0f ? 1.0f : c < -1.0f ? -1.0f : c) * 57.29578f;
}

static void orientation_checks(void)
{
	static const struct {
		int kind;
		float yaw, pitch;
		float max_dir, max_normal;
		const char* what;
	} cases[] = {
		{ OPEN, 0, 0, 10, 10, "open hand facing the Kinect" },
		{ OPEN, -35, 0, 12, 12, "palm turned 35 degrees away from the body" },
		{ OPEN, 45, 0, 12, 12, "palm turned 45 degrees towards the midline" },
		{ OPEN, 0, 35, 12, 12, "fingers tipped 35 degrees towards the Kinect" },
		{ OPEN, 0, -35, 12, 12, "fingers tipped 35 degrees away" },
		{ OPEN, 30, 25, 15, 15, "turned 30 and tipped 25 degrees" },
		{ OPEN, 80, 0, 15, 25, "edge-on (palm towards the midline, 80 degrees)" },
		{ FIST, 0, 0, 20, 45, "fist: along the forearm" },
	};
	unsigned i;
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		freenect_nui_hand h;
		v3 wd, wn, y;
		float ed, en, eq;
		char what[160];
		analyze_turned(cases[i].kind, 1.0f, cases[i].yaw, cases[i].pitch, &h, &wd, &wn);
		ed = angle_to(h.direction, wd, 0);
		// the sign of the normal is a guess when the hand is edge-on
		en = angle_to(h.normal, wn, cases[i].yaw >= 60);
		y = v3make(h.orientation.matrix[1][0], h.orientation.matrix[1][1], h.orientation.matrix[1][2]);
		eq = angle_to(h.direction, y, 0);
		printf("    (%s: direction (%.2f %.2f %.2f) off %.1f, normal (%.2f %.2f %.2f) off %.1f degrees)\n", cases[i].what,
		       h.direction.x, h.direction.y, h.direction.z, ed, h.normal.x, h.normal.y, h.normal.z, en);
		snprintf(what, sizeof(what), "%s: direction within %.0f, normal within %.0f degrees", cases[i].what, cases[i].max_dir,
		         cases[i].max_normal);
		CHECK(ed <= cases[i].max_dir && (cases[i].kind == FIST || en <= cases[i].max_normal) && eq < 0.5f, what);
	}
}

static const char* name(int s)
{
	return s == 1 ? "open" : s == 2 ? "closed" : s == 3 ? "pointing" : s == 4 ? "gun" : "unknown";
}

static void show(const freenect_nui_hand* h)
{
	static const char* fn[5] = { "thumb", "index", "middle", "ring", "little" };
	int f;
	for (f = 0; f < 5; f++)
		if (h->finger_found[f])
			printf("      %-6s (%.3f %.3f %.3f)\n", fn[f], h->fingertips[f].x, h->fingertips[f].y, h->fingertips[f].z);
}

/* A right fist at 1 m with the index finger pointing at the Kinect (and the
 * thumb up for a gun). */
static void towards_kinect_checks(void)
{
	static uint16_t depth[640 * 480];
	const float x0 = 0.15f, y0 = 0.1f, z0 = 1.0f;
	int gun;
	for (gun = 0; gun < 2; gun++) {
		freenect_nui_hand h;
		char what[128];
		ncaps = 0;
		wrist_at = v3make(x0, y0 - 0.06f, z0 + 0.05f);
		cap(wrist_at, v3make(x0, y0 - 0.25f, z0 + 0.32f), 0.03f);                                   // forearm, back and down
		cap(v3make(x0, y0 - 0.005f, z0), v3make(x0, y0 + 0.005f, z0), 0.045f);                      // fist
		cap(v3make(x0 + 0.01f, y0 + 0.02f, z0 - 0.035f), v3make(x0 + 0.01f, y0 + 0.02f, z0 - 0.11f), 0.009f); // index, at the Kinect
		if (gun)
			cap(v3make(x0 - 0.02f, y0 + 0.035f, z0 - 0.01f), v3make(x0 - 0.025f, y0 + 0.095f, z0 - 0.01f), 0.0105f); // thumb, up
		render(depth);
		fn_nui_hand_analyze(depth, v3make(x0, y0, z0), wrist_at, v3make(-0.15f, 0.4f, z0), &h);
		printf("    (%s at the Kinect: %d fingers, %s, towards %d, index (%.3f %.3f %.3f))\n", gun ? "gun" : "pointing", h.num_fingers,
		       name(h.state), h.finger_towards_kinect, h.fingertips[1].x, h.fingertips[1].y, h.fingertips[1].z);
		snprintf(what, sizeof(what), "%s at the Kinect: %s, the index tip within 2 cm", gun ? "a gun" : "pointing",
		         gun ? "gun" : "pointing");
		CHECK(h.finger_towards_kinect && h.finger_found[1] && h.state == (gun ? FREENECT_NUI_HAND_GUN : FREENECT_NUI_HAND_POINTING) &&
		      fabsf(h.fingertips[1].z - (z0 - 0.119f)) < 0.02f && fabsf(h.fingertips[1].x - (x0 + 0.01f)) < 0.02f, what);
	}
	{
		freenect_nui_hand h;
		v3 wd, wn;
		analyze_turned(OPEN, 1.0f, 0, 35, &h, &wd, &wn);
		CHECK(!h.finger_towards_kinect, "an open hand tipped towards the Kinect is not a finger at it");
		analyze(FIST, 1.0f, &h);
		CHECK(!h.finger_towards_kinect && h.state == FREENECT_NUI_HAND_CLOSED, "a fist is not a finger at the Kinect");
	}
}

/* Both hands together at 1 m in front of the body: two fists side by side,
 * forearms back and down; optionally both index fingers together pointing
 * at the Kinect (a two-hand gun), or up. */
enum { CLASP_FISTS, CLASP_AT_KINECT, CLASP_APART_40 };

static void make_clasp(int kind, v3* hl, v3* hr, v3* wl, v3* wr)
{
	const float x0 = 0.0f, y0 = 0.1f, z0 = 1.0f, half = kind == CLASP_APART_40 ? 0.2f : 0.045f;
	ncaps = 0;
	*hl = v3make(x0 - half, y0, z0);
	*hr = v3make(x0 + half, y0, z0);
	*wl = v3make(x0 - half - 0.01f, y0 - 0.06f, z0 + 0.05f);
	*wr = v3make(x0 + half + 0.01f, y0 - 0.06f, z0 + 0.05f);
	cap(*wl, v3make(x0 - half - 0.1f, y0 - 0.25f, z0 + 0.32f), 0.03f);
	cap(*wr, v3make(x0 + half + 0.1f, y0 - 0.25f, z0 + 0.32f), 0.03f);
	cap(v3make(hl->x, y0 - 0.005f, z0), v3make(hl->x, y0 + 0.005f, z0), 0.045f);
	cap(v3make(hr->x, y0 - 0.005f, z0), v3make(hr->x, y0 + 0.005f, z0), 0.045f);
	if (kind == CLASP_AT_KINECT)
		cap(v3make(x0, y0 + 0.02f, z0 - 0.04f), v3make(x0, y0 + 0.02f, z0 - 0.12f), 0.013f);
}

/* The triangle: both hands up in front at 1 m, palms to the Kinect, the
 * other fingers curled, thumbs level pointing towards each other, index
 * fingers up and tilted 25 degrees inwards. right: 1 for the right hand. */
static void triangle_hand(int right, v3* hand, v3* wrist)
{
	float x0 = right ? 0.13f : -0.13f, y0 = 0.1f, z0 = 1.0f, in = right ? -1.0f : 1.0f, s = sinf(0.436f), c = cosf(0.436f);
	int i;
	static const float bx[4] = { -0.027f, -0.009f, 0.009f, 0.027f };
	*hand = v3make(x0, y0, z0);
	*wrist = v3make(x0, y0 - 0.08f, z0 + 0.01f);
	cap(*wrist, v3make(x0, y0 - 0.40f, z0 + 0.03f), 0.03f);
	for (i = 0; i < 4; i++)
		cap(v3make(x0 + bx[i], y0 - 0.06f, z0), v3make(x0 + bx[i], y0 + 0.025f, z0), 0.011f);
	cap(v3make(x0 - 0.02f, y0 + 0.035f, z0 - 0.005f), v3make(x0 + 0.02f, y0 + 0.035f, z0 - 0.005f), 0.013f); // curled fingers
	cap(v3make(x0 + in * 0.027f, y0 + 0.03f, z0), v3make(x0 + in * (0.027f + s * 0.085f), y0 + 0.03f + c * 0.085f, z0), 0.0085f); // index
	cap(v3make(x0 + in * 0.035f, y0 - 0.02f, z0), v3make(x0 + in * 0.095f, y0 - 0.015f, z0), 0.01f); // thumb, level, inwards
}

static void triangle_checks(void)
{
	static uint16_t depth[640 * 480];
	freenect_nui_hand h[2];
	v3 hand[2], wrist[2], L, R, tl, tr;
	int k;
	ncaps = 0;
	triangle_hand(0, &hand[0], &wrist[0]);
	triangle_hand(1, &hand[1], &wrist[1]);
	render(depth);
	for (k = 0; k < 2; k++) {
		fn_nui_hand_analyze(depth, hand[k], wrist[k], v3make(0.0f, 0.4f, 1.0f), &h[k]);
		printf("    (triangle, %s hand: %s, thumb %d (%.3f %.3f), index %d (%.3f %.3f), palm normal (%.2f %.2f %.2f))\n",
		       k ? "right" : "left", name(h[k].state), h[k].finger_found[0], h[k].fingertips[0].x, h[k].fingertips[0].y,
		       h[k].finger_found[1], h[k].fingertips[1].x, h[k].fingertips[1].y, h[k].normal.x, h[k].normal.y, h[k].normal.z);
	}
	CHECK(h[0].state == FREENECT_NUI_HAND_GUN && h[1].state == FREENECT_NUI_HAND_GUN && h[0].finger_found[0] && h[1].finger_found[0] &&
	      h[0].fingertips[0].x > hand[0].x + 0.05f && h[1].fingertips[0].x < hand[1].x - 0.05f,
	      "triangle: both hands thumb and index, thumbs pointing inwards");
	L = v3make(h[0].palm.x, h[0].palm.y, h[0].palm.z);
	R = v3make(h[1].palm.x, h[1].palm.y, h[1].palm.z);
	tl = v3make(h[0].fingertips[0].x, h[0].fingertips[0].y, h[0].fingertips[0].z);
	tr = v3make(h[1].fingertips[0].x, h[1].fingertips[0].y, h[1].fingertips[0].z);
	CHECK(fn_nui_grip_of(FREENECT_NUI_GRIP_ANY, 1, 1, h[0].state, h[1].state, L, R, v3make(h[0].normal.x, h[0].normal.y, h[0].normal.z),
	                     v3make(h[1].normal.x, h[1].normal.y, h[1].normal.z), tl, tr, v3make(0.0f, 0.4f, 1.4f)) == FREENECT_NUI_GRIP_TRIANGLE,
	      "  read as the triangle grip");
}

static void clasp_checks(void)
{
	static uint16_t depth[640 * 480];
	freenect_nui_clasp c;
	v3 hl, hr, wl, wr;

	make_clasp(CLASP_FISTS, &hl, &hr, &wl, &wr);
	render(depth);
	fn_nui_clasp_analyze(depth, hl, hr, wl, wr, v3make(0.0f, 0.4f, 1.3f), &c);
	printf("    (clasped fists: clasped %d, %s)\n", c.clasped, name(c.state));
	CHECK(c.clasped && c.state == FREENECT_NUI_HAND_CLOSED, "hands clasped together, nothing out: clasped, closed");

	make_clasp(CLASP_AT_KINECT, &hl, &hr, &wl, &wr);
	render(depth);
	fn_nui_clasp_analyze(depth, hl, hr, wl, wr, v3make(0.0f, 0.4f, 1.3f), &c);
	printf("    (two-hand gun: clasped %d, %s, towards %d, tip (%.3f %.3f %.3f), direction (%.2f %.2f %.2f))\n", c.clasped,
	       name(c.state), c.towards_kinect, c.tip.x, c.tip.y, c.tip.z, c.direction.x, c.direction.y, c.direction.z);
	CHECK(c.clasped && c.state == FREENECT_NUI_HAND_POINTING && c.towards_kinect && fabsf(c.tip.z - 0.867f) < 0.02f &&
	      c.direction.z < -0.7f, "two-hand gun at the Kinect: pointing, the tip and direction");

	make_clasp(CLASP_APART_40, &hl, &hr, &wl, &wr);
	render(depth);
	fn_nui_clasp_analyze(depth, hl, hr, wl, wr, v3make(0.0f, 0.4f, 1.3f), &c);
	CHECK(!c.clasped, "hands 40 cm apart: not clasped");
}

int main(void)
{
	freenect_nui_hand h;
	printf("Hands and fingers (nui/hands.c)\n");

	analyze(OPEN, 1.0f, &h);
	printf("    (open, 1.0 m: %d pixels, palm r %.1f cm, extent %.1f cm, %d fingers, %s)\n", h.pixels, 100 * h.palm_radius,
	       100 * h.extent, h.num_fingers, name(h.state));
	show(&h);
	CHECK(h.fingers_valid && h.num_fingers == 5 && h.state == FREENECT_NUI_HAND_OPEN, "open hand at 1 m: 5 fingers, open");
	CHECK(h.finger_found[0] && h.fingertips[0].x < 0.09f && h.fingertips[0].y < 0.15f, "  the thumb is the thumb (out to the side, low)");
	CHECK(h.finger_found[1] && h.finger_found[2] && h.finger_found[3] && h.finger_found[4] &&
	      h.fingertips[1].x < h.fingertips[2].x && h.fingertips[2].x < h.fingertips[3].x && h.fingertips[3].x < h.fingertips[4].x,
	      "  index, middle, ring, little in order from the thumb");
	CHECK(fabsf(h.fingertips[2].x - 0.131f) < 0.02f && fabsf(h.fingertips[2].y - 0.215f) < 0.025f, "  the middle fingertip within 2 cm of the truth");
	CHECK(fabsf(h.palm.x - 0.15f) < 0.03f && fabsf(h.palm.y - 0.09f) < 0.04f, "  palm centre within 3-4 cm");

	analyze(FIST, 1.0f, &h);
	printf("    (fist, 1.0 m: %d pixels, palm r %.1f cm, extent %.1f cm, %d fingers, %s)\n", h.pixels, 100 * h.palm_radius,
	       100 * h.extent, h.num_fingers, name(h.state));
	CHECK(h.fingers_valid && h.num_fingers == 0 && h.state == FREENECT_NUI_HAND_CLOSED, "fist at 1 m: no finger, closed");

	analyze(POINT, 1.0f, &h);
	printf("    (pointing, 1.0 m: %d pixels, %d fingers, %s)\n", h.pixels, h.num_fingers, name(h.state));
	show(&h);
	CHECK(h.fingers_valid && h.num_fingers == 1 && h.finger_found[1] && h.state == FREENECT_NUI_HAND_POINTING,
	      "pointing at 1 m: the index finger, pointing");
	CHECK(h.finger_found[1] && h.fingertips[1].y > 0.19f && h.fingertips[1].y < 0.24f, "  at the true tip (y 0.225 m)");

	analyze(GUN, 1.0f, &h);
	printf("    (gun, 1.0 m: %d pixels, %d fingers, %s)\n", h.pixels, h.num_fingers, name(h.state));
	show(&h);
	CHECK(h.fingers_valid && h.num_fingers == 2 && h.finger_found[0] && h.finger_found[1] && h.state == FREENECT_NUI_HAND_GUN,
	      "gun shape at 1 m: thumb and index, gun");
	CHECK(h.finger_found[0] && h.finger_found[1] && h.fingertips[0].y > 0.17f && h.fingertips[1].x > 0.25f,
	      "  thumb up, index forward");

	analyze(STICK, 1.0f, &h);
	printf("    (stick, 1.0 m: %d pixels, %d fingers, holding %d, %s)\n", h.pixels, h.num_fingers, h.holding, name(h.state));
	CHECK(h.holding && h.num_fingers == 0 && h.state == FREENECT_NUI_HAND_CLOSED, "holding a stick at 1 m: holding, closed, no fingers");
	analyze(STICK, 2.5f, &h);
	printf("    (stick, 2.5 m: %d pixels, holding %d, %s)\n", h.pixels, h.holding, name(h.state));
	CHECK(h.holding && h.state == FREENECT_NUI_HAND_CLOSED, "holding a stick at 2.5 m: holding, closed");
	analyze(OPEN, 1.0f, &h);
	CHECK(!h.holding, "an open hand holds nothing");

	orientation_checks();
	towards_kinect_checks();
	clasp_checks();
	triangle_checks();

	analyze(OPEN, 2.5f, &h);
	printf("    (open, 2.5 m: %d pixels, extent %.1f cm, palm r %.1f cm, fingers valid %d, %s)\n", h.pixels,
	       100 * h.extent, 100 * h.palm_radius, h.fingers_valid, name(h.state));
	CHECK(!h.fingers_valid && h.num_fingers == 0 && h.state == FREENECT_NUI_HAND_OPEN, "open hand at 2.5 m: no fingers reported, open");
	analyze(FIST, 2.5f, &h);
	printf("    (fist, 2.5 m: %d pixels, extent %.1f cm, palm r %.1f cm, %s)\n", h.pixels, 100 * h.extent, 100 * h.palm_radius, name(h.state));
	CHECK(!h.fingers_valid && h.state == FREENECT_NUI_HAND_CLOSED, "fist at 2.5 m: closed");

	{
		static uint16_t empty[640 * 480];
		memset(empty, 0, sizeof(empty));
		fn_nui_hand_analyze(empty, v3make(0, 0, 1.0f), v3make(0, -0.08f, 1.0f), v3make(-0.3f, 0.3f, 1.0f), &h);
		CHECK(h.state == FREENECT_NUI_HAND_UNKNOWN, "no depth: unknown");
	}
	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
