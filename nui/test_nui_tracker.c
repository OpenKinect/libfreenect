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
 * The tracker on rendered depth: a person built from capsules (torso,
 * pelvis, head, limbs, feet) standing on a floor 1 m below the Kinect, in
 * front of a wall, rendered as 640x480 millimetre depth in libfreenect's
 * orientation. Needs DATABASE.WSDF (skipped without it), and the forest was
 * trained on people, not capsules, so the thresholds are generous; the point
 * is the whole pipeline end to end: floor, segmentation without motion,
 * verification, joints near the truth, smoothing, bone orientations.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nui_internal.h"
#include "models.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

#define FLOOR_Y -1.0f
#define WALL_Z 4.0f
#define F640 571.26f

typedef struct {
	v3 a, b;
	float r;
} capsule;

static v3 J[NJ]; // ground truth, skeleton space

static void pose(float t, float x0, float z0)
{
	float swing = 0.25f * sinf(t);
	float hip_y = FLOOR_Y + 0.95f;
	J[0] = v3make(x0, hip_y, z0);
	J[2] = v3make(x0, hip_y + 0.46f, z0);
	J[1] = v3add(J[0], v3scale(v3sub(J[2], J[0]), 1.0f / 6.0f));
	J[3] = v3make(x0, hip_y + 0.68f, z0);
	// left is +x in skeleton space seen from the Kinect? The user's left
	// shoulder is on the Kinect's right = -x... keep the SDK's convention:
	// SHOULDER_LEFT is at -x when facing the Kinect (mirrored image left)
	J[4] = v3make(x0 - 0.17f, hip_y + 0.44f, z0);
	J[5] = v3make(x0 - 0.30f, hip_y + 0.20f, z0 + swing * 0.3f);
	J[6] = v3make(x0 - 0.40f, hip_y + 0.0f + swing * 0.2f, z0 - 0.05f);
	J[7] = v3make(x0 - 0.43f, hip_y - 0.08f + swing * 0.2f, z0 - 0.07f);
	J[8] = v3make(x0 + 0.17f, hip_y + 0.44f, z0);
	J[9] = v3make(x0 + 0.30f, hip_y + 0.20f, z0 - swing * 0.3f);
	J[10] = v3make(x0 + 0.40f, hip_y + 0.0f - swing * 0.2f, z0 - 0.05f);
	J[11] = v3make(x0 + 0.43f, hip_y - 0.08f - swing * 0.2f, z0 - 0.07f);
	J[12] = v3make(x0 - 0.09f, hip_y - 0.03f, z0);
	J[13] = v3make(x0 - 0.11f, hip_y - 0.45f, z0 - 0.03f);
	J[14] = v3make(x0 - 0.11f, FLOOR_Y + 0.08f, z0);
	J[15] = v3make(x0 - 0.12f, FLOOR_Y + 0.04f, z0 - 0.12f);
	J[16] = v3make(x0 + 0.09f, hip_y - 0.03f, z0);
	J[17] = v3make(x0 + 0.11f, hip_y - 0.45f, z0 - 0.03f);
	J[18] = v3make(x0 + 0.11f, FLOOR_Y + 0.08f, z0);
	J[19] = v3make(x0 + 0.12f, FLOOR_Y + 0.04f, z0 - 0.12f);
}

static int body(capsule* c)
{
	int n = 0;
#define CAP(A, B, R) do { c[n].a = (A); c[n].b = (B); c[n].r = (R); n++; } while (0)
	CAP(v3add(J[0], v3make(0, 0.05f, 0)), v3sub(J[2], v3make(0, 0.05f, 0)), 0.15f);   // torso
	CAP(J[12], J[16], 0.11f);                                                         // pelvis
	CAP(J[4], J[8], 0.07f);                                                           // shoulders
	CAP(v3sub(J[3], v3make(0, 0.04f, 0)), v3add(J[3], v3make(0, 0.04f, 0)), 0.10f);  // head
	CAP(J[2], J[3], 0.05f);                                                           // neck
	CAP(J[4], J[5], 0.05f); CAP(J[5], J[6], 0.04f); CAP(J[6], J[7], 0.045f);
	CAP(J[8], J[9], 0.05f); CAP(J[9], J[10], 0.04f); CAP(J[10], J[11], 0.045f);
	CAP(J[12], J[13], 0.08f); CAP(J[13], J[14], 0.06f); CAP(J[14], J[15], 0.045f);
	CAP(J[16], J[17], 0.08f); CAP(J[17], J[18], 0.06f); CAP(J[18], J[19], 0.045f);
#undef CAP
	return n;
}

// ray from the origin along d: distance to a capsule, or -1
static float hit_capsule(v3 d, const capsule* c)
{
	// sample along the ray: cheap and exact enough at 1 mm steps near the body
	v3 ab = v3sub(c->b, c->a);
	float ab2 = v3dot(ab, ab), t, best = -1.0f;
	float tc = v3dot(v3scale(v3add(c->a, c->b), 0.5f), d) / v3dot(d, d);
	float span = 0.5f * sqrtf(ab2) + c->r + 0.05f;
	for (t = tc - span; t <= tc + span; t += 0.002f) {
		v3 p = v3scale(d, t), ap = v3sub(p, c->a);
		float h = ab2 > 0 ? v3dot(ap, ab) / ab2 : 0.0f;
		v3 q;
		if (h < 0) h = 0;
		if (h > 1) h = 1;
		q = v3sub(p, v3add(c->a, v3scale(ab, h)));
		if (v3dot(q, q) <= c->r * c->r) {
			best = t;
			break;
		}
	}
	return best;
}

static void render(uint16_t* depth, int with_person)
{
	static capsule caps[32];
	int n = with_person ? body(caps) : 0, X, Y, k;
	for (Y = 0; Y < 480; Y++)
		for (X = 0; X < 640; X++) {
			float xm = (float)(639 - X); // libfreenect orientation -> mirrored
			v3 d = v3make((xm - 319.5f) / F640, -((float)Y - 239.5f) / F640, 1.0f);
			float t = WALL_Z; // wall: z = 4 m
			if (d.y < 0 && FLOOR_Y / d.y < t)
				t = FLOOR_Y / d.y;
			for (k = 0; k < n; k++) {
				float h;
				// quick reject by angle
				v3 m = v3scale(v3add(caps[k].a, caps[k].b), 0.5f);
				float tm = m.z, dx = d.x * tm - m.x, dy = d.y * tm - m.y;
				float reach = 0.5f * fn_nui_v3len(v3sub(caps[k].a, caps[k].b)) + caps[k].r;
				if (dx * dx + dy * dy > reach * reach * 1.2f)
					continue;
				h = hit_capsule(d, &caps[k]);
				if (h > 0 && h < t)
					t = h;
			}
			depth[Y * 640 + X] = (uint16_t)(t * 1000.0f + 0.5f);
		}
}

int main(void)
{
	static uint16_t depth[640 * 480];
	static uint8_t pidx[640 * 480];
	freenect_nui* n;
	freenect_nui_frame fr;
	int f, tracked_at = -1, k, users;
	char path[1024];
	size_t len;
	uint8_t* db = fn_nui_read_model(NULL, FN_NUI_FOREST_FILE, &len, path, sizeof(path));
	if (!db) {
		printf("DATABASE.WSDF not installed (fwfetcher.py --nui); skipped\n");
		return 77;
	}
	free(db);
	printf("Skeleton tracker on a rendered scene (nui/)\n");
	n = freenect_nui_create(NULL, 0);
	CHECK(n != NULL, "tracker created");
	if (!n)
		return 1;

	// empty room first, then the person (standing still apart from the arms)
	for (f = 0; f < 90; f++) {
		pose(0.15f * f, 0.1f, 2.3f);
		render(depth, f >= 20);
		freenect_nui_process(n, depth, (uint32_t)f, &fr);
		if (tracked_at < 0)
			for (k = 0; k < NU; k++)
				if (fr.users[k].state == FREENECT_NUI_USER_TRACKED)
					tracked_at = f;
	}
	printf("    (floor %.3f %.3f %.3f %.3f; tracked from frame %d)\n", fr.floor_plane[0], fr.floor_plane[1],
	       fr.floor_plane[2], fr.floor_plane[3], tracked_at);
	CHECK(fabsf(fr.floor_plane[1] - 1.0f) < 0.01f && fabsf(fr.floor_plane[3] - 1.0f) < 0.05f,
	      "floor found: y = -1 m (B ~ 1, D ~ 1 m)");
	users = 0;
	for (k = 0; k < NU; k++)
		users += fr.users[k].state != FREENECT_NUI_USER_NOT_TRACKED;
	CHECK(users == 1, "exactly one user");
	CHECK(tracked_at >= 0, "the user gets a skeleton");
	for (k = 0; k < NU; k++) {
		freenect_nui_skeleton* S = &fr.users[k];
		int j, nt = 0;
		float err = 0.0f, worst = 0.0f;
		const char* names[NJ] = { "hipC", "spine", "shC", "head", "shL", "elL", "wrL", "haL", "shR", "elR", "wrR", "haR",
		                          "hipL", "knL", "anL", "ftL", "hipR", "knR", "anR", "ftR" };
		if (S->state != FREENECT_NUI_USER_TRACKED)
			continue;
		printf("    joint errors (cm), T = tracked:\n     ");
		for (j = 0; j < NJ; j++) {
			v3 p = v3make(S->joints[j].x, S->joints[j].y, S->joints[j].z);
			float e = fn_nui_v3len(v3sub(p, J[j]));
			printf(" %s %.0f%s", names[j], 100 * e, S->joint_state[j] == 2 ? "T" : "");
			if (j == 9) printf("\n     ");
			if (S->joint_state[j] == FREENECT_NUI_JOINT_TRACKED) {
				nt++;
				err += e;
				if (e > worst) worst = e;
			}
		}
		printf("\n    (%d tracked joints, mean error %.1f cm, worst %.1f cm; user at %.2f m)\n", nt,
		       nt ? 100 * err / nt : 0.0f, 100 * worst, S->position.z);
		CHECK(nt >= 10, "at least 10 joints tracked");
		CHECK(nt && err / nt < 0.12f, "tracked joints within 12 cm of the truth on average");
		{
			v3 h = v3make(S->joints[3].x, S->joints[3].y, S->joints[3].z), hip = v3make(S->joints[0].x, S->joints[0].y, S->joints[0].z);
			CHECK(fn_nui_v3len(v3sub(h, J[3])) < 0.15f && fn_nui_v3len(v3sub(hip, J[0])) < 0.15f, "head and hip centre within 15 cm");
		}
		{
			freenect_nui_bone bones[NJ];
			int ok = freenect_nui_bone_orientations(S, bones) == 0, b;
			for (b = 0; b < NJ && ok; b++) {
				float q = bones[b].absolute.quaternion[0] * bones[b].absolute.quaternion[0] +
				          bones[b].absolute.quaternion[1] * bones[b].absolute.quaternion[1] +
				          bones[b].absolute.quaternion[2] * bones[b].absolute.quaternion[2] +
				          bones[b].absolute.quaternion[3] * bones[b].absolute.quaternion[3];
				ok &= fabsf(q - 1.0f) < 1e-3f;
			}
			CHECK(ok, "bone orientations: unit quaternions for all 20 bones");
		}
	}
	freenect_nui_get_player_index(n, pidx);
	{
		int X, Y, inside = 0, outside = 0;
		for (Y = 0; Y < 480; Y++)
			for (X = 0; X < 640; X++)
				if (pidx[Y * 640 + X]) {
					// rendered person pixels are nearer than the wall and above the floor
					if (depth[Y * 640 + X] < 3500) inside++;
					else outside++;
				}
		printf("    (player index: %d pixels on the person, %d elsewhere)\n", inside, outside);
		CHECK(inside > 5000 && outside < inside / 50, "player index covers the person and little else");
	}
	{
		freenect_nui_smoother* sm = freenect_nui_smoother_create();
		CHECK(sm && freenect_nui_smooth(sm, &fr, NULL) == 0, "smoothing runs");
		freenect_nui_smoother_destroy(sm);
	}
	freenect_nui_destroy(n);
	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
