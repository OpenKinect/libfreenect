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
#pragma once

/*
 * The filterbank of Microsoft's Kinect audio DSP (§40.6.7): a 256-band
 * MCLT (modulated complex lapped transform) over 512 samples with the sine
 * window built in, hop 256 (16 ms at 16 kHz), as in the reference's
 * tools/mclt_ref.py:
 *   forward: real FFT of the frame, then X_k = s (t_{k+1} F_{k+1} + j t_k F_k)
 *   inverse: V_k = z_k (Y_{k-1} - j Y_k), Hermitian real IFFT, x 1/sqrt(8192)
 * Spectra are complex X[0..255]; subband k is centred at (k + 1/2) 31.25 Hz.
 * The runtime keeps only the real part of X[0] (its packed format); so does
 * this, which makes reconstruction exact except below about 60 Hz.
 */

#include <stdint.h>

#define FN_MCLT_N 512
#define FN_MCLT_M 256

typedef struct {
	float re, im;
} fn_cplx;

typedef struct {
	float ring[FN_MCLT_N];   // the last 512 input samples
	float ola[FN_MCLT_M];    // second half of the previous inverse frame
	uint32_t n;              // frame counter, for the phase alignment
	int align;               // frame phase alignment (pid 49, on in the runtime)
} fn_mclt_channel;

void fn_mclt_init(void);
void fn_mclt_channel_reset(fn_mclt_channel* c, int align);
// One MCLT of 512 real samples (no phase alignment, no ring).
void fn_mclt_forward(const float x[FN_MCLT_N], fn_cplx X[FN_MCLT_M]);
void fn_mclt_inverse(const fn_cplx X[FN_MCLT_M], float y[FN_MCLT_N]);
// Pushes 256 new samples and returns the spectrum of the last 512.
void fn_mclt_analysis(fn_mclt_channel* c, const float hop[FN_MCLT_M], fn_cplx X[FN_MCLT_M]);
// Inverse, overlap-added: 256 output samples; advances the frame counter.
void fn_mclt_synthesis(fn_mclt_channel* c, const fn_cplx X[FN_MCLT_M], float out[FN_MCLT_M]);
