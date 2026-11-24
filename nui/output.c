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
 * The SDK's post-processing, as Kinect10 does it:
 * - NuiTransformSmooth: the Holt double exponential filter with jitter
 *   removal and a deviation limit, per joint, history per user slot
 *   (§30.6.3);
 * - NuiSkeletonCalculateBoneOrientations: the per-bone axis rules, left and
 *   right as observed (they are not mirror images), hierarchical rotation =
 *   M transpose(parent's absolute) (§30.6.4, §30.8.8).
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "nui_internal.h"

/* ------------------------------------------------------------ smoothing */

enum { F_HOLT, F_ONE_EURO, F_KALMAN };
#define DT (1.0f / 30.0f)

typedef struct {
	float raw_prev[4], filtered[4], trend[4]; // Holt
	float x[3], dx[3];                        // One-Euro: estimate and speed
	float kp[3], kv[3], P[3][3];              // Kalman per axis: position, velocity, covariance (00, 01, 11)
	int frames;
} joint_filter;

struct freenect_nui_smoother {
	joint_filter j[NU][FREENECT_NUI_JOINTS]; // body and hand joints
	uint32_t id[NU];                         // the user each slot's history belongs to
	int kind;
	float min_cutoff, beta, d_cutoff;        // One-Euro
	float accel_noise, meas_noise;           // Kalman
};

freenect_nui_smoother* freenect_nui_smoother_create(void)
{
	freenect_nui_smoother* s = (freenect_nui_smoother*)calloc(1, sizeof(freenect_nui_smoother));
	if (s) {
		s->kind = F_HOLT;
		s->min_cutoff = 1.0f;
		s->beta = 1.5f;
		s->d_cutoff = 1.0f;
		s->accel_noise = 3.0f;
		s->meas_noise = 0.02f;
	}
	return s;
}

int freenect_nui_smoother_set_holt(freenect_nui_smoother* s)
{
	if (!s)
		return -1;
	s->kind = F_HOLT;
	memset(s->j, 0, sizeof(s->j));
	return 0;
}

int freenect_nui_smoother_set_one_euro(freenect_nui_smoother* s, float min_cutoff, float beta, float d_cutoff)
{
	if (!s || min_cutoff <= 0.0f || beta < 0.0f || d_cutoff <= 0.0f)
		return -1;
	s->kind = F_ONE_EURO;
	s->min_cutoff = min_cutoff;
	s->beta = beta;
	s->d_cutoff = d_cutoff;
	memset(s->j, 0, sizeof(s->j));
	return 0;
}

int freenect_nui_smoother_set_kalman(freenect_nui_smoother* s, float accel_noise, float measurement_noise)
{
	if (!s || accel_noise <= 0.0f || measurement_noise <= 0.0f)
		return -1;
	s->kind = F_KALMAN;
	s->accel_noise = accel_noise;
	s->meas_noise = measurement_noise;
	memset(s->j, 0, sizeof(s->j));
	return 0;
}

// the smoothing factor of a first-order low-pass at `cutoff` Hz
static float alpha(float cutoff)
{
	float tau = 1.0f / (2.0f * 3.14159265f * cutoff);
	return 1.0f / (1.0f + tau / DT);
}

static void one_euro_user(const freenect_nui_smoother* sm, joint_filter* J, freenect_nui_skeleton* s)
{
	int j, c;
	for (j = 0; j < FREENECT_NUI_JOINTS; j++) {
		joint_filter* F = &J[j];
		float raw[3], speed = 0.0f, a, ad = alpha(sm->d_cutoff);
		raw[0] = s->joints[j].x; raw[1] = s->joints[j].y; raw[2] = s->joints[j].z;
		if (!s->joint_state[j] || (raw[0] == 0.0f && raw[1] == 0.0f && raw[2] == 0.0f)) {
			F->frames = 0;
			continue;
		}
		if (F->frames == 0) {
			memcpy(F->x, raw, sizeof(raw));
			memset(F->dx, 0, sizeof(F->dx));
			F->frames = 1;
			continue;
		}
		// the speed, low-passed, sets the cutoff
		for (c = 0; c < 3; c++) {
			F->dx[c] += ad * ((raw[c] - F->x[c]) / DT - F->dx[c]);
			speed += F->dx[c] * F->dx[c];
		}
		a = alpha(sm->min_cutoff + sm->beta * sqrtf(speed));
		for (c = 0; c < 3; c++)
			F->x[c] += a * (raw[c] - F->x[c]);
		s->joints[j].x = F->x[0];
		s->joints[j].y = F->x[1];
		s->joints[j].z = F->x[2];
	}
}

static void kalman_user(const freenect_nui_smoother* sm, joint_filter* J, freenect_nui_skeleton* s)
{
	// white acceleration noise: Q = q^2 [dt^4/4 dt^3/2; dt^3/2 dt^2]
	float q2 = sm->accel_noise * sm->accel_noise, q00 = q2 * DT * DT * DT * DT / 4, q01 = q2 * DT * DT * DT / 2, q11 = q2 * DT * DT;
	int j, c;
	for (j = 0; j < FREENECT_NUI_JOINTS; j++) {
		joint_filter* F = &J[j];
		float raw[3], r = sm->meas_noise;
		raw[0] = s->joints[j].x; raw[1] = s->joints[j].y; raw[2] = s->joints[j].z;
		if (!s->joint_state[j] || (raw[0] == 0.0f && raw[1] == 0.0f && raw[2] == 0.0f)) {
			F->frames = 0;
			continue;
		}
		if (s->joint_state[j] == FREENECT_NUI_JOINT_INFERRED)
			r *= 4.0f;
		if (F->frames == 0) {
			for (c = 0; c < 3; c++) {
				F->kp[c] = raw[c];
				F->kv[c] = 0.0f;
				F->P[c][0] = r * r;
				F->P[c][1] = 0.0f;
				F->P[c][2] = 1.0f; // speed unknown: (1 m/s)^2
			}
			F->frames = 1;
			continue;
		}
		for (c = 0; c < 3; c++) {
			// predict
			float p = F->kp[c] + F->kv[c] * DT, v = F->kv[c];
			float P00 = F->P[c][0] + 2 * DT * F->P[c][1] + DT * DT * F->P[c][2] + q00;
			float P01 = F->P[c][1] + DT * F->P[c][2] + q01;
			float P11 = F->P[c][2] + q11;
			// update with the measured position
			float S = P00 + r * r, k0 = P00 / S, k1 = P01 / S, e = raw[c] - p;
			F->kp[c] = p + k0 * e;
			F->kv[c] = v + k1 * e;
			F->P[c][0] = (1 - k0) * P00;
			F->P[c][1] = (1 - k0) * P01;
			F->P[c][2] = P11 - k1 * P01;
		}
		s->joints[j].x = F->kp[0];
		s->joints[j].y = F->kp[1];
		s->joints[j].z = F->kp[2];
	}
}

void freenect_nui_smoother_destroy(freenect_nui_smoother* s)
{
	free(s);
}

static float len3(const float* a, const float* b)
{
	float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

static void filter_user(const freenect_nui_smoothing* P, joint_filter* J, freenect_nui_skeleton* s)
{
	float jitter = P->jitter_radius > 1e-4f ? P->jitter_radius : 1e-4f;
	int j, c;
	for (j = 0; j < FREENECT_NUI_JOINTS; j++) {
		joint_filter* F = &J[j];
		float raw[4], in[4], filtered[4], trend[4], pred[4], jr = jitter, md = P->max_deviation_radius, d;
		raw[0] = s->joints[j].x; raw[1] = s->joints[j].y; raw[2] = s->joints[j].z; raw[3] = 1.0f;
		if (s->joint_state[j] == FREENECT_NUI_JOINT_INFERRED) {
			jr *= 2.0f;
			md *= 2.0f;
		}
		if (raw[0] == 0.0f && raw[1] == 0.0f && raw[2] == 0.0f)
			F->frames = 0;
		if (F->frames == 0) {
			memcpy(filtered, raw, sizeof(raw));
			memset(trend, 0, sizeof(trend));
			F->frames = 1;
		} else if (F->frames == 1) {
			for (c = 0; c < 4; c++) {
				filtered[c] = (raw[c] + F->raw_prev[c]) * 0.5f;
				trend[c] = P->correction * (filtered[c] - F->filtered[c]) + (1.0f - P->correction) * F->trend[c];
			}
			F->frames = 2;
		} else {
			d = len3(raw, F->filtered);
			for (c = 0; c < 4; c++)
				in[c] = d <= jr ? (d / jr) * raw[c] + (1.0f - d / jr) * F->filtered[c] : raw[c];
			for (c = 0; c < 4; c++) {
				filtered[c] = (1.0f - P->smoothing) * in[c] + P->smoothing * (F->filtered[c] + F->trend[c]);
				trend[c] = P->correction * (filtered[c] - F->filtered[c]) + (1.0f - P->correction) * F->trend[c];
			}
		}
		for (c = 0; c < 4; c++)
			pred[c] = filtered[c] + P->prediction * trend[c];
		d = len3(pred, raw);
		if (d > md)
			for (c = 0; c < 4; c++)
				pred[c] = pred[c] * (md / d) + raw[c] * (1.0f - md / d);
		memcpy(F->raw_prev, raw, sizeof(raw));
		memcpy(F->filtered, filtered, sizeof(filtered));
		memcpy(F->trend, trend, sizeof(trend));
		s->joints[j].x = pred[0];
		s->joints[j].y = pred[1];
		s->joints[j].z = pred[2];
	}
}

int freenect_nui_smooth(freenect_nui_smoother* s, freenect_nui_frame* frame, const freenect_nui_smoothing* params)
{
	static const freenect_nui_smoothing def = FREENECT_NUI_SMOOTHING_DEFAULT;
	int u;
	if (!s || !frame)
		return -1;
	if (!params)
		params = &def;
	for (u = 0; u < NU; u++) {
		freenect_nui_skeleton* S = &frame->users[u];
		if (S->state != FREENECT_NUI_USER_TRACKED || S->tracking_id != s->id[u]) {
			memset(s->j[u], 0, sizeof(s->j[u])); // a new user in this slot: no history
			s->id[u] = S->state == FREENECT_NUI_USER_TRACKED ? S->tracking_id : 0;
		}
		if (S->state != FREENECT_NUI_USER_TRACKED)
			continue;
		if (s->kind == F_ONE_EURO)
			one_euro_user(s, s->j[u], S);
		else if (s->kind == F_KALMAN)
			kalman_user(s, s->j[u], S);
		else
			filter_user(params, s->j[u], S);
	}
	return 0;
}

/* ------------------------------------------------------------ orientations */

typedef struct {
	v3 r[3]; // rows X, Y, Z
	int set;
} rot;

static rot axes(v3 X, v3 Y, v3 Z)
{
	rot m;
	m.r[0] = X;
	m.r[1] = Y;
	m.r[2] = Z;
	m.set = 1;
	return m;
}

#define N fn_nui_v3norm
#define C v3cross

static v3 P(const freenect_nui_skeleton* s, int j)
{
	return v3make(s->joints[j].x, s->joints[j].y, s->joints[j].z);
}

static void quat_from(const rot* m, float q[4])
{
	// FUN_1800a8120 with m0..m10 the row-major 4x4 entries
	float m0 = m->r[0].x, m1 = m->r[0].y, m2 = m->r[0].z;
	float m4 = m->r[1].x, m5 = m->r[1].y, m6 = m->r[1].z;
	float m8 = m->r[2].x, m9 = m->r[2].y, m10 = m->r[2].z;
	float t = m0 + m5 + m10, s, r;
	if (t > 0.0f) {
		s = 0.5f / sqrtf(t + 1.0f);
		q[0] = (m6 - m9) * s; q[1] = (m8 - m2) * s; q[2] = (m1 - m4) * s; q[3] = 0.25f / s;
	} else if (m0 > m5 && m0 > m10) {
		r = 2.0f * sqrtf(1.0f + m0 - m5 - m10);
		q[0] = 0.25f * r; q[1] = (m4 + m1) / r; q[2] = (m8 + m2) / r; q[3] = (m6 - m9) / r;
	} else if (m5 > m10) {
		r = 2.0f * sqrtf(1.0f + m5 - m0 - m10);
		q[0] = (m4 + m1) / r; q[1] = 0.25f * r; q[2] = (m9 + m6) / r; q[3] = (m8 - m2) / r;
	} else {
		r = 2.0f * sqrtf(1.0f + m10 - m0 - m5);
		q[0] = (m8 + m2) / r; q[1] = (m9 + m6) / r; q[2] = 0.25f * r; q[3] = (m1 - m4) / r;
	}
}

static int orthonormal(const rot* m)
{
	int a, b;
	for (a = 0; a < 3; a++)
		for (b = 0; b < 3; b++) {
			float d = v3dot(m->r[a], m->r[b]) - (a == b ? 1.0f : 0.0f);
			if (fabsf(d) > 1e-4f)
				return 0;
		}
	return 1;
}

static int nz(v3 a) { return a.x != 0.0f || a.y != 0.0f || a.z != 0.0f; }

int freenect_nui_bone_orientations(const freenect_nui_skeleton* s, freenect_nui_bone bones[FREENECT_NUI_BODY_JOINTS])
{
	rot abs_[NJ];
	const freenect_nui_joint_state* st;
	v3 Xh = v3make(0, 0, 0), Zh = v3make(0, 0, 0), Xs = v3make(0, 0, 0), Ys = v3make(0, 0, 0), Zs = v3make(0, 0, 0);
	int ok = 0, b;
	memset(abs_, 0, sizeof(abs_));
	for (b = 0; b < NJ; b++) {
		int a, c;
		bones[b].end_joint = b;
		bones[b].start_joint = fn_nui_parent[b];
		for (a = 0; a < 3; a++)
			for (c = 0; c < 3; c++)
				bones[b].hierarchical.matrix[a][c] = bones[b].absolute.matrix[a][c] = a == c ? 1.0f : 0.0f;
		bones[b].hierarchical.quaternion[0] = bones[b].hierarchical.quaternion[1] = bones[b].hierarchical.quaternion[2] = 0.0f;
		bones[b].hierarchical.quaternion[3] = 1.0f;
		bones[b].absolute.quaternion[0] = bones[b].absolute.quaternion[1] = bones[b].absolute.quaternion[2] = 0.0f;
		bones[b].absolute.quaternion[3] = 1.0f;
	}
	if (!s || s->state != FREENECT_NUI_USER_TRACKED)
		return -1;
	st = s->joint_state;
	if (!st[0] && !st[2])
		return -1;

	// HipFrame (bone 0, 12, 16)
	if (st[0] && st[12] && st[16]) {
		v3 X = N(v3sub(P(s, 12), P(s, 16)));
		if (nz(X)) {
			v3 up = N(v3sub(P(s, 0), v3scale(v3add(P(s, 12), P(s, 16)), 0.5f))), Z, Y;
			if (!nz(up))
				up = v3make(0, 1, 0);
			Z = N(C(X, up));
			Y = N(C(Z, X));
			abs_[0] = axes(X, Y, Z);
			Xh = X;
			Zh = Z;
			{
				v3 Yl = N(v3sub(P(s, 12), P(s, 0))), Yr = N(v3sub(P(s, 16), P(s, 0)));
				abs_[12] = axes(N(C(Yl, Zh)), Yl, Zh);
				abs_[16] = axes(N(C(Yr, Zh)), Yr, Zh);
			}
			// Spine (bone 1)
			if (st[1]) {
				v3 Ys1 = N(v3sub(P(s, 1), P(s, 0))), Xr = N(v3sub(P(s, 4), P(s, 8))), Z1 = N(C(Xr, Ys1));
				abs_[1] = axes(N(C(Ys1, Z1)), Ys1, Z1);
			}
			// ShoulderCenter (bone 2)
			if (st[8] && st[4] && st[1]) {
				v3 Xr = N(v3sub(P(s, 4), P(s, 8))), Y2 = N(v3sub(P(s, 2), P(s, 1))), Z2 = N(C(Xr, Y2)), X2 = N(C(Y2, Z2));
				abs_[2] = axes(X2, Y2, Z2);
				Xs = X2; Ys = Y2; Zs = Z2;
			}
			// legs: r = -Xh for both (§30.8.8)
			{
				int side;
				for (side = 0; side < 2; side++) {
					int H = side ? 16 : 12, K = H + 1, A = H + 2, F = H + 3;
					v3 r = v3scale(Xh, -1.0f), u, f, g, X13;
					if (!st[K])
						continue;
					u = N(v3sub(P(s, K), P(s, H)));
					f = N(v3sub(P(s, A), P(s, K)));
					g = N(v3sub(P(s, F), P(s, A)));
					if (st[K] != FREENECT_NUI_JOINT_TRACKED || !st[A] || v3dot(u, f) >= 0.972f) {
						v3 Aa = N(C(r, u)), X = N(C(u, Aa));
						abs_[K] = axes(X, u, N(C(X, u)));
					} else {
						v3 Aa = N(C(r, f)), B = N(C(f, Aa)), Z = N(C(B, u));
						abs_[K] = axes(N(C(u, Z)), u, Z);
					}
					X13 = abs_[K].r[0];
					if (st[A]) {
						if (v3dot(u, f) >= 0.972f) {
							v3 Z = N(C(X13, f));
							abs_[A] = axes(N(C(f, Z)), f, Z);
						} else {
							v3 Aa = N(C(r, f)), X = N(C(f, Aa));
							abs_[A] = axes(X, f, N(C(X, f)));
						}
						if (st[F]) {
							v3 Z = N(C(abs_[A].r[0], g));
							abs_[F] = axes(N(C(g, Z)), g, Z);
						}
					}
				}
			}
			ok = 1;
		}
	}
	if (!ok && st[2] && st[4] && st[8] && st[3]) {
		// SeatedFrame (bones 0 identity, 2)
		v3 X = N(v3sub(P(s, 4), P(s, 8))), mid = v3scale(v3add(P(s, 4), P(s, 8)), 0.5f), up, Z, Y;
		float a = fn_nui_v3len(v3sub(P(s, 3), P(s, 2))), bb = fn_nui_v3len(v3sub(P(s, 3), mid));
		if (a - bb > 0.001f) up = N(v3sub(mid, P(s, 2)));
		else if (bb - a > 0.001f) up = N(v3sub(P(s, 2), mid));
		else up = v3sub(P(s, 3), P(s, 2));
		Z = N(C(X, up));
		Y = N(C(Z, X));
		if (nz(X) && nz(Y) && nz(Z)) {
			abs_[2] = axes(X, Y, Z);
			Xs = X; Ys = Y; Zs = Z;
			ok = 1;
		}
	}
	if (!ok)
		return -1;
	// Shoulders (bones 4, 8)
	if (st[8] && st[4]) {
		v3 Y4 = N(v3sub(P(s, 4), P(s, 2))), Y8 = N(v3sub(P(s, 8), P(s, 2)));
		v3 X4 = N(C(Y4, Zs)), X8 = N(C(Y8, Zs));
		abs_[4] = axes(X4, Y4, N(C(X4, Y4)));
		abs_[8] = axes(X8, Y8, N(C(X8, Y8)));
	}
	// Head (bone 3)
	if (st[3]) {
		v3 Y = N(v3sub(P(s, 3), P(s, 2))), Z = N(C(Xs, Y));
		abs_[3] = axes(N(C(Y, Z)), Y, Z);
	}
	// Arms (§30.8.8): the right arm differs in the sideways test and the fallback cross product
	{
		int side;
		for (side = 0; side < 2; side++) {
			int S = side ? 8 : 4, E = S + 1, W = S + 2, H = S + 3;
			v3 u, f, h, Xside = side ? v3scale(Xs, -1.0f) : Xs, X5;
			float c;
			int bent;
			if (!st[E])
				continue;
			u = N(v3sub(P(s, E), P(s, S)));
			f = N(v3sub(P(s, W), P(s, E)));
			h = N(v3sub(P(s, H), P(s, W)));
			c = fabsf(v3dot(u, f));
			bent = c < 0.94f;
			if (bent) {
				v3 X = N(C(u, f));
				abs_[E] = axes(X, u, N(C(X, u)));
			} else if (v3dot(u, Xside) <= 0.972f && v3dot(u, Ys) <= 0.0f) {
				v3 Z = side ? N(C(Xside, u)) : N(C(u, Xs));
				abs_[E] = axes(N(C(u, Z)), u, Z);
			} else {
				v3 T = side ? N(C(Ys, u)) : N(C(u, Ys)), X = N(C(u, T));
				abs_[E] = axes(X, u, N(C(X, u)));
			}
			X5 = abs_[E].r[0];
			if (st[W]) {
				if (bent) {
					v3 X = N(C(u, f));
					abs_[W] = axes(X, f, N(C(X, f)));
				} else {
					v3 Z = N(C(X5, f));
					abs_[W] = axes(N(C(f, Z)), f, Z);
				}
				if (st[H]) {
					v3 A = N(C(h, abs_[W].r[2])), Z = N(C(A, h));
					abs_[H] = axes(N(C(h, Z)), h, Z);
				}
			}
		}
	}

	// store: orthonormal check, absolute, hierarchical = M transpose(parent absolute)
	for (b = 0; b < NJ; b++) {
		int p = fn_nui_parent[b], a, c;
		if (!abs_[b].set || !orthonormal(&abs_[b])) {
			if (b == 0)
				abs_[0] = axes(v3make(1, 0, 0), v3make(0, 1, 0), v3make(0, 0, 1));
			else
				abs_[b] = abs_[p]; // inherits the parent's absolute; hierarchical identity
			abs_[b].set = 0;
		}
		for (a = 0; a < 3; a++) {
			bones[b].absolute.matrix[a][0] = abs_[b].r[a].x;
			bones[b].absolute.matrix[a][1] = abs_[b].r[a].y;
			bones[b].absolute.matrix[a][2] = abs_[b].r[a].z;
		}
		quat_from(&abs_[b], bones[b].absolute.quaternion);
		if (b == 0 || !abs_[b].set) {
			if (b == 0)
				bones[0].hierarchical = bones[0].absolute;
			continue;
		}
		{
			rot hm;
			for (a = 0; a < 3; a++) {
				v3 r = abs_[b].r[a];
				// row a of M times transpose(parent): dot with each parent row
				hm.r[a] = v3make(v3dot(r, abs_[p].r[0]), v3dot(r, abs_[p].r[1]), v3dot(r, abs_[p].r[2]));
			}
			for (a = 0; a < 3; a++) {
				bones[b].hierarchical.matrix[a][0] = hm.r[a].x;
				bones[b].hierarchical.matrix[a][1] = hm.r[a].y;
				bones[b].hierarchical.matrix[a][2] = hm.r[a].z;
			}
			(void)c;
			quat_from(&hm, bones[b].hierarchical.quaternion);
		}
	}
	return 0;
}
