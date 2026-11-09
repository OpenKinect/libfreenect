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
 * Host-only test of the audio DSP (src/mclt.c, src/audio_dsp.c):
 * - the MCLT against the reference's tools/mclt_ref.py self-tests (relation
 *   to the textbook MCLT, reconstruction, Parseval, phase alignment, gain);
 * - the designed beams: distortionless, white noise gain, directivity;
 * - direction finding and beam choice on simulated sources;
 * - loading a weights file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../src/audio_dsp.c"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-64s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

static uint32_t rng = 12345;
static double urand(void) { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0; }
static double grand(void) { double u = urand() + 1e-12, v = urand(); return sqrt(-2.0 * log(u)) * cos(2.0 * M_PI * v); }

/* ------------------------------------------------------------ MCLT */

static void test_mclt(void)
{
	static float x[64 * NK], y[64 * NK];
	float frame[FN_MCLT_N];
	fn_cplx X[NK];
	fn_mclt_channel a, s;
	double err = 0.0, mx = 0.0, e2 = 0.0, acc = 0.0;
	int i, k, n, nf = 0;

	printf("MCLT filterbank (src/mclt.c)\n");
	/* 1. X = -sqrt(2/M) x textbook MCLT with the sine window */
	for (i = 0; i < FN_MCLT_N; i++)
		frame[i] = (float)grand();
	fn_mclt_forward(frame, X);
	for (k = 1; k < NK; k++) {
		double re = 0.0, im = 0.0, ph;
		for (n = 0; n < FN_MCLT_N; n++) {
			ph = -(n + 0.5 + NK / 2.0) * (k + 0.5) * M_PI / NK;
			re += frame[n] * sin((n + 0.5) * M_PI / (2 * NK)) * cos(ph);
			im += frame[n] * sin((n + 0.5) * M_PI / (2 * NK)) * sin(ph);
		}
		re *= -sqrt(2.0 / NK);
		im *= -sqrt(2.0 / NK);
		err = fmax(err, hypot(X[k].re - re, X[k].im - im));
		mx = fmax(mx, hypot(re, im));
	}
	printf("    (max |X - (-sqrt(2/M)) X_textbook| = %.3g, max |X| = %.3g)\n", err, mx);
	CHECK(err < 1e-4 * mx, "1. the textbook MCLT with the sine window, times -sqrt(2/M)");

	/* 2. reconstruction: delay 256, gain 1; only below ~60 Hz an error */
	fn_mclt_channel_reset(&a, 1);
	fn_mclt_channel_reset(&s, 1);
	for (i = 0; i < 64 * NK; i++)
		x[i] = (float)grand();
	for (i = 0; i < 64 * NK; i += NK) {
		fn_mclt_analysis(&a, x + i, X);
		fn_mclt_synthesis(&s, X, y + i);
	}
	err = 0.0;
	for (i = 2 * FN_MCLT_N; i < 64 * NK - NK; i++) {
		double e = y[i + NK] - x[i];
		e2 += e * e;
		err = fmax(err, fabs(e));
	}
	e2 = sqrt(e2 / (64 * NK - NK - 2 * FN_MCLT_N));
	printf("    (rms error %.4f, max %.3f; the reference: 0.0337, 0.132)\n", e2, err);
	CHECK(e2 < 0.04 && err < 0.2, "2. analysis + synthesis: x delayed by 256 (as the runtime)");
	{
		/* the error is all below 62.5 Hz: a 1 kHz tone comes back exactly */
		for (i = 0; i < 64 * NK; i++)
			x[i] = (float)sin(2.0 * M_PI * 1000.0 * i / 16000.0);
		fn_mclt_channel_reset(&a, 1);
		fn_mclt_channel_reset(&s, 1);
		for (i = 0; i < 64 * NK; i += NK) {
			fn_mclt_analysis(&a, x + i, X);
			fn_mclt_synthesis(&s, X, y + i);
		}
		err = 0.0;
		for (i = 2 * FN_MCLT_N; i < 64 * NK - NK; i++)
			err = fmax(err, fabs(y[i + NK] - x[i]));
		printf("    (1 kHz tone: max error %.2g)\n", err);
		CHECK(err < 1e-4, "   a 1 kHz tone exactly");
	}

	/* 3. Parseval: mean sum |X|^2 / M = 2 for unit white noise */
	fn_mclt_channel_reset(&a, 1);
	for (i = 0; i < 64 * NK; i++)
		x[i] = (float)grand();
	for (i = 0; i < 64 * NK; i += NK) {
		fn_mclt_analysis(&a, x + i, X);
		if (i >= FN_MCLT_N) {
			for (k = 0; k < NK; k++)
				acc += X[k].re * X[k].re + X[k].im * X[k].im;
			nf++;
		}
	}
	printf("    (mean sum |X_k|^2 / M = %.3f; the reference 1.970)\n", acc / nf / NK);
	CHECK(fabs(acc / nf / NK - 2.0) < 0.1, "3. energy: twice the input variance per band");

	/* 4. phase alignment: a tone at a band centre gives the same X every frame */
	{
		int align;
		for (align = 1; align >= 0; align--) {
			fn_cplx first = { 0, 0 };
			double spread = 0.0;
			fn_mclt_channel_reset(&a, align);
			for (i = 0; i < 24 * NK; i++)
				x[i] = (float)cos((40 + 0.5) * M_PI / NK * i + 0.3);
			for (i = 0, n = 0; i < 24 * NK; i += NK, n++) {
				fn_mclt_analysis(&a, x + i, X);
				if (n == 4)
					first = X[40];
				if (n > 4)
					spread = fmax(spread, hypot(X[40].re - first.re, X[40].im - first.im) / hypot(first.re, first.im));
			}
			printf("    (align %d: relative change of X_40 across frames %.2g)\n", align, spread);
			if (align)
				CHECK(spread < 1e-3, "4. with phase alignment a steady tone has a steady spectrum");
			else
				CHECK(spread > 1.0, "   without it, it rotates");
		}
	}
}

/* ------------------------------------------------------------ beams */

static double response(const freenect_dsp* d, const fn_cplx* w, double theta, double f)
{
	dc v[MAXM], r = { 0, 0 };
	int m;
	steering(d, theta, FN_DSP_BEAM_RANGE, f, FN_DSP_C, v);
	for (m = 0; m < d->nm; m++) {
		dc wc;
		wc.re = w[m].re; wc.im = -w[m].im;
		r = dc_add(r, dc_mul(wc, v[m]));
	}
	return hypot(r.re, r.im);
}

static double di(const freenect_dsp* d, const fn_cplx* w, double f)
{
	/* directivity: |w^H v|^2 / (w^H Gamma w) */
	double num, den = 0.0;
	int i, j;
	num = response(d, w, d->beam_angle[NB / 2], f);
	for (i = 0; i < d->nm; i++)
		for (j = 0; j < d->nm; j++) {
			double dist = fabs(d->my[i] - d->my[j]);
			double g = sinc(2.0 * M_PI * f * dist / FN_DSP_C);
			den += g * (w[i].re * w[j].re + w[i].im * w[j].im);
		}
	return num * num / den;
}

static void test_beams(freenect_dsp* d)
{
	int b, k, m, ok = 1, okw = 1;
	double worst = 1e9;
	printf("Designed beams\n");
	for (b = 0; b < NB; b++)
		for (k = 1; k < NK; k++) {
			double f = (k + 0.5) * 31.25, r = response(d, d->W[b][k], d->beam_angle[b], f), ww = 0.0;
			ok &= fabs(r - 1.0) < 1e-3;
			for (m = 0; m < d->nm; m++)
				ww += d->W[b][k][m].re * d->W[b][k][m].re + d->W[b][k][m].im * d->W[b][k][m].im;
			worst = fmin(worst, 1.0 / ww);
			okw &= 1.0 / ww >= FN_DSP_MIN_WNG * 0.999;
		}
	CHECK(ok, "each beam passes its own direction unchanged, at every band");
	printf("    (smallest white noise gain %.3f)\n", worst);
	CHECK(okw, "white noise gain at least -10 dB everywhere");
	{
		fn_cplx ds[MAXM];
		double f = 1000.0, dim, did;
		dc v[MAXM];
		steering(d, 0.0, FN_DSP_BEAM_RANGE, f, FN_DSP_C, v);
		for (m = 0; m < d->nm; m++) {
			ds[m].re = (float)(v[m].re / d->nm);
			ds[m].im = (float)(v[m].im / d->nm);
		}
		k = (int)(f / 31.25);
		dim = di(d, d->W[NB / 2][k], f);
		did = di(d, ds, f);
		printf("    (1 kHz directivity: designed %.2f dB, delay-and-sum %.2f dB)\n", 10 * log10(dim), 10 * log10(did));
		CHECK(dim > did * 1.1, "more directive than delay-and-sum at 1 kHz");
	}
	{
		/* beam +50 against a source at -50 */
		double att = 0.0;
		int n = 0;
		for (k = 32; k <= 112; k += 8) {
			att += 20 * log10(response(d, d->W[NB - 1][k], d->beam_angle[0], (k + 0.5) * 31.25));
			n++;
		}
		printf("    (beam +50 deg, source at -50 deg, 1-3.5 kHz: %.1f dB)\n", att / n);
		CHECK(att / n < -6.0, "a source 100 degrees off the beam is at least 6 dB down");
	}
}

/* ------------------------------------------------- simulated sources */

/* 4 microphone signals of a noise-like source (sum of tones, 300-3500 Hz)
 * at angle theta, 1.5 m away, plus a little sensor noise; full scale 1.
 * on: whether the source sounds in this sample. */
static void simulate(const freenect_dsp* d, double theta, float* out, int n, int t0, double amp, int silent_until)
{
	static double fr[60], ph[60];
	static int init;
	double xs = 1.5 * cos(theta), ys = 1.5 * sin(theta), tau[MAXM];
	int i, m, j;
	if (!init) {
		for (j = 0; j < 60; j++) {
			fr[j] = 300.0 + urand() * 3200.0;
			ph[j] = urand() * 2 * M_PI;
		}
		init = 1;
	}
	for (m = 0; m < d->nm; m++)
		tau[m] = sqrt((xs - d->mx[m]) * (xs - d->mx[m]) + (ys - d->my[m]) * (ys - d->my[m])) / 343.0;
	for (i = 0; i < n; i++) {
		double t = (t0 + i) / 16000.0;
		for (m = 0; m < d->nm; m++) {
			double s = 0.0;
			if (t0 + i >= silent_until)
				for (j = 0; j < 60; j++)
					s += cos(2 * M_PI * fr[j] * (t - tau[m]) + ph[j]);
			out[4 * i + m] = (float)(amp * s / sqrt(30.0) + 1e-4 * grand());
		}
	}
}

static void test_direction(void)
{
	static const double angles[3] = { -30.0, 0.0, 35.0 };
	static float in[4 * 1600];
	static int16_t out[1600];
	int a;
	printf("Direction of a source, and the beam it gets\n");
	for (a = 0; a < 3; a++) {
		freenect_dsp* d = freenect_dsp_create(NULL);
		double ang = -99, conf = 0;
		int t, r;
		char what[100];
		for (t = 0; t < 16000 * 4; t += 1600) {
			simulate(d, angles[a] * M_PI / 180.0, in, 1600, t, 0.05, 16000);
			freenect_dsp_process_float(d, in, 1600, out);
		}
		r = freenect_dsp_get_source_angle(d, &ang, &conf);
		printf("    (source at %+.0f deg: found %+.1f deg, confidence %.2f; beam %+.0f deg)\n", angles[a],
		       ang * 180 / M_PI, conf, freenect_dsp_get_beam_angle(d) * 180 / M_PI);
		snprintf(what, sizeof(what), "source at %+.0f degrees: found within 6 degrees", angles[a]);
		CHECK(r == 0 && fabs(ang * 180 / M_PI - angles[a]) < 6.0, what);
		snprintf(what, sizeof(what), "  confident, and the nearest beam is chosen");
		CHECK(conf > SEL_MIN_CONF && fabs(freenect_dsp_get_beam_angle(d) * 180 / M_PI - 10.0 * floor(angles[a] / 10.0 + 0.5)) < 0.1, what);
		freenect_dsp_destroy(d);
	}
	{
		/* adaptive: steered onto the source itself */
		freenect_dsp* d = freenect_dsp_create(NULL);
		int t;
		freenect_dsp_set_beam_mode(d, FREENECT_DSP_BEAM_ADAPTIVE);
		for (t = 0; t < 16000 * 4; t += 1600) {
			simulate(d, 35.0 * M_PI / 180.0, in, 1600, t, 0.05, 16000);
			freenect_dsp_process_float(d, in, 1600, out);
		}
		printf("    (adaptive: beam at %+.1f deg)\n", freenect_dsp_get_beam_angle(d) * 180 / M_PI);
		CHECK(fabs(freenect_dsp_get_beam_angle(d) * 180 / M_PI - 35.0) < 6.0, "adaptive mode steers to the source, between the fixed beams");
		freenect_dsp_destroy(d);
	}
	{
		/* silence: no direction */
		freenect_dsp* d = freenect_dsp_create(NULL);
		int t;
		for (t = 0; t < 16000 * 2; t += 1600) {
			simulate(d, 0.0, in, 1600, t, 0.05, 1 << 30);
			freenect_dsp_process_float(d, in, 1600, out);
		}
		CHECK(freenect_dsp_get_source_angle(d, NULL, NULL) == -1, "no source in silence");
		freenect_dsp_destroy(d);
	}
}

/* end to end: a tone straight ahead through the centre beam */
static void test_gain(void)
{
	freenect_dsp* d = freenect_dsp_create(NULL);
	static float in[4 * 16000];
	static int16_t out[16000];
	int i, m, peak = 0, peak_off = 0;
	printf("Output level\n");
	freenect_dsp_set_beam_mode(d, FREENECT_DSP_BEAM_MANUAL);
	freenect_dsp_set_beam(d, NB / 2);
	for (i = 0; i < 16000; i++)
		for (m = 0; m < 4; m++) {
			double tau = sqrt(1.0 + d->my[m] * d->my[m]) / 343.0;
			in[4 * i + m] = (float)(0.05 * sin(2 * M_PI * 1000.0 * (i / 16000.0 - tau)));
		}
	freenect_dsp_process_float(d, in, 16000, out);
	for (i = 8000; i < 16000; i++)
		peak = abs(out[i]) > peak ? abs(out[i]) : peak;
	printf("    (1 kHz at 0.05 full scale from 0 deg: output peak %d = %.3f x input; the runtime: x2)\n", peak, peak / 32767.0 / 0.05);
	CHECK(fabs(peak / 32767.0 / 0.05 - 2.0) < 0.1, "a source in the beam comes out at the runtime's level (x2, AGC off)");
	freenect_dsp_set_beam(d, NB - 1);
	freenect_dsp_process_float(d, in, 16000, out);
	for (i = 8000; i < 16000; i++)
		peak_off = abs(out[i]) > peak_off ? abs(out[i]) : peak_off;
	printf("    (with beam +50 deg: %.1f dB)\n", 20 * log10((double)peak_off / peak));
	CHECK(peak_off < peak / 2, "  and is attenuated by the +50 degree beam");
	CHECK(freenect_dsp_set_beam(d, NB) == -1 && freenect_dsp_set_beam(d, -1) == -1, "beam numbers outside 0..10 are refused");
	freenect_dsp_destroy(d);
}

/* ------------------------------------------------- weights file */

static void put32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static void putf(uint8_t* p, float f) { uint32_t u; memcpy(&u, &f, 4); put32(p, u); }

/* A file as the fetcher writes it, from the designed beams on the table's
 * grid (k x 31.25 Hz). conj_convention: 1 stores w with y = conj(w) x. */
static int write_weights(const char* path, freenect_dsp* d, int conj_convention, int corrupt)
{
	uint8_t* buf = (uint8_t*)calloc(1, BW_SIZE);
	uint8_t *wt = buf + BW_HEADER + NB * 8, *st = wt + BW_TABLE;
	FILE* f;
	int b, k, m;
	memcpy(buf, "FNBW", 4);
	put32(buf + 4, 1); put32(buf + 8, 4); put32(buf + 12, NK); put32(buf + 16, NB);
	for (b = 0; b < NB; b++) {
		uint64_t u;
		memcpy(&u, &d->beam_angle[b], 8);
		put32(buf + BW_HEADER + 8 * b, (uint32_t)u);
		put32(buf + BW_HEADER + 8 * b + 4, (uint32_t)(u >> 32));
		for (k = 0; k < NK; k++) {
			fn_cplx w[MAXM];
			dc v[MAXM];
			double f = k ? k * 31.25 : 15.625;
			design_bin(d, d->beam_angle[b], f, w);
			steering(d, d->beam_angle[b], 1.0, f, 343.0, v);
			for (m = 0; m < 4; m++) {
				size_t o = 8 * (((size_t)b * NK + k) * 4 + m);
				putf(wt + o, w[m].re);
				putf(wt + o + 4, conj_convention ? w[m].im : -w[m].im);
				putf(st + o, (float)v[m].re);
				putf(st + o + 4, (float)v[m].im);
			}
		}
	}
	put32(buf + BW_SIZE - 4, crc32_of(buf, BW_SIZE - 4));
	if (corrupt)
		buf[200] ^= 1;
	f = fopen(path, "wb");
	if (!f) {
		free(buf);
		return -1;
	}
	fwrite(buf, 1, BW_SIZE, f);
	fclose(f);
	free(buf);
	return 0;
}

static void test_weights_file(void)
{
	const char* path = "test_beam_weights.bin";
	freenect_dsp* d = freenect_dsp_create(NULL);
	int conv;
	printf("Microsoft's weights from a file\n");
	CHECK(freenect_dsp_load_sdk_weights(d, "no_such_file.bin") == -ENOENT && !freenect_dsp_using_sdk_weights(d),
	      "no file: refused, designed weights stay");
	for (conv = 1; conv >= 0; conv--) {
		double maxd = 0.0;
		int b, k;
		write_weights(path, d, conv, 0);
		CHECK(freenect_dsp_load_sdk_weights(d, path) == 0 && freenect_dsp_using_sdk_weights(d),
		      conv ? "a valid file is loaded" : "a file with the other conjugation is loaded");
		/* a wrong conjugation would steer each beam to the mirror direction */
		for (b = 0; b < NB; b++)
			for (k = 16; k <= 224; k++)
				maxd = fmax(maxd, fabs(response(d, d->W[b][k], d->beam_angle[b], (k + 0.5) * 31.25) - 1.0));
		printf("    (each beam towards its own direction, 0.5-7 kHz: gain 1 within %.3f)\n", maxd);
		CHECK(maxd < 0.05, "  its convention is recognised: each beam passes its own direction");
	}
	write_weights(path, d, 1, 1);
	CHECK(freenect_dsp_load_sdk_weights(d, path) == -EINVAL, "a damaged file (CRC) is refused");
	{
		freenect_mic_geometry g;
		freenect_dsp* o;
		memset(&g, 0, sizeof(g));
		g.num_mics = 4;
		g.mics[0].y_mm = -95; g.mics[1].y_mm = -28; g.mics[2].y_mm = 28; g.mics[3].y_mm = 95;
		o = freenect_dsp_create(&g);
		write_weights(path, d, 1, 0);
		CHECK(o && freenect_dsp_load_sdk_weights(o, path) == -EINVAL, "another array's DSP refuses the Kinect's weights");
		freenect_dsp_destroy(o);
	}
	remove(path);
	freenect_dsp_destroy(d);
}

int main(void)
{
	freenect_dsp* d;
	test_mclt();
	d = freenect_dsp_create(NULL);
	CHECK(d != NULL, "DSP for the Kinect's array");
	test_beams(d);
	freenect_dsp_destroy(d);
	test_direction();
	test_gain();
	test_weights_file();
	{
		freenect_mic_geometry g;
		memset(&g, 0, sizeof(g));
		g.num_mics = 1;
		CHECK(freenect_dsp_create(&g) == NULL, "one microphone: no DSP");
	}
#ifdef HAVE_SPEEXDSP
	{
		/* a loudspeaker's sound reaching the microphones 10 ms later */
		static float in[4 * 1600];
		static int16_t ref[1600], out[1600];
		double e_on = 0.0, e_off = 0.0;
		int run, t, i, m;
		printf("SpeexDSP stages\n");
		for (run = 0; run < 2; run++) {
			static int16_t hist[160 + 1600];
			d = freenect_dsp_create(NULL);
			CHECK(freenect_dsp_set_noise_suppression(d, 0) == 0 && freenect_dsp_set_agc(d, 0) == 0,
			      run ? "noise suppression and AGC can be switched" : "noise suppression and AGC available");
			if (run)
				CHECK(freenect_dsp_set_echo_cancellation(d, 1) == 0, "echo cancellation on");
			rng = 777;
			memset(hist, 0, sizeof(hist));
			for (t = 0; t < 16000 * 8; t += 1600) {
				for (i = 0; i < 1600; i++)
					ref[i] = (int16_t)(3000.0 * grand());
				memcpy(hist + 160, ref, sizeof(ref));
				for (i = 0; i < 1600; i++)
					for (m = 0; m < 4; m++)
						in[4 * i + m] = (float)(0.25 * hist[i] / 32768.0);
				memmove(hist, hist + 1600, 160 * sizeof(int16_t));
				freenect_dsp_feed_reference(d, ref, 1600);
				freenect_dsp_process_float(d, in, 1600, out);
				if (t >= 16000 * 6)
					for (i = 0; i < 1600; i++) {
						if (run) e_on += (double)out[i] * out[i];
						else e_off += (double)out[i] * out[i];
					}
			}
			freenect_dsp_destroy(d);
		}
		printf("    (echo after 6 s: %.1f dB with echo cancellation)\n", 10 * log10(e_on / e_off));
		CHECK(e_on < e_off / 10.0, "echo reduced by at least 10 dB");
	}
#else
	d = freenect_dsp_create(NULL);
	CHECK(freenect_dsp_set_noise_suppression(d, 1) == -ENOTSUP && freenect_dsp_set_echo_cancellation(d, 1) == -ENOTSUP,
	      "without SpeexDSP: noise suppression and echo cancellation unavailable");
	freenect_dsp_destroy(d);
#endif
	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
