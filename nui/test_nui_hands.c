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
		cap(v3make(x0 - 0.02f, y0 - 0.01f, z0), v3make(x0 + 0.02f, y0 - 0.01f, z0), 0.035f); // palm
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
