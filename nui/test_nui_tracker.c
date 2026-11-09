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
 * The tracker on rendered depth: people built from capsules (torso, pelvis,
 * head, limbs, feet) standing on a floor 1 m below the Kinect, in front of a
 * wall at 4 m, rendered as 640x480 millimetre depth in libfreenect's
 * orientation. Needs DATABASE.WSDF (skipped without it).
 *
 * Scenarios, each scored with PCK@10cm (share of tracked joints within
 * 10 cm of the truth) and the mean error, after the skeleton locks:
 *   - one user standing at 1.6, 2.3 and 3.3 m, arms swinging; the room is
 *     empty for the first frames, then the user is there without walking in
 *   - two users side by side: two skeletons, distinct ids and player indices
 *   - one user walking across the room: the same id throughout
 * plus the floor plane, the player index, smoothing and bone orientations.
 * The forest was trained on people, not capsules; the thresholds leave room
 * for that.
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
#define MAXP 2

typedef struct {
	v3 a, b;
	float r;
} capsule;

typedef struct {
	v3 J[NJ]; // ground truth, skeleton space
} person;

// SHOULDER_LEFT etc. at -x: the user's left is on the mirrored image's left
static void pose(person* P, float t, float x0, float z0)
{
	v3* J = P->J;
	float swing = 0.25f * sinf(t), hip_y = FLOOR_Y + 0.95f;
	J[0] = v3make(x0, hip_y, z0);
	J[2] = v3make(x0, hip_y + 0.46f, z0);
	J[1] = v3add(J[0], v3scale(v3sub(J[2], J[0]), 1.0f / 6.0f));
	J[3] = v3make(x0, hip_y + 0.68f, z0);
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

static int body(const person* P, capsule* c)
{
	const v3* J = P->J;
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

// distance (as z) along the ray through d to a capsule, or -1; 2 mm steps
static float hit_capsule(v3 d, const capsule* c)
{
	v3 ab = v3sub(c->b, c->a);
	float ab2 = v3dot(ab, ab), t;
	float tc = v3dot(v3scale(v3add(c->a, c->b), 0.5f), d) / v3dot(d, d);
	float span = 0.5f * sqrtf(ab2) + c->r + 0.05f;
	for (t = tc - span; t <= tc + span; t += 0.002f) {
		v3 p = v3scale(d, t), ap = v3sub(p, c->a), q;
		float h = ab2 > 0 ? v3dot(ap, ab) / ab2 : 0.0f;
		if (h < 0) h = 0;
		if (h > 1) h = 1;
		q = v3sub(p, v3add(c->a, v3scale(ab, h)));
		if (v3dot(q, q) <= c->r * c->r)
			return t;
	}
	return -1.0f;
}

static void render(uint16_t* depth, const person* people, int np)
{
	static capsule caps[MAXP * 20];
	int n = 0, X, Y, k, p;
	for (p = 0; p < np; p++)
		n += body(&people[p], caps + n);
	for (Y = 0; Y < 480; Y++)
		for (X = 0; X < 640; X++) {
			float xm = (float)(639 - X); // libfreenect orientation -> mirrored
			v3 d = v3make((xm - 319.5f) / F640, -((float)Y - 239.5f) / F640, 1.0f);
			float t = WALL_Z;
			if (d.y < 0 && FLOOR_Y / d.y < t)
				t = FLOOR_Y / d.y;
			for (k = 0; k < n; k++) {
				v3 m = v3scale(v3add(caps[k].a, caps[k].b), 0.5f);
				float dx = d.x * m.z - m.x, dy = d.y * m.z - m.y, h;
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

typedef struct {
	int frames_tracked, joints, within10;
	double err;
	int lock_frame;
	uint32_t ids[MAXP];
	int id_changes;
	int users_max;
} score;

// The user (by position) closest to person p.
static freenect_nui_skeleton* match(freenect_nui_frame* fr, const person* P)
{
	int k, best = -1;
	float bd = 0.5f;
	for (k = 0; k < NU; k++) {
		freenect_nui_skeleton* S = &fr->users[k];
		float d;
		if (S->state == FREENECT_NUI_USER_NOT_TRACKED)
			continue;
		d = fabsf(S->position.x - P->J[0].x) + fabsf(S->position.z - P->J[0].z);
		if (d < bd) {
			bd = d;
			best = k;
		}
	}
	return best >= 0 ? &fr->users[best] : NULL;
}

typedef void (*scene_fn)(person* people, int* np, int f);

static void run(freenect_nui* n, scene_fn scene, int frames, score* s, freenect_nui_frame* last, uint16_t* depth)
{
	person people[MAXP];
	freenect_nui_frame fr;
	int f, np, p, k;
	memset(s, 0, sizeof(*s));
	s->lock_frame = -1;
	freenect_nui_reset(n);
	for (f = 0; f < frames; f++) {
		int users = 0;
		scene(people, &np, f);
		render(depth, people, np);
		freenect_nui_process(n, depth, (uint32_t)f, &fr);
		for (k = 0; k < NU; k++)
			users += fr.users[k].state != FREENECT_NUI_USER_NOT_TRACKED;
		if (users > s->users_max)
			s->users_max = users;
		for (p = 0; p < np; p++) {
			freenect_nui_skeleton* S = match(&fr, &people[p]);
			int j, all = 1;
			if (!S || S->state != FREENECT_NUI_USER_TRACKED) {
				continue;
			}
			if (s->ids[p] && s->ids[p] != S->tracking_id)
				s->id_changes++;
			s->ids[p] = S->tracking_id;
			if (s->lock_frame < 0)
				s->lock_frame = f;
			if (f < s->lock_frame + 10) // score once the bone lengths have settled a little
				continue;
			s->frames_tracked++;
			for (j = 0; j < NJ; j++) {
				v3 q = v3make(S->joints[j].x, S->joints[j].y, S->joints[j].z);
				float e = fn_nui_v3len(v3sub(q, people[p].J[j]));
				if (S->joint_state[j] != FREENECT_NUI_JOINT_TRACKED) {
					all = 0;
					continue;
				}
				s->joints++;
				s->err += e;
				s->within10 += e < 0.10f;
			}
			(void)all;
		}
	}
	*last = fr;
}

static float sc_x, sc_z;
static void one_user(person* P, int* np, int f)
{
	*np = f >= 15;
	pose(&P[0], 0.2f * f, sc_x, sc_z);
}
static void two_users(person* P, int* np, int f)
{
	*np = f >= 15 ? 2 : 0;
	pose(&P[0], 0.2f * f, -0.65f, 2.5f);
	pose(&P[1], 0.2f * f + 1.0f, 0.65f, 2.7f);
}
static void walking(person* P, int* np, int f)
{
	*np = f >= 15;
	pose(&P[0], 0.2f * f, -0.9f + 0.025f * (f - 15 > 0 ? f - 15 : 0), 2.4f);
}

static void report(const char* name, const score* s)
{
	printf("    (%s: locked at frame %d, %d scored frames, PCK@10cm %.1f %%, mean error %.1f cm, %d users seen)\n", name,
	       s->lock_frame, s->frames_tracked, s->joints ? 100.0 * s->within10 / s->joints : 0.0,
	       s->joints ? 100.0 * s->err / s->joints : 0.0, s->users_max);
}

int main(void)
{
	static uint16_t depth[640 * 480];
	static uint8_t pidx[640 * 480];
	freenect_nui* n;
	freenect_nui_frame fr;
	score s;
	char path[1024], what[120];
	size_t len;
	int i;
	static const float dist[3] = { 1.6f, 2.3f, 3.3f };
	uint8_t* db = fn_nui_read_model(NULL, FN_NUI_FOREST_FILE, &len, path, sizeof(path));
	if (!db) {
		printf("DATABASE.WSDF not installed (fwfetcher.py --nui); skipped\n");
		return 77;
	}
	free(db);
	printf("Skeleton tracker on rendered scenes (nui/)\n");
	n = freenect_nui_create(NULL, 0);
	CHECK(n != NULL, "tracker created");
	if (!n)
		return 1;

	for (i = 0; i < 3; i++) {
		sc_x = 0.1f;
		sc_z = dist[i];
		run(n, one_user, 75, &s, &fr, depth);
		snprintf(what, sizeof(what), "one user at %.1f m", dist[i]);
		report(what, &s);
		// at 1.6 m the bottom of the view cuts the shins (the Kinect is 1 m
		// above the floor): the knees just above the cut are the weak joints
		snprintf(what, sizeof(what), "one user at %.1f m: skeleton, PCK@10cm >= %d %%", dist[i], i == 0 ? 75 : 85);
		CHECK(s.lock_frame >= 15 && s.lock_frame <= 30 && s.users_max == 1 && s.joints > 0 &&
		      s.within10 >= (i == 0 ? 0.75 : 0.85) * s.joints, what);
		if (i == 1) {
			int X, Y, inside = 0, outside = 0, k;
			printf("    (floor %.3f %.3f %.3f %.3f)\n", fr.floor_plane[0], fr.floor_plane[1], fr.floor_plane[2], fr.floor_plane[3]);
			CHECK(fabsf(fr.floor_plane[1] - 1.0f) < 0.01f && fabsf(fr.floor_plane[3] - 1.0f) < 0.05f,
			      "floor found: y = -1 m (B ~ 1, D ~ 1 m)");
			freenect_nui_get_player_index(n, pidx);
			for (Y = 0; Y < 480; Y++)
				for (X = 0; X < 640; X++)
					if (pidx[Y * 640 + X]) {
						if (depth[Y * 640 + X] < 3500) inside++;
						else outside++;
					}
			CHECK(inside > 5000 && outside < inside / 50, "player index on the person and little else");
			for (k = 0; k < NU; k++)
				if (fr.users[k].state == FREENECT_NUI_USER_TRACKED) {
					freenect_nui_bone bones[NJ];
					int ok = freenect_nui_bone_orientations(&fr.users[k], bones) == 0, b;
					for (b = 0; b < NJ && ok; b++) {
						const float* q = bones[b].absolute.quaternion;
						ok &= fabsf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3] - 1.0f) < 1e-3f;
					}
					CHECK(ok, "bone orientations: unit quaternions for all 20 bones");
				}
			{
				freenect_nui_smoother* sm = freenect_nui_smoother_create();
				CHECK(sm && freenect_nui_smooth(sm, &fr, NULL) == 0, "smoothing runs");
				freenect_nui_smoother_destroy(sm);
			}
		}
	}

	run(n, two_users, 75, &s, &fr, depth);
	report("two users", &s);
	{
		int k, tracked = 0;
		uint32_t ids[2] = { 0, 0 };
		for (k = 0; k < NU; k++)
			if (fr.users[k].state == FREENECT_NUI_USER_TRACKED) {
				if (tracked < 2)
					ids[tracked] = fr.users[k].tracking_id;
				tracked++;
			}
		CHECK(tracked == 2 && ids[0] != ids[1], "two users side by side: two skeletons, distinct ids");
		CHECK(s.joints > 0 && s.within10 >= 0.80 * s.joints, "  PCK@10cm >= 80 %");
		freenect_nui_get_player_index(n, pidx);
		{
			int c1 = 0, c2 = 0, X;
			for (X = 0; X < 640 * 480; X++) {
				if (pidx[X] == 1) c1++;
				if (pidx[X] == 2) c2++;
			}
			CHECK(c1 > 3000 && c2 > 3000, "  each has its own player index");
		}
	}

	run(n, walking, 75, &s, &fr, depth);
	report("walking", &s);
	CHECK(s.lock_frame >= 0 && s.id_changes == 0 && s.users_max == 1, "a user walking across: the same id throughout");
	CHECK(s.joints > 0 && s.within10 >= 0.80 * s.joints, "  PCK@10cm >= 80 %");

	freenect_nui_destroy(n);
	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
