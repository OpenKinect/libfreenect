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
 * Skeleton fit, libfreenect's own (not Kinect10's search):
 *
 * 1. Candidate choice: every joint takes one of its proposals or "missing";
 *    the choice minimises, exactly, by dynamic programming over the skeleton
 *    tree: -log(confidence) + distance from the predicted position (motion)
 *    + deviation of each bone from the user's length.
 * 2. Refinement: Gauss-Newton over all joint positions together, pulling
 *    each joint towards its chosen proposal (by confidence), keeping bone
 *    lengths, staying close to the prediction (strongly for joints without a
 *    proposal, which therefore move with the body: bounded extrapolation),
 *    and never in front of the measured depth surface.
 * 3. Bone lengths: population values scaled by the user's size until enough
 *    confident measurements are in (the first seconds), then the medians,
 *    left and right averaged, locked.
 * 4. From Kinect10: SPINE = HIP_CENTER + (SHOULDER_CENTER - HIP_CENTER) / 6,
 *    and the hips from the shoulders when not seen (§30.5.4.2); the clipping
 *    planes of the depth camera's frustum, which mark joints INFERRED and set
 *    the quality flags (§30.5.5.5).
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "nui_internal.h"

const int fn_nui_parent[NJ] = { 0, 0, 1, 2, 2, 4, 5, 6, 2, 8, 9, 10, 0, 12, 13, 14, 0, 16, 17, 18 };

// The fit's tree skips SPINE: SHOULDER_CENTER hangs off HIP_CENTER.
static const int fit_parent[NJ] = { -1, -1, 0, 2, 2, 4, 5, 6, 2, 8, 9, 10, 0, 12, 13, 14, 0, 16, 17, 18 };

// Adult lengths (m) of the bone from fit_parent[j] to j, for size factor 1.
static const float default_len[NJ] = {
	0.0f, 0.0f, 0.46f, 0.22f, 0.17f, 0.28f, 0.25f, 0.08f, 0.17f, 0.28f,
	0.25f, 0.08f, 0.09f, 0.42f, 0.40f, 0.10f, 0.09f, 0.42f, 0.40f, 0.10f,
};
static const int mirror_joint[NJ] = { 0, 1, 2, 3, 8, 9, 10, 11, 4, 5, 6, 7, 16, 17, 18, 19, 12, 13, 14, 15 };

#define S_MISSING 8 // state index for "no candidate"
#define TRACKED_CONF 0.2f
#define LEARN_FRAMES 90

void fn_nui_fit_reset(fn_nui_fit* f, float scale)
{
	int j;
	memset(f, 0, sizeof(*f));
	for (j = 0; j < NJ; j++)
		f->bone[j] = default_len[j] * (scale > 0.5f ? scale : 1.0f);
}

static float unary(const fn_nui_fit* f, int j, const fn_nui_cand* c)
{
	float u = -logf(c->conf + 0.02f);
	if (f->has_prev && f->seen[j] < 5) {
		v3 pred = v3add(f->joint[j], f->vel[j]), d = v3sub(c->p, pred);
		float t = v3dot(d, d) / (0.15f * 0.15f);
		u += t > 4.0f ? 4.0f : t;
	}
	return u;
}

static float pairwise(const fn_nui_fit* f, int j, v3 a, v3 b)
{
	float L = f->bone[j], d = fn_nui_v3len(v3sub(a, b)) - L, s = 0.05f + 0.15f * L, e = d * d / (s * s);
	return e > 6.0f ? 6.0f : e;
}

// children lists of the fit tree, in an order where children follow parents
static const int order[] = { 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 };
#define NORDER ((int)(sizeof(order) / sizeof(order[0])))

static void choose(const fn_nui_fit* f, const fn_nui_props* P, int sel[NJ])
{
	float cost[NJ][S_MISSING + 1];
	int best_child[NJ][S_MISSING + 1][4]; // per child of j (at most 3) the best child state
	int k, j, s;
	// leaves first: reverse order
	for (k = NORDER - 1; k >= 0; k--) {
		int nc = 0, children[4];
		j = order[k];
		for (s = 0; s < NJ; s++)
			if (s != j && fit_parent[s] == j)
				children[nc++] = s;
		for (s = 0; s <= S_MISSING; s++) {
			int ci;
			if (s < S_MISSING && s >= P->n[j]) {
				cost[j][s] = 1e30f;
				continue;
			}
			cost[j][s] = s == S_MISSING ? -logf(0.06f) : unary(f, j, &P->c[j][s]);
			for (ci = 0; ci < nc; ci++) {
				int c = children[ci], t, bt = S_MISSING;
				float bc = 1e30f;
				for (t = 0; t <= S_MISSING; t++) {
					float v = cost[c][t];
					if (v >= 1e29f)
						continue;
					if (s < S_MISSING && t < S_MISSING)
						v += pairwise(f, c, P->c[c][t].p, P->c[j][s].p);
					if (v < bc) {
						bc = v;
						bt = t;
					}
				}
				cost[j][s] += bc;
				best_child[j][s][ci] = bt;
			}
		}
	}
	// root, then down the tree
	{
		int bs = S_MISSING;
		for (s = 0; s <= S_MISSING; s++)
			if (cost[0][s] < cost[0][bs])
				bs = s;
		for (j = 0; j < NJ; j++)
			sel[j] = S_MISSING;
		sel[0] = bs;
		for (k = 0; k < NORDER; k++) {
			int nc = 0;
			j = order[k];
			for (s = 0; s < NJ; s++)
				if (s != j && fit_parent[s] == j)
					sel[s] = best_child[j][sel[j]][nc++];
		}
	}
}

/* ---- Gauss-Newton refinement over 19 joints (SPINE excluded) ---- */

#define NV (3 * NJ)

typedef struct {
	double A[NV][NV];
	double b[NV];
} normal_eq;

static void add_point_term(normal_eq* E, int j, v3 x, v3 target, double w)
{
	double r[3] = { x.x - target.x, x.y - target.y, x.z - target.z };
	int a;
	for (a = 0; a < 3; a++) {
		E->A[3 * j + a][3 * j + a] += w;
		E->b[3 * j + a] -= w * r[a];
	}
}

static void add_bone_term(normal_eq* E, int c, int p, v3 xc, v3 xp, double L, double w)
{
	v3 d = v3sub(xc, xp);
	double len = fn_nui_v3len(d), r, g[3];
	int a, bb;
	if (len < 1e-6)
		return;
	r = len - L;
	g[0] = d.x / len; g[1] = d.y / len; g[2] = d.z / len;
	for (a = 0; a < 3; a++) {
		for (bb = 0; bb < 3; bb++) {
			double v = w * g[a] * g[bb];
			E->A[3 * c + a][3 * c + bb] += v;
			E->A[3 * p + a][3 * p + bb] += v;
			E->A[3 * c + a][3 * p + bb] -= v;
			E->A[3 * p + a][3 * c + bb] -= v;
		}
		E->b[3 * c + a] -= w * r * g[a];
		E->b[3 * p + a] += w * r * g[a];
	}
}

static int cholesky_solve(double A[NV][NV], double* b, int n)
{
	int i, j, k;
	for (i = 0; i < n; i++) {
		for (j = 0; j <= i; j++) {
			double s = A[i][j];
			for (k = 0; k < j; k++)
				s -= A[i][k] * A[j][k];
			if (i == j) {
				if (s <= 0.0)
					return -1;
				A[i][i] = sqrt(s);
			} else {
				A[i][j] = s / A[j][j];
			}
		}
	}
	for (i = 0; i < n; i++) {
		double s = b[i];
		for (k = 0; k < i; k++)
			s -= A[i][k] * b[k];
		b[i] = s / A[i][i];
	}
	for (i = n - 1; i >= 0; i--) {
		double s = b[i];
		for (k = i + 1; k < n; k++)
			s -= A[k][i] * b[k];
		b[i] = s / A[i][i];
	}
	return 0;
}

static void refine(fn_nui_fit* f, v3 x[NJ], const v3 target[NJ], const float wtarget[NJ], const v3 pred[NJ],
                   const int have_pred, const uint16_t* map)
{
	static normal_eq E;
	int it, j;
	for (it = 0; it < 4; it++) {
		memset(&E, 0, sizeof(E));
		for (j = 0; j < NJ; j++) {
			if (j == 1)
				continue;
			if (wtarget[j] > 0.0f)
				add_point_term(&E, j, x[j], target[j], 4.0 * wtarget[j]);
			if (have_pred)
				add_point_term(&E, j, x[j], pred[j], wtarget[j] > 0.0f ? 0.05 : 0.6);
			if (fit_parent[j] >= 0)
				add_bone_term(&E, j, fit_parent[j], x[j], x[fit_parent[j]], f->bone[j], 3.0);
			// not in front of the measured surface (3 cm margin)
			{
				float u, v;
				int ui, vi, d;
				if (x[j].z <= 0.1f)
					continue;
				fn_nui_project(x[j], &u, &v);
				ui = (int)(u + 0.5f);
				vi = (int)(v + 0.5f);
				if (ui < 0 || ui >= NW || vi < 0 || vi >= NH)
					continue;
				d = map[vi * NW + ui];
				if (d > 0 && x[j].z < d * 0.001f - 0.03f) {
					double w = 2.0, r = x[j].z - (d * 0.001 - 0.03);
					E.A[3 * j + 2][3 * j + 2] += w;
					E.b[3 * j + 2] -= w * r;
				}
			}
		}
		// spine is not a variable; a tiny diagonal keeps the system regular
		for (j = 0; j < NV; j++)
			E.A[j][j] += 1e-4;
		if (cholesky_solve(E.A, E.b, NV) < 0)
			return;
		for (j = 0; j < NJ; j++) {
			if (j == 1)
				continue;
			x[j].x += (float)E.b[3 * j];
			x[j].y += (float)E.b[3 * j + 1];
			x[j].z += (float)E.b[3 * j + 2];
		}
	}
}

static void learn_bones(fn_nui_fit* f, const fn_nui_props* P, const int sel[NJ], float scale)
{
	int j;
	if (f->bones_locked)
		return;
	for (j = 2; j < NJ; j++) {
		int p = fit_parent[j];
		float l;
		if (p < 0 || sel[j] == S_MISSING || sel[p] == S_MISSING)
			continue;
		if (P->c[j][sel[j]].conf < 0.4f || P->c[p][sel[p]].conf < 0.4f)
			continue;
		l = fn_nui_v3len(v3sub(P->c[j][sel[j]].p, P->c[p][sel[p]].p));
		if (l < 0.5f * default_len[j] * scale || l > 1.6f * default_len[j] * scale)
			continue;
		if (f->bone_n[j] < 64)
			f->bone_acc[j][f->bone_n[j]++] = l;
	}
	if (++f->learn_frames < LEARN_FRAMES)
		return;
	for (j = 2; j < NJ; j++) {
		int n = f->bone_n[j], a, b;
		float* s = f->bone_acc[j];
		if (n < 10)
			continue;
		for (a = 1; a < n; a++) // insertion sort; n <= 64
			for (b = a; b > 0 && s[b - 1] > s[b]; b--) {
				float t = s[b];
				s[b] = s[b - 1];
				s[b - 1] = t;
			}
		f->bone[j] = s[n / 2];
	}
	for (j = 4; j < NJ; j++) { // left and right the same
		int m = mirror_joint[j];
		if (m > j) {
			float a = 0.5f * (f->bone[j] + f->bone[m]);
			f->bone[j] = f->bone[m] = a;
		}
	}
	f->bones_locked = 1;
}

static uint32_t clip_flags(v3 p)
{
	uint32_t q = 0;
	if (-p.y + 0.4201245f * p.z < 0.0f) q |= FREENECT_NUI_CLIPPED_TOP;
	else if (p.y + 0.4201245f * p.z < 0.0f) q |= FREENECT_NUI_CLIPPED_BOTTOM;
	if (p.x + 0.560166f * p.z < 0.0f) q |= FREENECT_NUI_CLIPPED_LEFT;
	else if (-p.x + 0.560166f * p.z < 0.0f) q |= FREENECT_NUI_CLIPPED_RIGHT;
	return q;
}

int fn_nui_fit_user(fn_nui_fit* f, const fn_nui_props* P, const uint16_t* map, const uint8_t* mask,
                    int slot, float scale)
{
	int sel[NJ], j, tracked = 0, good;
	v3 x[NJ], target[NJ], pred[NJ];
	float wt[NJ];
	(void)mask;
	(void)slot;

	choose(f, P, sel);

	for (j = 0; j < NJ; j++) {
		pred[j] = f->has_prev ? v3add(f->joint[j], f->vel[j]) : v3make(0, 0, 0);
		if (sel[j] != S_MISSING) {
			target[j] = P->c[j][sel[j]].p;
			wt[j] = P->c[j][sel[j]].conf;
			x[j] = target[j];
		} else {
			target[j] = v3make(0, 0, 0);
			wt[j] = 0.0f;
			x[j] = pred[j];
		}
	}
	// joints without a proposal and without history: hang them off the parent
	for (j = 0; j < NJ; j++) {
		int p = fit_parent[j];
		if (j == 1 || sel[j] != S_MISSING || f->has_prev)
			continue;
		if (j == 0) {
			// no hip centre: below the shoulder centre, if seen
			if (sel[2] != S_MISSING)
				x[0] = v3add(P->c[2][sel[2]].p, v3make(0, -f->bone[2], 0));
			else
				return 0;
			continue;
		}
		if (p >= 0) {
			v3 off = v3make(0, -f->bone[j], 0);
			if (j == 4 || j == 12) off = v3make(-f->bone[j], 0, 0);
			if (j == 8 || j == 16) off = v3make(f->bone[j], 0, 0);
			if (j == 3) off = v3make(0, f->bone[j], 0);
			if (j == 2) off = v3make(0, f->bone[j], 0);
			x[j] = v3add(x[p], off);
		}
	}
	// the hips from the shoulders when not seen (§30.5.4.2)
	if (sel[12] == S_MISSING && sel[16] == S_MISSING && sel[0] != S_MISSING && sel[4] != S_MISSING && sel[8] != S_MISSING) {
		v3 d = v3scale(v3sub(x[8], x[4]), 0.45f);
		d.z = 0.0f;
		x[12] = v3sub(x[0], v3scale(d, 0.5f));
		x[16] = v3add(x[0], v3scale(d, 0.5f));
	}

	refine(f, x, target, wt, pred, f->has_prev, map);
	learn_bones(f, P, sel, scale);

	// SPINE (§30.5.4.2)
	x[1] = v3add(x[0], v3scale(v3sub(x[2], x[0]), 1.0f / 6.0f));

	f->quality = 0;
	for (j = 0; j < NJ; j++) {
		uint32_t q = clip_flags(x[j]);
		int st;
		if (j == 1)
			st = (f->jstate[0] == 2 && f->jstate[2] == 2) ? 2 : 1;
		else if (sel[j] != S_MISSING && wt[j] >= TRACKED_CONF && fn_nui_v3len(v3sub(x[j], target[j])) < 0.15f)
			st = 2;
		else
			st = 1;
		if (q) {
			f->quality |= q;
			if (st == 2)
				st = 1;
		}
		f->jstate[j] = st;
		if (st == 2)
			tracked++;
		f->seen[j] = sel[j] != S_MISSING ? 0 : f->seen[j] + 1;
		if (f->has_prev) {
			v3 dv = v3sub(x[j], f->joint[j]);
			f->vel[j] = v3add(v3scale(dv, 0.5f), v3scale(f->vel[j], 0.5f));
		}
		f->prev[j] = f->joint[j];
		f->joint[j] = x[j];
	}
	// SPINE again, with the final states of its two ends
	f->jstate[1] = (f->jstate[0] == 2 && f->jstate[2] == 2) ? 2 : 1;
	f->has_prev = 1;
	good = tracked >= 8 && f->jstate[3] == 2 && f->jstate[2] == 2;
	return good;
}
