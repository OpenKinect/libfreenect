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
 * Audio DSP for the Kinect's microphone array, after the reference's §40.6:
 *
 *   per frame of 256 samples (16 ms):
 *     input, clamped to +-1 and scaled by 16384 (FUN_180021490)
 *     -> MCLT analysis of each microphone, with frame phase alignment (§40.6.7)
 *     -> direction estimate and tracker (§40.6.16) -> beam choice (§40.6.4)
 *     -> beamformer y = w^H x per subband (§40.6.3)
 *     -> x2 on all bins but DC (AGC off, FUN_180020360)
 *     -> MCLT synthesis -> y 2^-15 x2 -> int16 (FUN_180006750)
 *     -> optional SpeexDSP echo cancellation, noise suppression, AGC.
 *
 * The beam weights are designed here: for each beam and subband, the
 * minimum-variance distortionless response for diffuse noise (a
 * superdirective beam), with diagonal loading raised until the white noise
 * gain is at least FN_DSP_MIN_WNG, so that low frequencies do not amplify
 * microphone noise without bound; then faded in from 156 to 375 Hz and out
 * from 7156 to 7469 Hz. The steering vectors model a point source at 1.5 m.
 *
 * How these choices compare with Microsoft's own weights (measured against
 * the tables in KinectAudio10.dll, which are not used here): those are
 * distortionless towards a source about 1.5 m away, fade in and out at the
 * same frequencies, and keep the white noise gain high rather than push the
 * directivity; they are not a closed-form design (each beam trades the two
 * differently). With the +1.8 dB floor, the beams here have the same mean
 * directivity (5.1 dB for both, 500-4000 Hz, spherically diffuse noise) and
 * a higher worst-case white noise gain (+1.8 dB against -7.8 dB).
 *
 * Direction estimate: the runtime squares phase errors wrapped to [0, 2 pi),
 * which biases every estimate the same way (+4..+7 degrees in simulation);
 * here they are wrapped to (-pi, pi].
 */

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS // fopen, getenv
#endif
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libfreenect_audio_dsp.h"
#include "mclt.h"

#ifdef HAVE_SPEEXDSP
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NB FREENECT_DSP_NUM_BEAMS
#define NK FN_MCLT_M
#define MAXM 4

#define FN_DSP_C 343.0            // speed of sound for the beams (§40.6.3)
#define FN_DSP_BEAM_RANGE 1.5     // m: what the runtime's steering vectors fit
#ifndef FN_DSP_MIN_WNG
#define FN_DSP_MIN_WNG 1.5        // +1.8 dB white noise gain at worst (see the note at the top)
#endif

// Direction estimator (§40.6.16)
#define DOA_C 342.0
#define DOA_RANGE 1.5
#define DOA_NGRID 18
#define DOA_K0 16                 // 500 Hz
#define DOA_K1 112                // 3500 Hz
#define DOA_NK (DOA_K1 - DOA_K0 + 1)
#define DOA_FRAME_S 0.016
#define TRK_RECORDS 40
#define TRK_WINDOW_S 2.5
#define TRK_UPDATE_S 0.2

// Beam choice (§40.6.4)
#define SEL_MIN_CONF 0.43195
#define SEL_HYST 0.66

typedef struct {
	double re, im;
} dc;

static dc dc_mul(dc a, dc b) { dc r; r.re = a.re * b.re - a.im * b.im; r.im = a.re * b.im + a.im * b.re; return r; }
static dc dc_conj(dc a) { a.im = -a.im; return a; }
static dc dc_scale(dc a, double s) { a.re *= s; a.im *= s; return a; }
static dc dc_add(dc a, dc b) { a.re += b.re; a.im += b.im; return a; }
static dc dc_sub(dc a, dc b) { a.re -= b.re; a.im -= b.im; return a; }
static dc dc_div(dc a, dc b)
{
	double d = b.re * b.re + b.im * b.im;
	dc r;
	r.re = (a.re * b.re + a.im * b.im) / d;
	r.im = (a.im * b.re - a.re * b.im) / d;
	return r;
}
static dc dc_exp_j(double phi) { dc r; r.re = cos(phi); r.im = sin(phi); return r; }

typedef struct {
	double t, theta, c;
} doa_record;

struct freenect_dsp {
	int nm;
	double mx[MAXM], my[MAXM], mz[MAXM];
	int mtype[MAXM];

	fn_mclt_channel in[MAXM], out;
	float hop[MAXM][NK];
	int fill;
	int16_t fifo[2048];
	int fifo_r, fifo_n;

	double beam_angle[NB];
	fn_cplx W[NB][NK][MAXM];      // in use: y = sum conj(W) x
	fn_cplx Wdesign[NB][NK][MAXM];
	int using_sdk;
	fn_cplx Wad[NK][MAXM];        // adaptive beam
	double ad_angle;
	int ad_valid;

	freenect_dsp_beam_mode mode;
	int beam;

	// direction estimate
	float ssM[MAXM][NK], ssV[MAXM][NK];
	int ss_init;
	double floor_F;
	int speech;
	float dphi[MAXM - 1][DOA_NGRID][DOA_NK];
	float bandw[NK];
	doa_record rec[TRK_RECORDS];
	int nrec, rec_head;
	double last_track_t;
	double src_angle, src_conf;
	int have_src;
	uint64_t frame;

	int ns, agc, aec;
#ifdef HAVE_SPEEXDSP
	SpeexPreprocessState* pp;
	SpeexEchoState* echo;
	int16_t ref[4096];
	int ref_n;
#endif
};

/* ------------------------------------------------------------ beam design */

// Point source at r metres in direction theta (x forward, y along the
// array): d_m = e^{-j 2 pi f r_m / c} r / r_m.
static void steering(const freenect_dsp* d, double theta, double r, double f, double c, dc* v)
{
	double xs = r * cos(theta), ys = r * sin(theta);
	int m;
	for (m = 0; m < d->nm; m++) {
		double dx = xs - d->mx[m], dy = ys - d->my[m], dz = -d->mz[m];
		double rm = sqrt(dx * dx + dy * dy + dz * dz);
		v[m] = dc_scale(dc_exp_j(-2.0 * M_PI * f * rm / c), r / rm);
	}
}

// Solves A x = b for a 4x4 (or smaller) complex system; A is overwritten.
static int solve(dc A[MAXM][MAXM], dc* b, int n)
{
	int i, j, k;
	for (i = 0; i < n; i++) {
		int p = i;
		double best = 0.0;
		for (k = i; k < n; k++) {
			double a = A[k][i].re * A[k][i].re + A[k][i].im * A[k][i].im;
			if (a > best) { best = a; p = k; }
		}
		if (best == 0.0)
			return -1;
		if (p != i) {
			dc t;
			for (j = 0; j < n; j++) { t = A[i][j]; A[i][j] = A[p][j]; A[p][j] = t; }
			t = b[i]; b[i] = b[p]; b[p] = t;
		}
		for (k = i + 1; k < n; k++) {
			dc f = dc_div(A[k][i], A[i][i]);
			for (j = i; j < n; j++)
				A[k][j] = dc_sub(A[k][j], dc_mul(f, A[i][j]));
			b[k] = dc_sub(b[k], dc_mul(f, b[i]));
		}
	}
	for (i = n - 1; i >= 0; i--) {
		dc s = b[i];
		for (j = i + 1; j < n; j++)
			s = dc_sub(s, dc_mul(A[i][j], b[j]));
		b[i] = dc_div(s, A[i][i]);
	}
	return 0;
}

static double sinc(double x) { return fabs(x) < 1e-12 ? 1.0 : sin(x) / x; }

// MVDR weights against diffuse noise for one subband: w = R^-1 v / (v^H R^-1 v),
// R = Gamma + mu I, mu raised until the white noise gain |w^H v|^2 / w^H w
// reaches FN_DSP_MIN_WNG.
static void design_bin(const freenect_dsp* d, double theta, double f, fn_cplx* w)
{
	dc v[MAXM], x[MAXM], A[MAXM][MAXM];
	double mu = 1e-4;
	int n = d->nm, i, j, it;
	steering(d, theta, FN_DSP_BEAM_RANGE, f, FN_DSP_C, v);
	for (it = 0; it < 60; it++, mu *= 1.5) {
		dc den = { 0.0, 0.0 };
		double ww = 0.0;
		for (i = 0; i < n; i++) {
			for (j = 0; j < n; j++) {
				double dx = d->mx[i] - d->mx[j], dy = d->my[i] - d->my[j], dz = d->mz[i] - d->mz[j];
				double dist = sqrt(dx * dx + dy * dy + dz * dz);
				A[i][j].re = sinc(2.0 * M_PI * f * dist / FN_DSP_C) + (i == j ? mu : 0.0);
				A[i][j].im = 0.0;
			}
			x[i] = v[i];
		}
		if (solve(A, x, n) < 0)
			continue;
		for (i = 0; i < n; i++)
			den = dc_add(den, dc_mul(dc_conj(v[i]), x[i]));
		for (i = 0; i < n; i++) {
			x[i] = dc_div(x[i], den); // w = x / (v^H x), so that w^H v = 1 (den is real)
			ww += x[i].re * x[i].re + x[i].im * x[i].im;
		}
		if (1.0 / ww >= FN_DSP_MIN_WNG || it == 59) {
			for (i = 0; i < n; i++) {
				w[i].re = (float)x[i].re;
				w[i].im = (float)x[i].im;
			}
			return;
		}
	}
}

// Raised-cosine fade at the ends of the band: 0 below 156.25 Hz, 1 from
// 375 Hz to 7156.25 Hz, 0 above 7468.75 Hz.
static double band_gain(double f)
{
	if (f <= 156.25 || f >= 7468.75)
		return 0.0;
	if (f < 375.0)
		return 0.5 - 0.5 * cos(M_PI * (f - 156.25) / 218.75);
	if (f > 7156.25)
		return 0.5 + 0.5 * cos(M_PI * (f - 7156.25) / 312.5);
	return 1.0;
}

static void design_beam(const freenect_dsp* d, double theta, fn_cplx W[NK][MAXM])
{
	int k, m;
	for (k = 0; k < NK; k++) {
		double f = (k + 0.5) * 31.25, g = band_gain(f);
		design_bin(d, theta, f, W[k]);
		for (m = 0; m < d->nm; m++) {
			W[k][m].re = (float)(W[k][m].re * g);
			W[k][m].im = (float)(W[k][m].im * g);
		}
	}
}

/* ------------------------------------------------- direction estimate */

// First-order gradient capsule, =1 on axis at 1 kHz (§40.6.18, micdir.py).
static dc gradient_P(double f, double cospsi)
{
	const double TWO_PI = 6.283185307179586, C = 1e-09, R = 10000000.0, TAU = 1.4619883040935671e-05;
	const double PHI0 = 0.09185943431549101;
	double X = f >= 1e-10 ? 1.0 / (f * TWO_PI * C) : 1e-10;
	dc A, B, D, Cc, nrm;
	A.re = X * X / (X * X + R * R);
	A.im = X * R / (X * X + R * R);
	nrm.re = 0.00025323881296515996;
	nrm.im = 1591549430918.9534 * 9.99746761187035e-15;
	B = dc_exp_j(-PHI0);
	B.re = 1.0 - B.re; B.im = -B.im;
	D = dc_mul(B, nrm);
	Cc = dc_exp_j(-f * TWO_PI * TAU * cospsi);
	Cc.re = 1.0 - Cc.re; Cc.im = -Cc.im;
	return dc_div(dc_mul(A, Cc), D);
}

static dc directivity(int type, double f, double cospsi)
{
	static const double a0[5] = { 1.0, 0.7, 0.5, 0.37, 0.25 }, a1[5] = { 0.0, 0.3, 0.5, 0.63, 0.75 };
	dc p, r;
	if (type == 5)
		return gradient_P(f, cospsi);
	if (type < 0 || type > 4) {
		r.re = 1.0; r.im = 0.0;
		return r;
	}
	p = gradient_P(f, cospsi);
	r.re = a0[type] + a1[type] * p.re;
	r.im = a1[type] * p.im;
	return r;
}

static double wrap2pi(double a)
{
	a = fmod(a, 2.0 * M_PI);
	return a < 0.0 ? a + 2.0 * M_PI : a;
}

static void doa_tables(freenect_dsp* d)
{
	int i, k, m;
	for (i = 0; i < DOA_NGRID; i++) {
		double th = -M_PI / 2 + i * M_PI / 17.0;
		double xs = DOA_RANGE * cos(th), ys = DOA_RANGE * sin(th);
		for (k = DOA_K0; k <= DOA_K1; k++) {
			double f = k * 31.25;
			double ph[MAXM];
			for (m = 0; m < d->nm; m++) {
				double dx = xs - d->mx[m], dy = ys - d->my[m], dz = -d->mz[m];
				double rho = sqrt(dx * dx + dy * dy), rm = sqrt(rho * rho + dz * dz);
				double elev = atan2(dz, rho), az = atan2(dy, dx);
				dc D = directivity(d->mtype[m] == 2 ? 2 : d->mtype[m], f, cos(elev) * cos(az));
				dc H = dc_mul(D, dc_scale(dc_exp_j(-2.0 * M_PI * f * rm / DOA_C), 1.0 / rm));
				ph[m] = atan2(H.im, H.re);
			}
			for (m = 0; m < d->nm - 1; m++)
				d->dphi[m][i][k - DOA_K0] = (float)wrap2pi(ph[0] - ph[m + 1]);
		}
	}
	// band weighting: 0 below 500 Hz, up to 1 at 1 kHz, 1 to 2 kHz, down to 0 at 3.5 kHz
	for (k = 0; k < NK; k++) {
		double f = k * 31.25, w;
		if (f < 500.0 || f > 3500.0) w = 0.0;
		else if (f < 1000.0) w = 0.5 - 0.5 * cos(M_PI * (f - 500.0) / 500.0);
		else if (f <= 2000.0) w = 1.0;
		else w = 0.5 + 0.5 * cos(M_PI * (f - 2000.0) / 1500.0);
		d->bandw[k] = (float)w;
	}
}

static void track(freenect_dsp* d, double t)
{
	double sum[36], mean[36], sd[36];
	int cnt[36], b, i, best = -1;
	double t_new = -1e30;
	// drop old records
	{
		doa_record keep[TRK_RECORDS];
		int n = 0;
		for (i = 0; i < d->nrec; i++) {
			doa_record* r = &d->rec[(d->rec_head + i) % TRK_RECORDS];
			if (t - r->t <= TRK_WINDOW_S)
				keep[n++] = *r;
		}
		memcpy(d->rec, keep, sizeof(doa_record) * (size_t)n);
		d->nrec = n;
		d->rec_head = 0;
	}
	if (d->nrec == 0) {
		d->src_conf = 0.0;
		return;
	}
	for (b = 0; b < 36; b++) {
		double lo = (-180.0 + 10.0 * b) * M_PI / 180.0, hi = lo + 20.0 * M_PI / 180.0;
		int pass;
		mean[b] = sd[b] = sum[b] = 0.0;
		cnt[b] = 0;
		for (pass = 0; pass < 2; pass++) {
			double s = 0.0, sw = 0.0, sq = 0.0, m0 = mean[b], s0 = sd[b];
			int c = 0;
			for (i = 0; i < d->nrec; i++) {
				const doa_record* r = &d->rec[i];
				if (r->theta < lo || r->theta >= hi)
					continue;
				if (pass == 1 && fabs(r->theta - m0) > 2.0 * s0)
					continue;
				sw += r->c;
				s += r->c * r->theta;
				c++;
			}
			if (c == 0 || sw <= 0.0)
				break;
			mean[b] = s / sw;
			for (i = 0; i < d->nrec; i++) {
				const doa_record* r = &d->rec[i];
				if (r->theta < lo || r->theta >= hi)
					continue;
				if (pass == 1 && fabs(r->theta - m0) > 2.0 * s0)
					continue;
				sq += r->c * (r->theta - mean[b]) * (r->theta - mean[b]);
			}
			sd[b] = sqrt(sq / sw);
			sum[b] = sw;
			cnt[b] = c;
			if (c < 11) // outlier rejection only with enough records
				break;
		}
		if (cnt[b] > 0 && (best < 0 || sum[b] > sum[best]))
			best = b;
	}
	for (i = 0; i < d->nrec; i++)
		if (d->rec[i].t > t_new)
			t_new = d->rec[i].t;
	if (best < 0) {
		d->src_conf = 0.0;
		return;
	}
	{
		double c1 = cnt[best] < 1.5 ? cnt[best] * 0.6667 : 1.0;
		double c2 = sd[best] > 0.0 ? fmin(1.0, 0.032656 / sd[best]) : 1.0;
		double c3 = fmin(1.0, 2.0 * (TRK_WINDOW_S - (t - t_new)) / TRK_WINDOW_S);
		d->src_angle = mean[best];
		d->src_conf = c1 * c2 * c3;
		if (d->src_conf < 0.0)
			d->src_conf = 0.0;
		d->have_src = 1;
	}
}

static void doa_frame(freenect_dsp* d, fn_cplx X[MAXM][NK])
{
	static const double alpha = 0.016;
	float Y[MAXM][NK][2];
	double E = 0.0, t = d->frame * DOA_FRAME_S, F = d->floor_F;
	int m, k, i, pushed = 0;

	// band weighting and per-microphone spectral subtraction
	for (m = 0; m < d->nm; m++) {
		double acc = 0.0;
		for (k = DOA_K0; k <= DOA_K1; k++) {
			double re = X[m][k].re * d->bandw[k], im = X[m][k].im * d->bandw[k];
			double a = sqrt(re * re + im * im), dev, w, g;
			if (!d->ss_init) {
				d->ssM[m][k] = (float)a;
				d->ssV[m][k] = (float)(a * a > 1.0 ? a * a : 1.0);
			}
			dev = a - d->ssM[m][k];
			w = exp(-0.25 * dev * dev / d->ssV[m][k]);
			if (w * alpha < 1.0) {
				d->ssM[m][k] += (float)(dev * w * alpha);
				d->ssV[m][k] = (float)((1.0 - alpha) * d->ssV[m][k] + alpha * dev * dev);
				if (d->ssV[m][k] < 1e-6f)
					d->ssV[m][k] = 1e-6f;
			}
			g = a <= d->ssM[m][k] ? 0.1 : (a * a - 0.9 * d->ssM[m][k] * d->ssM[m][k]) / (a * a);
			Y[m][k][0] = (float)(re * g);
			Y[m][k][1] = (float)(im * g);
			acc += (re * g) * (re * g) + (im * g) * (im * g);
		}
		E += sqrt(acc / DOA_NK);
	}
	d->ss_init = 1;
	E /= d->nm;

	// noise floor: rises slowly (20 s), falls fast (40 ms), never above E
	if (E > F)
		d->floor_F = F + (E - F) * (DOA_FRAME_S / 20.0);
	else
		d->floor_F = F + (E - F) * fmin(1.0, DOA_FRAME_S / 0.04);
	if (d->floor_F > E)
		d->floor_F = E;
	if (E > 3.5 * F) d->speech = 1;
	else if (E < 1.5 * F) d->speech = 0;

	if (E > 5.29079 * F && E > 82.30574) {
		double hist[DOA_NGRID];
		memset(hist, 0, sizeof(hist));
		for (k = DOA_K0; k <= DOA_K1; k++) {
			double A = 0.0, phi[MAXM - 1], best = 1e300;
			int bi = 0;
			double a0 = atan2(Y[0][k][1], Y[0][k][0]);
			A = hypot(Y[0][k][0], Y[0][k][1]);
			for (m = 0; m < d->nm - 1; m++) {
				A += hypot(Y[m][k][0], Y[m][k][1]);  // as coded: mics 0..M-2
				phi[m] = wrap2pi(a0 - atan2(Y[m + 1][k][1], Y[m + 1][k][0]));
			}
			for (i = 0; i < DOA_NGRID; i++) {
				double s = 0.0;
				for (m = 0; m < d->nm - 1; m++) {
					double e = wrap2pi(phi[m] - d->dphi[m][i][k - DOA_K0]);
					if (e > M_PI)
						e -= 2.0 * M_PI; // symmetric, see the note at the top
					s += e * e;
				}
				if (s < best) { best = s; bi = i; }
			}
			hist[bi] += A / d->nm;
		}
		{
			int im = 0;
			double theta, c;
			for (i = 1; i < DOA_NGRID; i++)
				if (hist[i] > hist[im])
					im = i;
			theta = -M_PI / 2 + im * M_PI / 17.0;
			if (im >= 1 && im <= 16) {
				double den = hist[im - 1] - 2.0 * hist[im] + hist[im + 1];
				if (den < 0.0)
					theta += 0.5 * (hist[im - 1] - hist[im + 1]) / den * (M_PI / 17.0);
			}
			c = (hist[im] / DOA_NK) * (1.0 / 18.0);
			if (fabs(theta) <= 50.0 * M_PI / 180.0 + 1e-9 && c > 0.2716) {
				doa_record* r;
				if (d->nrec == TRK_RECORDS) {
					d->rec_head = (d->rec_head + 1) % TRK_RECORDS;
					d->nrec--;
				}
				r = &d->rec[(d->rec_head + d->nrec) % TRK_RECORDS];
				r->t = t;
				r->theta = theta;
				r->c = c < 5.0 ? c : 5.0;
				d->nrec++;
				pushed = 1;
			}
		}
	}
	if (pushed || t - d->last_track_t >= TRK_UPDATE_S) {
		track(d, t);
		d->last_track_t = t;
	}
}

/* ------------------------------------------------------------ beam choice */

static int nearest_beam(const freenect_dsp* d, double a)
{
	int i, b = 0;
	for (i = 1; i < NB; i++)
		if (fabs(d->beam_angle[i] - a) < fabs(d->beam_angle[b] - a))
			b = i;
	return b;
}

static void choose_beam(freenect_dsp* d)
{
	if (d->mode == FREENECT_DSP_BEAM_MANUAL || !d->have_src || d->src_conf <= SEL_MIN_CONF)
		return;
	if (d->mode == FREENECT_DSP_BEAM_ADAPTIVE) {
		if (!d->ad_valid || fabs(d->src_angle - d->ad_angle) > 2.0 * M_PI / 180.0) {
			design_beam(d, d->src_angle, d->Wad);
			d->ad_angle = d->src_angle;
			d->ad_valid = 1;
		}
		return;
	}
	{
		int b = nearest_beam(d, d->src_angle);
		double spacing = d->beam_angle[1] - d->beam_angle[0];
		if (abs(b - d->beam) < 2) {
			if (fabs(d->beam_angle[d->beam] - d->src_angle) > SEL_HYST * spacing)
				d->beam = b;
		} else {
			d->beam = b;
		}
	}
}

/* ------------------------------------------------------------ processing */

static void process_frame(freenect_dsp* d)
{
	fn_cplx X[MAXM][NK], Yb[NK];
	float y[NK];
	int16_t o[NK];
	int m, k, i;
	const fn_cplx (*W)[MAXM];

	for (m = 0; m < d->nm; m++)
		fn_mclt_analysis(&d->in[m], d->hop[m], X[m]);
	doa_frame(d, X);
	choose_beam(d);

	W = (d->mode == FREENECT_DSP_BEAM_ADAPTIVE && d->ad_valid) ? (const fn_cplx (*)[MAXM])d->Wad
	                                                            : (const fn_cplx (*)[MAXM])d->W[d->beam];
	for (k = 0; k < NK; k++) {
		float re = 0.0f, im = 0.0f;
		for (m = 0; m < d->nm; m++) {
			// conj(w) x
			re += W[k][m].re * X[m][k].re + W[k][m].im * X[m][k].im;
			im += W[k][m].re * X[m][k].im - W[k][m].im * X[m][k].re;
		}
		// x2 (AGC off) on every bin but DC
		Yb[k].re = k ? 2.0f * re : re;
		Yb[k].im = k ? 2.0f * im : im;
	}
	fn_mclt_synthesis(&d->out, Yb, y);
	for (i = 0; i < NK; i++) {
		float v = y[i] * (1.0f / 32768.0f) * 2.0f;
		o[i] = v >= 1.0f ? 32767 : v <= -1.0f ? -32768 : (int16_t)(int)(v * 32767.0f);
	}
#ifdef HAVE_SPEEXDSP
	if (d->aec && d->echo) {
		int16_t ref[NK], e[NK];
		int n = d->ref_n < NK ? d->ref_n : NK;
		memset(ref, 0, sizeof(ref));
		memcpy(ref, d->ref, sizeof(int16_t) * (size_t)n);
		memmove(d->ref, d->ref + n, sizeof(int16_t) * (size_t)(d->ref_n - n));
		d->ref_n -= n;
		speex_echo_cancellation(d->echo, o, ref, e);
		memcpy(o, e, sizeof(o));
	}
	if (d->pp && (d->ns || d->agc || d->aec))
		speex_preprocess_run(d->pp, o);
#endif
	for (i = 0; i < NK; i++) {
		if (d->fifo_n < (int)(sizeof(d->fifo) / sizeof(d->fifo[0]))) {
			d->fifo[(d->fifo_r + d->fifo_n) % 2048] = o[i];
			d->fifo_n++;
		}
	}
	d->frame++;
}

// The common path: sample i of microphone m as a float, full scale 1.
typedef float (*sample_fn)(const void* src, int i, int m);

static int run(freenect_dsp* d, const void* src, int n, int16_t* out, sample_fn get)
{
	int i, m;
	if (!d || !src || !out || n < 0)
		return -1;
	for (i = 0; i < n; i++) {
		for (m = 0; m < d->nm; m++) {
			float v = get(src, i, m);
			v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
			d->hop[m][d->fill] = v * 16384.0f;
		}
		if (++d->fill == NK) {
			d->fill = 0;
			process_frame(d);
		}
		out[i] = d->fifo[d->fifo_r];
		d->fifo_r = (d->fifo_r + 1) % 2048;
		d->fifo_n--;
	}
	return n;
}

static float get_s32(const void* src, int i, int m) { return (float)((const int32_t*)src)[4 * i + m] * (1.0f / 2147483648.0f); }
static float get_f32(const void* src, int i, int m) { return ((const float*)src)[4 * i + m]; }
static float get_planar(const void* src, int i, int m) { return (float)((const int32_t* const*)src)[m][i] * (1.0f / 2147483648.0f); }

int freenect_dsp_process(freenect_dsp* d, const int32_t* frames, int n, int16_t* out)
{
	return run(d, frames, n, out, get_s32);
}

int freenect_dsp_process_float(freenect_dsp* d, const float* frames, int n, int16_t* out)
{
	return run(d, frames, n, out, get_f32);
}

int freenect_dsp_process_planar(freenect_dsp* d, const int32_t* mic1, const int32_t* mic2,
	const int32_t* mic3, const int32_t* mic4, int n, int16_t* out)
{
	const int32_t* p[4];
	p[0] = mic1; p[1] = mic2; p[2] = mic3; p[3] = mic4;
	if (!mic1 || !mic2 || !mic3 || !mic4)
		return -1;
	return run(d, p, n, out, get_planar);
}

/* ------------------------------------------------------------ setup */

freenect_dsp* freenect_dsp_create(const freenect_mic_geometry* g)
{
	freenect_mic_geometry def;
	freenect_dsp* d;
	int m, b;
	if (!g) {
		static const int16_t y[4] = { -113, 36, 76, 113 };
		memset(&def, 0, sizeof(def));
		def.num_mics = 4;
		for (m = 0; m < 4; m++) {
			def.mics[m].type = 2;
			def.mics[m].y_mm = y[m];
		}
		g = &def;
	}
	if (g->num_mics < 2 || g->num_mics > MAXM)
		return NULL;
	d = (freenect_dsp*)calloc(1, sizeof(*d));
	if (!d)
		return NULL;
	d->nm = g->num_mics;
	for (m = 0; m < d->nm; m++) {
		d->mx[m] = g->mics[m].x_mm * 0.001;
		d->my[m] = g->mics[m].y_mm * 0.001;
		d->mz[m] = g->mics[m].z_mm * 0.001;
		d->mtype[m] = g->mics[m].type;
	}
	for (m = 0; m < MAXM; m++)
		fn_mclt_channel_reset(&d->in[m], 1);
	fn_mclt_channel_reset(&d->out, 1);
	for (b = 0; b < NB; b++) {
		d->beam_angle[b] = (-50.0 + 10.0 * b) * M_PI / 180.0;
		design_beam(d, d->beam_angle[b], d->Wdesign[b]);
	}
	memcpy(d->W, d->Wdesign, sizeof(d->W));
	d->beam = NB / 2;
	d->mode = FREENECT_DSP_BEAM_AUTOMATIC;
	d->floor_F = 30000.0;
	doa_tables(d);
	// FREENECT_DSP_LATENCY: 256 samples of output buffer (+ 256 in the MCLT)
	d->fifo_n = NK;
	return d;
}

void freenect_dsp_destroy(freenect_dsp* d)
{
	if (!d)
		return;
#ifdef HAVE_SPEEXDSP
	if (d->pp)
		speex_preprocess_state_destroy(d->pp);
	if (d->echo)
		speex_echo_state_destroy(d->echo);
#endif
	free(d);
}

int freenect_dsp_set_beam_mode(freenect_dsp* d, freenect_dsp_beam_mode mode)
{
	if (!d || (mode != FREENECT_DSP_BEAM_AUTOMATIC && mode != FREENECT_DSP_BEAM_ADAPTIVE && mode != FREENECT_DSP_BEAM_MANUAL))
		return -1;
	d->mode = mode;
	d->ad_valid = 0;
	return 0;
}

int freenect_dsp_set_beam(freenect_dsp* d, int beam)
{
	if (!d || beam < 0 || beam >= NB)
		return -1;
	d->beam = beam;
	return 0;
}

double freenect_dsp_get_beam_angle(freenect_dsp* d)
{
	if (!d)
		return 0.0;
	if (d->mode == FREENECT_DSP_BEAM_ADAPTIVE && d->ad_valid)
		return d->ad_angle;
	return d->beam_angle[d->beam];
}

int freenect_dsp_get_source_angle(freenect_dsp* d, double* angle, double* confidence)
{
	if (!d || !d->have_src)
		return -1;
	if (angle)
		*angle = d->src_angle;
	if (confidence)
		*confidence = d->src_conf;
	return 0;
}

/* ------------------------------------------------ Microsoft's weights */

/*
 * The weights file (written by freenect-driver-install / fwfetcher.py from
 * the user's own copy of KinectAudio10.dll, §40.6.3), little endian:
 *    0  "FNBW"
 *    4  u32 version 1
 *    8  u32 microphones (4), u32 frequencies (256), u32 beams (11), u32 0
 *   24  beams x f64 azimuth (rad)
 *  112  weights   complex64 [beam][frequency][mic]
 *       steering  complex64 [beam][frequency][mic]
 *  end  u32 CRC-32 of everything before it
 * Frequency f of the tables is f x 31.25 Hz.
 */
#define BW_HEADER 24
#define BW_TABLE (NB * NK * 4 * 8)
#define BW_SIZE (BW_HEADER + NB * 8 + 2 * BW_TABLE + 4)

static uint32_t crc32_of(const uint8_t* p, size_t n)
{
	uint32_t c = 0xFFFFFFFFu;
	size_t i;
	int k;
	for (i = 0; i < n; i++) {
		c ^= p[i];
		for (k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
	}
	return ~c;
}

static uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static float lef(const uint8_t* p) { uint32_t u = le32(p); float f; memcpy(&f, &u, 4); return f; }
static double led(const uint8_t* p)
{
	uint64_t u = (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32);
	double v;
	memcpy(&v, &u, 8);
	return v;
}

static FILE* open_weights(const char* path)
{
	static const char* name = "kinect_beam_weights.bin";
	char buf[1024];
	const char* e;
	FILE* f;
	if (path)
		return fopen(path, "rb");
#define TRY(dir, sub) do { if (dir) { snprintf(buf, sizeof(buf), "%s%s/%s", dir, sub, name); if ((f = fopen(buf, "rb")) != NULL) return f; } } while (0)
	if ((e = getenv("LIBFREENECT_AUDIO_WEIGHTS")) != NULL)
		return fopen(e, "rb");
	TRY(getenv("LIBFREENECT_FIRMWARE_PATH"), "");
#ifdef _WIN32
	TRY(getenv("LOCALAPPDATA"), "/libfreenect");
	TRY(getenv("ProgramData"), "/libfreenect");
#else
	if ((e = getenv("XDG_DATA_HOME")) != NULL && e[0] == '/')
		TRY(e, "/libfreenect");
	else
		TRY(getenv("HOME"), "/.local/share/libfreenect");
#ifdef __APPLE__
	TRY(getenv("HOME"), "/Library/Application Support/libfreenect");
#endif
	TRY("/usr/local/share/libfreenect", "");
	TRY("/usr/share/libfreenect", "");
#endif
#undef TRY
	return NULL;
}

int freenect_dsp_load_sdk_weights(freenect_dsp* d, const char* path)
{
	static const double kinect_y[4] = { -0.113, 0.036, 0.076, 0.113 };
	uint8_t* buf;
	FILE* f;
	size_t n;
	int b, k, m, flip_votes = 0, keep_votes = 0;
	const uint8_t *wt, *st;
	if (!d)
		return -1;
	// only for the array the weights were made for
	if (d->nm != 4)
		return -EINVAL;
	for (m = 0; m < 4; m++)
		if (fabs(d->my[m] - kinect_y[m]) > 0.002 || fabs(d->mx[m]) > 0.002 || fabs(d->mz[m]) > 0.002)
			return -EINVAL;
	f = open_weights(path);
	if (!f)
		return -ENOENT;
	buf = (uint8_t*)malloc(BW_SIZE + 1);
	if (!buf) {
		fclose(f);
		return -ENOMEM;
	}
	n = fread(buf, 1, BW_SIZE + 1, f);
	fclose(f);
	if (n != BW_SIZE || memcmp(buf, "FNBW", 4) != 0 || le32(buf + 4) != 1 || le32(buf + 8) != 4 ||
	    le32(buf + 12) != NK || le32(buf + 16) != NB || crc32_of(buf, BW_SIZE - 4) != le32(buf + BW_SIZE - 4)) {
		free(buf);
		return -EINVAL;
	}
	for (b = 0; b < NB; b++)
		if (fabs(led(buf + BW_HEADER + 8 * b) - d->beam_angle[b]) > 0.01) {
			free(buf);
			return -EINVAL;
		}
	wt = buf + BW_HEADER + NB * 8;
	st = wt + BW_TABLE;
#define W_AT(t, b, k, m) ((t) + 8 * (((size_t)(b) * NK + (k)) * 4 + (m)))
	// The runtime's conjugation convention is not known (§40.6.3): use the
	// one under which each beam passes its own steering vector unchanged.
	for (b = 0; b < NB; b++)
		for (k = 8; k < 200; k += 16) {
			double r1re = 0, r1im = 0, r2re = 0, r2im = 0;
			for (m = 0; m < 4; m++) {
				double wr = lef(W_AT(wt, b, k, m)), wi = lef(W_AT(wt, b, k, m) + 4);
				double sr = lef(W_AT(st, b, k, m)), si = lef(W_AT(st, b, k, m) + 4);
				r1re += wr * sr + wi * si; r1im += wr * si - wi * sr;   // conj(w) s
				r2re += wr * sr - wi * si; r2im += wr * si + wi * sr;   // w s
			}
			if (hypot(r1re - 1.0, r1im) <= hypot(r2re - 1.0, r2im)) keep_votes++;
			else flip_votes++;
		}
	// onto the MCLT's subband centres (k + 1/2) x 31.25 Hz: the mean of the
	// table's frequencies k and k + 1
	for (b = 0; b < NB; b++)
		for (k = 0; k < NK; k++)
			for (m = 0; m < 4; m++) {
				int k1 = k + 1 < NK ? k + 1 : k;
				float re = 0.5f * (lef(W_AT(wt, b, k, m)) + lef(W_AT(wt, b, k1, m)));
				float im = 0.5f * (lef(W_AT(wt, b, k, m) + 4) + lef(W_AT(wt, b, k1, m) + 4));
				d->W[b][k][m].re = re;
				d->W[b][k][m].im = flip_votes > keep_votes ? -im : im; // stored for conj(W) x
			}
#undef W_AT
	free(buf);
	d->using_sdk = 1;
	return 0;
}

int freenect_dsp_using_sdk_weights(freenect_dsp* d)
{
	return d ? d->using_sdk : 0;
}

/* ------------------------------------------------ SpeexDSP stages */

#ifdef HAVE_SPEEXDSP
static int ensure_pp(freenect_dsp* d)
{
	if (!d->pp) {
		d->pp = speex_preprocess_state_init(NK, FREENECT_DSP_RATE);
		if (!d->pp)
			return -ENOMEM;
	}
	{
		int on;
		on = d->ns; speex_preprocess_ctl(d->pp, SPEEX_PREPROCESS_SET_DENOISE, &on);
		on = d->agc; speex_preprocess_ctl(d->pp, SPEEX_PREPROCESS_SET_AGC, &on);
		if (d->echo)
			speex_preprocess_ctl(d->pp, SPEEX_PREPROCESS_SET_ECHO_STATE, d->aec ? d->echo : NULL);
	}
	return 0;
}
#endif

int freenect_dsp_set_noise_suppression(freenect_dsp* d, int on)
{
	if (!d)
		return -1;
#ifdef HAVE_SPEEXDSP
	d->ns = on != 0;
	return ensure_pp(d);
#else
	(void)on;
	return -ENOTSUP;
#endif
}

int freenect_dsp_set_agc(freenect_dsp* d, int on)
{
	if (!d)
		return -1;
#ifdef HAVE_SPEEXDSP
	d->agc = on != 0;
	return ensure_pp(d);
#else
	(void)on;
	return -ENOTSUP;
#endif
}

int freenect_dsp_set_echo_cancellation(freenect_dsp* d, int on)
{
	if (!d)
		return -1;
#ifdef HAVE_SPEEXDSP
	d->aec = on != 0;
	if (d->aec && !d->echo) {
		// 256 ms tail, as the runtime's echo length (§40.6.8)
		d->echo = speex_echo_state_init(NK, FREENECT_DSP_RATE / 4);
		if (!d->echo)
			return -ENOMEM;
		{
			int rate = FREENECT_DSP_RATE;
			speex_echo_ctl(d->echo, SPEEX_ECHO_SET_SAMPLING_RATE, &rate);
		}
	}
	return ensure_pp(d);
#else
	(void)on;
	return -ENOTSUP;
#endif
}

int freenect_dsp_feed_reference(freenect_dsp* d, const int16_t* samples, int n)
{
	if (!d || !samples || n < 0)
		return -1;
#ifdef HAVE_SPEEXDSP
	{
		int room = (int)(sizeof(d->ref) / sizeof(d->ref[0])) - d->ref_n;
		if (n > room) { // keep the newest
			int drop = n - room;
			if (drop > d->ref_n) drop = d->ref_n;
			memmove(d->ref, d->ref + drop, sizeof(int16_t) * (size_t)(d->ref_n - drop));
			d->ref_n -= drop;
			room = (int)(sizeof(d->ref) / sizeof(d->ref[0])) - d->ref_n;
			if (n > room) { samples += n - room; n = room; }
		}
		memcpy(d->ref + d->ref_n, samples, sizeof(int16_t) * (size_t)n);
		d->ref_n += n;
	}
	return 0;
#else
	return -ENOTSUP;
#endif
}
