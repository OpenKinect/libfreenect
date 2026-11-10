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
 * The seated head detector (nui/headforest.c) with Microsoft's head forest:
 * the file as the reference describes it (§30.3.3), a seated person rendered
 * in front of a chair and a wall, and the head tracks over frames. Needs
 * HEADPOSITION.MSHDF (skipped, exit 77, without it).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headforest.h"
#include "models.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

#define F640 571.26f

typedef struct {
	v3 a, b;
	float r;
} capsule;

static capsule caps[16];
static int ncaps;
static float wall_z = 3.5f;

static void cap(v3 a, v3 b, float r)
{
	caps[ncaps].a = a;
	caps[ncaps].b = b;
	caps[ncaps].r = r;
	ncaps++;
}

static float sdf(v3 p)
{
	float d = wall_z - p.z;
	int k;
	for (k = 0; k < ncaps; k++) {
		v3 ab = v3sub(caps[k].b, caps[k].a), ap = v3sub(p, caps[k].a);
		float ab2 = v3dot(ab, ab), h = ab2 > 0 ? v3dot(ap, ab) / ab2 : 0.0f, e;
		if (h < 0) h = 0;
		if (h > 1) h = 1;
		e = fn_nui_v3len(v3sub(ap, v3scale(ab, h))) - caps[k].r;
		if (e < d)
			d = e;
	}
	return d;
}

// 640x480 depth in mm, libfreenect's orientation (skeleton x is to the left in the image)
static void render(uint16_t* depth)
{
	int X, Y, i;
	for (Y = 0; Y < 480; Y++)
		for (X = 0; X < 640; X++) {
			float xm = (float)(639 - X);
			v3 d = v3make((xm - 319.5f) / F640, -((float)Y - 239.5f) / F640, 1.0f);
			float t = 0.4f;
			for (i = 0; i < 64; i++) {
				float e = sdf(v3scale(d, t));
				if (e < 0.0005f)
					break;
				t += e;
				if (t > 4.0f)
					break;
			}
			depth[Y * 640 + X] = t < 4.0f ? (uint16_t)(t * 1000.0f + 0.5f) : 0;
		}
}

static v3 head_at;

// a person seated at distance z, head centre at (hx, hy), on a chair
static void seated(float hx, float hy, float z, int with_person)
{
	ncaps = 0;
	head_at = v3make(hx, hy, z);
	// the chair: back and seat
	cap(v3make(hx - 0.22f, hy - 0.35f, z + 0.25f), v3make(hx + 0.22f, hy - 0.35f, z + 0.25f), 0.03f);
	cap(v3make(hx - 0.22f, hy - 0.35f, z + 0.25f), v3make(hx - 0.22f, hy - 0.95f, z + 0.25f), 0.025f);
	cap(v3make(hx + 0.22f, hy - 0.35f, z + 0.25f), v3make(hx + 0.22f, hy - 0.95f, z + 0.25f), 0.025f);
	cap(v3make(hx - 0.22f, hy - 0.72f, z + 0.05f), v3make(hx + 0.22f, hy - 0.72f, z + 0.05f), 0.04f);
	if (!with_person)
		return;
	cap(head_at, head_at, 0.095f);                                                                      // head
	cap(v3make(hx, hy - 0.12f, z + 0.03f), v3make(hx, hy - 0.16f, z + 0.03f), 0.05f);                // neck
	cap(v3make(hx, hy - 0.30f, z + 0.06f), v3make(hx, hy - 0.62f, z + 0.08f), 0.15f);                // torso
	cap(v3make(hx - 0.18f, hy - 0.24f, z + 0.06f), v3make(hx - 0.24f, hy - 0.52f, z - 0.05f), 0.045f); // upper arms
	cap(v3make(hx + 0.18f, hy - 0.24f, z + 0.06f), v3make(hx + 0.24f, hy - 0.52f, z - 0.05f), 0.045f);
	cap(v3make(hx - 0.24f, hy - 0.52f, z - 0.05f), v3make(hx - 0.12f, hy - 0.62f, z - 0.30f), 0.04f);  // forearms
	cap(v3make(hx + 0.24f, hy - 0.52f, z - 0.05f), v3make(hx + 0.12f, hy - 0.62f, z - 0.30f), 0.04f);
	cap(v3make(hx - 0.10f, hy - 0.72f, z + 0.02f), v3make(hx - 0.10f, hy - 0.74f, z - 0.40f), 0.08f);  // thighs
	cap(v3make(hx + 0.10f, hy - 0.72f, z + 0.02f), v3make(hx + 0.10f, hy - 0.74f, z - 0.40f), 0.08f);
}

static uint16_t depth[640 * 480], map[NW * NH];

static void frame(fn_hd* h)
{
	render(depth);
	fn_nui_make_map(depth, map);
	fn_hd_detect(h, map);
	fn_hd_track_update(h);
}

int main(void)
{
	size_t len;
	uint8_t* data = fn_nui_read_model(NULL, FN_NUI_HEAD_FOREST_FILE, &len, NULL, 0);
	static fn_hd h;
	freenect_nui_head out[FN_HD_SLOTS];
	int t, i, f, n;
	uint32_t id;
	static const int two_probe[3] = { 3972, 3826, 3859 }, thr_lo[3] = { -2391, -2335, -1691 }, thr_hi[3] = { 1670, 1574, 1720 };
	static const double leaf_mean[3] = { 22.36, 25.23, 23.04 };
	char what[160];

	printf("Seated head detector (nui/headforest.c)\n");
	if (!data) {
		printf("  HEADPOSITION.MSHDF not found: skipped\n");
		return 77;
	}
	CHECK(fn_hd_forest_load(&h.forest, data, len) == 0, "HEADPOSITION.MSHDF loads (version 2, format id)");
	if (!h.forest.nodes)
		return 1;
	CHECK(h.forest.ntrees == 3 && h.forest.nodes_per_tree == 16383 && h.forest.depth == 14, "  3 trees of 16383 nodes, depth 14");
	for (t = 0; t < 3; t++) {
		const fn_hd_node* nd = h.forest.nodes + (size_t)t * 16383;
		int two = 0, lo = 1 << 30, hi = -(1 << 30);
		double sum = 0;
		for (i = 0; i < 16383; i++) {
			two += nd[i].two;
			if (nd[i].thr < lo) lo = nd[i].thr;
			if (nd[i].thr > hi) hi = nd[i].thr;
			if (i >= 8191)
				sum += nd[i].leaf_l + nd[i].leaf_r;
		}
		snprintf(what, sizeof(what), "  tree %d: %d two-probe nodes, thresholds %d..%d mm, leaf mean %.2f", t, two, lo, hi, sum / 16384);
		CHECK(two == two_probe[t] && lo == thr_lo[t] && hi == thr_hi[t] && fabs(sum / 16384 - leaf_mean[t]) < 0.005, what);
	}
	{
		uint8_t bad[0x40];
		fn_hd_forest x;
		memcpy(bad, data, sizeof(bad));
		bad[12] ^= 1;
		CHECK(fn_hd_forest_load(&x, bad, sizeof(bad)) < 0, "  a wrong format id is refused");
	}
	free(data);

	// one frame: where the head is
	seated(0.05f, 0.30f, 1.8f, 1);
	frame(&h);
	{
		int best = -1, extra = 0;
		for (i = 0; i < h.ncand; i++) {
			v3 d = v3sub(h.cand[i].pos, head_at);
			printf("    (candidate (%.3f %.3f %.3f), score %.3f, box %.2f x %.2f m)\n", h.cand[i].pos.x, h.cand[i].pos.y,
			       h.cand[i].pos.z, h.cand[i].score, h.cand[i].mw, h.cand[i].mh);
			if (fabsf(d.x) < 0.06f && fabsf(d.y) < 0.08f && fabsf(d.z + 0.07f) < 0.07f)
				best = i;
			else if (h.cand[i].score >= 0.05f)
				extra++;
		}
		CHECK(best >= 0, "seated person at 1.8 m: a head candidate at the head (front surface)");
		CHECK(extra == 0, "  and no other candidate that would start a track");
	}

	// tracking
	memset(h.track, 0, sizeof(h.track));
	for (f = 1; f <= 12; f++)
		frame(&h);
	n = fn_hd_output(&h, out);
	CHECK(n == 1 && out[0].id && out[0].observed && out[0].rank == 1, "after 12 frames: one confirmed head");
	id = out[0].id;
	snprintf(what, sizeof(what), "  at (%.3f %.3f %.3f), box x %d..%d y %d..%d", out[0].position.x, out[0].position.y, out[0].position.z,
	         out[0].bbox[0], out[0].bbox[2], out[0].bbox[1], out[0].bbox[3]);
	CHECK(fabsf(out[0].position.x - 0.05f) < 0.06f && out[0].bbox[0] < out[0].bbox[2] && out[0].bbox[1] < out[0].bbox[3], what);
	for (f = 0; f < 15; f++) {
		seated(0.05f + 0.01f * f, 0.30f, 1.8f, 1); // leaning to the side
		frame(&h);
	}
	n = fn_hd_output(&h, out);
	CHECK(n == 1 && out[0].id == id && out[0].observed, "moved 15 cm to the side: the same head");
	for (f = 0; f < 20; f++) {
		seated(0.19f, 0.30f, 1.8f, 0);
		frame(&h);
	}
	n = fn_hd_output(&h, out);
	CHECK(n == 1 && out[0].id == id && !out[0].observed, "away for 20 frames: still there, not observed");
	for (f = 0; f < 3; f++) {
		seated(0.19f, 0.30f, 1.8f, 1);
		frame(&h);
	}
	n = fn_hd_output(&h, out);
	CHECK(n == 1 && out[0].id == id && out[0].observed, "back: the same head again");
	for (f = 0; f < 40; f++) {
		seated(0.19f, 0.30f, 1.8f, 0);
		frame(&h);
	}
	n = fn_hd_output(&h, out);
	CHECK(n == 1 && out[0].id == id && !out[0].observed, "away for 40 frames: lost, kept for a while");
	for (f = 0; f < 70; f++) {
		seated(0.19f, 0.30f, 1.8f, 0);
		frame(&h);
	}
	n = fn_hd_output(&h, out);
	CHECK(n == 0, "away for 110 frames: forgotten");

	// an empty chair is no head
	memset(h.track, 0, sizeof(h.track));
	for (f = 0; f < 15; f++) {
		seated(0.0f, 0.30f, 1.8f, 0);
		frame(&h);
	}
	CHECK(fn_hd_output(&h, out) == 0, "an empty chair: no head");

	fn_hd_forest_free(&h.forest);
	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
