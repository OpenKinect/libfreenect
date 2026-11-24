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

#include <math.h>
#include <string.h>
#include "mclt.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define N FN_MCLT_N
#define M FN_MCLT_M

typedef struct {
	double re, im;
} dcplx;

static int ready;
static dcplx fft_tw[N / 2];        // e^{-j 2 pi k / N}
static unsigned short bitrev[N];
static dcplx fwd_tw[M + 1], inv_tw[M + 1];
static double s_fwd, s_inv, r2;

// The constants as the runtime computes them (mclt_ref.py): theta from
// float pi, cos(pi/4) from float pi.
void fn_mclt_init(void)
{
	int i, k;
	double pif, theta, c0;
	if (ready)
		return;
	for (i = 0; i < N / 2; i++) {
		fft_tw[i].re = cos(2.0 * M_PI * i / N);
		fft_tw[i].im = -sin(2.0 * M_PI * i / N);
	}
	for (i = 0; i < N; i++) {
		int r = 0, b;
		for (b = 0; b < 9; b++)
			if (i & (1 << b))
				r |= 1 << (8 - b);
		bitrev[i] = (unsigned short)r;
	}
	pif = (double)(float)M_PI;
	theta = (double)(float)(((double)(float)(0.5 / M) + 0.5) * pif);
	c0 = (double)(float)cos(pif / 4.0);
	for (k = 0; k <= M; k++) {
		double c = cos(theta * k), s = sin(theta * k);
		// t_k = (c0 - j c0) e^{-j theta k};  z_k = (c0 + j c0) e^{+j theta k}
		fwd_tw[k].re = c0 * c - c0 * s;
		fwd_tw[k].im = -c0 * s - c0 * c;
		inv_tw[k].re = c0 * c - c0 * s;
		inv_tw[k].im = c0 * s + c0 * c;
	}
	s_fwd = (double)(float)sqrt((double)(float)(0.5 / M));
	s_inv = (double)(float)(1.0 / sqrt(M * 32.0));
	r2 = (double)(float)sqrt(2.0);
	ready = 1;
}

// In-place radix-2 FFT of N points; inverse without scaling.
static void fft(dcplx* a, int inverse)
{
	int i, len;
	for (i = 0; i < N; i++) {
		int j = bitrev[i];
		if (i < j) {
			dcplx t = a[i];
			a[i] = a[j];
			a[j] = t;
		}
	}
	for (len = 2; len <= N; len <<= 1) {
		int half = len / 2, step = N / len, s, k;
		for (s = 0; s < N; s += len) {
			for (k = 0; k < half; k++) {
				dcplx w = fft_tw[k * step], u = a[s + k], v = a[s + k + half], t;
				if (inverse)
					w.im = -w.im;
				t.re = v.re * w.re - v.im * w.im;
				t.im = v.re * w.im + v.im * w.re;
				a[s + k].re = u.re + t.re;
				a[s + k].im = u.im + t.im;
				a[s + k + half].re = u.re - t.re;
				a[s + k + half].im = u.im - t.im;
			}
		}
	}
}

void fn_mclt_forward(const float x[N], fn_cplx X[M])
{
	dcplx a[N], P[M + 1];
	int k;
	fn_mclt_init();
	for (k = 0; k < N; k++) {
		a[k].re = x[k];
		a[k].im = 0.0;
	}
	fft(a, 0);
	a[0].im = 0.0;   // only Re DC and Re Nyquist are read
	a[M].im = 0.0;
	for (k = 0; k <= M; k++) {
		P[k].re = fwd_tw[k].re * a[k].re - fwd_tw[k].im * a[k].im;
		P[k].im = fwd_tw[k].re * a[k].im + fwd_tw[k].im * a[k].re;
	}
	for (k = 0; k < M; k++) {
		// X_k = s (P_{k+1} + j P_k)
		X[k].re = (float)(s_fwd * (P[k + 1].re - P[k].im));
		X[k].im = (float)(s_fwd * (P[k + 1].im + P[k].re));
	}
	X[0].im = 0.0f; // the runtime's packed format has no Im X0
}

void fn_mclt_inverse(const fn_cplx Y[M], float y[N])
{
	dcplx V[N];
	int k;
	fn_mclt_init();
	for (k = 1; k < M; k++) {
		// V_k = z_k (Y_{k-1} - j Y_k)
		double re = (double)Y[k - 1].re + Y[k].im, im = (double)Y[k - 1].im - Y[k].re;
		V[k].re = inv_tw[k].re * re - inv_tw[k].im * im;
		V[k].im = inv_tw[k].re * im + inv_tw[k].im * re;
	}
	V[0].re = ((double)Y[0].re + Y[0].im) * r2;
	V[0].im = 0.0;
	V[M].re = -((double)Y[M - 1].re + Y[M - 1].im) * r2;
	V[M].im = 0.0;
	for (k = M + 1; k < N; k++) {
		V[k].re = V[N - k].re;
		V[k].im = -V[N - k].im;
	}
	fft(V, 1);
	for (k = 0; k < N; k++)
		y[k] = (float)(V[k].re * s_inv);
}

void fn_mclt_channel_reset(fn_mclt_channel* c, int align)
{
	memset(c, 0, sizeof(*c));
	c->align = align;
	fn_mclt_init();
}

// Bins k >= 1 times (j (-1)^(k+1))^q (§40.6.7, FUN_1800237e0).
static void rotate(fn_cplx* X, unsigned q)
{
	int k;
	q &= 3;
	if (!q)
		return;
	for (k = 1; k < M; k++) {
		float re = X[k].re, im = X[k].im;
		int odd = k & 1;   // (-1)^(k+1) = +1 for odd k
		switch (q) {
		case 1: // * j(+-1)
			X[k].re = odd ? -im : im;
			X[k].im = odd ? re : -re;
			break;
		case 2: // * -1
			X[k].re = -re;
			X[k].im = -im;
			break;
		case 3: // * -j(+-1)
			X[k].re = odd ? im : -im;
			X[k].im = odd ? -re : re;
			break;
		}
	}
}

void fn_mclt_analysis(fn_mclt_channel* c, const float hop[M], fn_cplx X[M])
{
	memmove(c->ring, c->ring + M, M * sizeof(float));
	memcpy(c->ring + M, hop, M * sizeof(float));
	fn_mclt_forward(c->ring, X);
	if (c->align)
		rotate(X, c->n);
	c->n++;
}

void fn_mclt_synthesis(fn_mclt_channel* c, const fn_cplx X[M], float out[M])
{
	fn_cplx Y[M];
	float y[N];
	int i;
	memcpy(Y, X, sizeof(Y));
	Y[0].im = 0.0f;
	if (c->align)
		rotate(Y, (0u - c->n) & 3u);
	fn_mclt_inverse(Y, y);
	for (i = 0; i < M; i++)
		out[i] = y[i] + c->ola[i];
	memcpy(c->ola, y + M, M * sizeof(float));
	c->n++;
}
