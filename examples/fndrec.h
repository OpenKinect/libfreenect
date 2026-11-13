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

// Depth recordings for freenect-nuitest (--record, --replay): small enough to
// hand over a minute of a real room (a few MB), lossless at 320x240.
//
// A file is the 8-byte magic "FNDREC3\0", then per frame a uint32 frame
// number, a uint32 payload size and the payload, coded with an adaptive
// binary range coder (as in LZMA):
// - the frame's distinct depth values (mm), as presence bits over the range
//   between the smallest and the largest, each predicted by whether the
//   value was present in the frame before; the pixels are indices into this
//   table, which follow the Kinect's disparity steps;
// - each pixel (the top-left of each 2x2 block of the 640x480 frame) as the
//   difference to a prediction: the same pixel in the frame before where its
//   left and upper neighbours did not change, else the median edge predictor
//   of the left, upper and upper-left neighbours (as in JPEG-LS). The
//   difference is coded as zero / sign / magnitude bits in contexts of the
//   predictor and the local gradient.
// Frames depend on the ones before, so a file is read from the start.
// "FNDREC2" files (the same pixels, Rice-coded per frame) can still be read.

#ifndef FNDREC_H_INCLUDED
#define FNDREC_H_INCLUDED

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FNDREC_W 320
#define FNDREC_H 240
#define FNDREC_N (FNDREC_W * FNDREC_H)
#define FNDREC_VMAX 16384 // depth values above this (mm) are kept as 0

static const char fndrec_magic3[8] = { 'F', 'N', 'D', 'R', 'E', 'C', '3', 0 };
static const char fndrec_magic2[8] = { 'F', 'N', 'D', 'R', 'E', 'C', '2', 0 };

/* ------------------------------------------------------------ range coder */

typedef struct {
	uint8_t *p, *end;
	uint64_t low;
	uint32_t range;
	uint8_t cache;
	uint64_t cache_size;
	int overflow;
} fndrec_enc;

typedef struct {
	const uint8_t *p, *end;
	uint32_t range, code;
	int bad;
} fndrec_dec;

static void fndrec_out(fndrec_enc* e, uint8_t b)
{
	if (e->p < e->end)
		*e->p++ = b;
	else
		e->overflow = 1;
}

static void fndrec_shift_low(fndrec_enc* e)
{
	if ((uint32_t)e->low < 0xFF000000u || (e->low >> 32) != 0) {
		uint8_t carry = (uint8_t)(e->low >> 32), c = e->cache;
		do {
			fndrec_out(e, (uint8_t)(c + carry));
			c = 0xFF;
		} while (--e->cache_size != 0);
		e->cache = (uint8_t)(e->low >> 24);
	}
	e->cache_size++;
	e->low = (e->low & 0x00FFFFFFu) << 8;
}

static void fndrec_enc_init(fndrec_enc* e, uint8_t* buf, size_t n)
{
	e->p = buf;
	e->end = buf + n;
	e->low = 0;
	e->range = 0xFFFFFFFFu;
	e->cache = 0;
	e->cache_size = 1;
	e->overflow = 0;
}

static void fndrec_enc_bit(fndrec_enc* e, uint16_t* prob, int bit)
{
	uint32_t bound = (e->range >> 11) * *prob;
	if (!bit) {
		e->range = bound;
		*prob += (uint16_t)((2048 - *prob) >> 5);
	} else {
		e->low += bound;
		e->range -= bound;
		*prob -= (uint16_t)(*prob >> 5);
	}
	while (e->range < (1u << 24)) {
		e->range <<= 8;
		fndrec_shift_low(e);
	}
}

static void fndrec_enc_direct(fndrec_enc* e, uint32_t v, int n)
{
	while (n-- > 0) {
		e->range >>= 1;
		if ((v >> n) & 1)
			e->low += e->range;
		while (e->range < (1u << 24)) {
			e->range <<= 8;
			fndrec_shift_low(e);
		}
	}
}

static void fndrec_enc_flush(fndrec_enc* e)
{
	int i;
	for (i = 0; i < 5; i++)
		fndrec_shift_low(e);
}

static void fndrec_dec_init(fndrec_dec* d, const uint8_t* buf, size_t n)
{
	int i;
	d->p = buf;
	d->end = buf + n;
	d->range = 0xFFFFFFFFu;
	d->code = 0;
	d->bad = 0;
	for (i = 0; i < 5; i++)
		d->code = (d->code << 8) | (d->p < d->end ? *d->p++ : (d->bad = 1, 0));
}

static void fndrec_dec_norm(fndrec_dec* d)
{
	while (d->range < (1u << 24)) {
		d->range <<= 8;
		d->code = (d->code << 8) | (d->p < d->end ? *d->p++ : (d->bad = 1, 0));
	}
}

static int fndrec_dec_bit(fndrec_dec* d, uint16_t* prob)
{
	uint32_t bound = (d->range >> 11) * *prob;
	int bit;
	if (d->code < bound) {
		d->range = bound;
		*prob += (uint16_t)((2048 - *prob) >> 5);
		bit = 0;
	} else {
		d->code -= bound;
		d->range -= bound;
		*prob -= (uint16_t)(*prob >> 5);
		bit = 1;
	}
	fndrec_dec_norm(d);
	return bit;
}

static uint32_t fndrec_dec_direct(fndrec_dec* d, int n)
{
	uint32_t v = 0;
	while (n-- > 0) {
		d->range >>= 1;
		if (d->code >= d->range) {
			d->code -= d->range;
			v = (v << 1) | 1;
		} else {
			v <<= 1;
		}
		fndrec_dec_norm(d);
	}
	return v;
}

/* ------------------------------------------------------------ the model */

#define FNDREC_CTX 8   // pixel contexts: predictor (2) x gradient (4)
#define FNDREC_MAG 14  // unary magnitude bits before the escape

typedef struct {
	// the coder's probabilities, kept from frame to frame
	uint16_t zero[FNDREC_CTX], sign[FNDREC_CTX], mag[FNDREC_CTX][FNDREC_MAG];
	uint16_t present[2], range_bits[2][16];
	// the frame before
	uint16_t prev_value[FNDREC_N]; // mm
	uint8_t flicker[FNDREC_N];     // the pixel changed in the frame before
	uint8_t prev_present[FNDREC_VMAX];
	int have_prev;
	// this frame
	uint16_t q[FNDREC_N], value[FNDREC_VMAX], index_of[FNDREC_VMAX];
	uint8_t present_now[FNDREC_VMAX];
} fndrec_state;

static void fndrec_state_init(fndrec_state* s)
{
	int i, j;
	for (i = 0; i < FNDREC_CTX; i++) {
		s->zero[i] = s->sign[i] = 1024;
		for (j = 0; j < FNDREC_MAG; j++)
			s->mag[i][j] = 1024;
	}
	s->present[0] = s->present[1] = 1024;
	for (i = 0; i < 2; i++)
		for (j = 0; j < 16; j++)
			s->range_bits[i][j] = 1024;
	memset(s->prev_present, 0, sizeof(s->prev_present));
	s->have_prev = 0;
	memset(s->flicker, 0, sizeof(s->flicker));
}

// the prediction for pixel i and its context
static int fndrec_predict(const fndrec_state* s, const uint16_t* tq, int x, int y, int* ctx)
{
	const uint16_t* q = s->q;
	int i = y * FNDREC_W + x;
	int a = x ? q[i - 1] : (y ? q[i - FNDREC_W] : 0);
	int b = y ? q[i - FNDREC_W] : a;
	int c = x && y ? q[i - FNDREC_W - 1] : b;
	int g = abs(a - c) + abs(b - c), gc = g == 0 ? 0 : g < 3 ? 1 : g < 12 ? 2 : 3;
	if (tq && (!x || q[i - 1] == tq[i - 1]) && (!y || q[i - FNDREC_W] == tq[i - FNDREC_W]) && (x || y)) {
		*ctx = 4 + 2 * s->flicker[i] + (gc > 1); // still: the frame before
		return tq[i];
	}
	*ctx = gc;
	{
		int mx = a > b ? a : b, mn = a < b ? a : b;
		return c >= mx ? mn : c <= mn ? mx : a + b - c;
	}
}

// the frame before as indices into this frame's table: the nearest value
static void fndrec_temporal(fndrec_state* s, uint16_t* tq, int nv)
{
	static uint16_t near_index[FNDREC_VMAX];
	int v, k = 0, i;
	for (v = 0; v < FNDREC_VMAX; v++) {
		while (k + 1 < nv && s->value[k + 1] <= v)
			k++;
		// the closer of value[k] and value[k + 1]
		near_index[v] = (uint16_t)(k + 1 < nv && s->value[k + 1] - v < v - s->value[k] ? k + 1 : k);
	}
	for (i = 0; i < FNDREC_N; i++)
		tq[i] = near_index[s->prev_value[i]];
}

static void fndrec_enc_residual(fndrec_enc* e, fndrec_state* s, int ctx, int r)
{
	int m = r < 0 ? -r : r, j;
	fndrec_enc_bit(e, &s->zero[ctx], m != 0);
	if (!m)
		return;
	fndrec_enc_bit(e, &s->sign[ctx], r < 0);
	for (j = 1; j < FNDREC_MAG + 1 && j < m; j++)
		fndrec_enc_bit(e, &s->mag[ctx][j - 1], 1);
	if (m <= FNDREC_MAG)
		fndrec_enc_bit(e, &s->mag[ctx][m - 1], 0);
	else
		fndrec_enc_direct(e, (uint32_t)(m - FNDREC_MAG - 1), 14);
}

static int fndrec_dec_residual(fndrec_dec* d, fndrec_state* s, int ctx)
{
	int neg, m = 1;
	if (!fndrec_dec_bit(d, &s->zero[ctx]))
		return 0;
	neg = fndrec_dec_bit(d, &s->sign[ctx]);
	while (m <= FNDREC_MAG && fndrec_dec_bit(d, &s->mag[ctx][m - 1]))
		m++;
	if (m > FNDREC_MAG)
		m = FNDREC_MAG + 1 + (int)fndrec_dec_direct(d, 14);
	return neg ? -m : m;
}

static void fndrec_enc_number(fndrec_enc* e, uint16_t* bits, uint32_t v)
{
	int i;
	for (i = 15; i >= 0; i--)
		fndrec_enc_bit(e, &bits[i], (v >> i) & 1);
}

static uint32_t fndrec_dec_number(fndrec_dec* d, uint16_t* bits)
{
	uint32_t v = 0;
	int i;
	for (i = 15; i >= 0; i--)
		v = (v << 1) | (uint32_t)fndrec_dec_bit(d, &bits[i]);
	return v;
}

/* ------------------------------------------------------------ frames */

// Writes one 640x480 frame (mm). 0: done, -1: write error.
static int fndrec_write(FILE* f, fndrec_state* s, const uint16_t* depth, uint32_t frame)
{
	static uint8_t buf[FNDREC_N * 4 + 65536];
	static uint16_t tq[FNDREC_N], cur[FNDREC_N];
	fndrec_enc e;
	uint32_t hdr[2];
	int x, y, i, v, vmin = FNDREC_VMAX, vmax = 0, nv = 0;
	memset(s->present_now, 0, sizeof(s->present_now));
	for (y = 0; y < FNDREC_H; y++)
		for (x = 0; x < FNDREC_W; x++) {
			int d = depth[2 * y * 640 + 2 * x];
			if (d >= FNDREC_VMAX)
				d = 0;
			cur[y * FNDREC_W + x] = (uint16_t)d;
			s->present_now[d] = 1;
		}
	for (v = 0; v < FNDREC_VMAX; v++)
		if (s->present_now[v]) {
			if (v < vmin) vmin = v;
			vmax = v;
			s->index_of[v] = (uint16_t)nv;
			s->value[nv++] = (uint16_t)v;
		}
	fndrec_enc_init(&e, buf, sizeof(buf));
	fndrec_enc_number(&e, s->range_bits[0], (uint32_t)vmin);
	fndrec_enc_number(&e, s->range_bits[1], (uint32_t)vmax);
	for (v = vmin + 1; v < vmax; v++)
		fndrec_enc_bit(&e, &s->present[s->prev_present[v]], s->present_now[v]);
	if (s->have_prev)
		fndrec_temporal(s, tq, nv);
	for (y = 0; y < FNDREC_H; y++)
		for (x = 0; x < FNDREC_W; x++) {
			int ctx, p;
			i = y * FNDREC_W + x;
			s->q[i] = s->index_of[cur[i]];
			p = fndrec_predict(s, s->have_prev ? tq : NULL, x, y, &ctx);
			fndrec_enc_residual(&e, s, ctx, (int)s->q[i] - p);
		}
	fndrec_enc_flush(&e);
	if (e.overflow)
		return -1;
	for (i = 0; i < FNDREC_N; i++)
		s->flicker[i] = (uint8_t)(s->have_prev && cur[i] != s->prev_value[i]);
	memcpy(s->prev_value, cur, sizeof(cur));
	memcpy(s->prev_present, s->present_now, sizeof(s->prev_present));
	s->have_prev = 1;
	hdr[0] = frame;
	hdr[1] = (uint32_t)(e.p - buf);
	return fwrite(hdr, sizeof(hdr), 1, f) == 1 && fwrite(buf, 1, hdr[1], f) == hdr[1] ? 0 : -1;
}

/* ---- FNDREC2: per frame, Rice codes (read only) */

typedef struct {
	const uint8_t *p, *end;
	uint32_t acc;
	int n;
} fndrec_bits;

static int fndrec_get_bit(fndrec_bits* b)
{
	if (b->n == 0) {
		if (b->p >= b->end)
			return -1;
		b->acc = *b->p++;
		b->n = 8;
	}
	b->n--;
	return (int)((b->acc >> b->n) & 1);
}

static int fndrec_varint(const uint8_t** p, const uint8_t* end, uint32_t* v)
{
	int sh = 0;
	*v = 0;
	while (*p < end && sh < 35) {
		uint8_t c = *(*p)++;
		*v |= (uint32_t)(c & 0x7f) << sh;
		if (!(c & 0x80))
			return 0;
		sh += 7;
	}
	return -1;
}

static int fndrec_read2(const uint8_t* buf, uint32_t len, uint16_t* q, uint16_t* value)
{
	const uint8_t *p = buf, *end = buf + len;
	uint32_t nv, v, i, acc = 0, A[3] = { 4, 4, 4 }, N[3] = { 1, 1, 1 };
	fndrec_bits b;
	int x, y;
	if (fndrec_varint(&p, end, &nv) < 0 || nv == 0 || nv > FNDREC_VMAX)
		return -1;
	for (i = 0; i < nv; i++) {
		if (fndrec_varint(&p, end, &v) < 0 || (acc += v) > 65535)
			return -1;
		value[i] = (uint16_t)acc;
	}
	b.p = p;
	b.end = end;
	b.acc = 0;
	b.n = 0;
	for (y = 0; y < FNDREC_H; y++)
		for (x = 0; x < FNDREC_W; x++) {
			int i2 = y * FNDREC_W + x;
			int a = x ? q[i2 - 1] : (y ? q[i2 - FNDREC_W] : 0);
			int bb = y ? q[i2 - FNDREC_W] : a;
			int c = x && y ? q[i2 - FNDREC_W - 1] : bb;
			int mx = a > bb ? a : bb, mn = a < bb ? a : bb, g = abs(a - c) + abs(bb - c);
			int cx = g == 0 ? 0 : g < 4 ? 1 : 2, pr = c >= mx ? mn : c <= mn ? mx : a + bb - c, k = 0, bit = 0, r;
			uint32_t u, qq = 0;
			while (k < 11 && (N[cx] << k) < A[cx])
				k++;
			while (qq < 20 && (bit = fndrec_get_bit(&b)) == 1)
				qq++;
			if (qq < 20 && bit < 0)
				return -1;
			if (qq == 20) {
				int j;
				u = 0;
				for (j = 0; j < 12; j++) {
					if ((bit = fndrec_get_bit(&b)) < 0)
						return -1;
					u = (u << 1) | (uint32_t)bit;
				}
			} else {
				int j;
				u = qq;
				for (j = 0; j < k; j++) {
					if ((bit = fndrec_get_bit(&b)) < 0)
						return -1;
					u = (u << 1) | (uint32_t)bit;
				}
			}
			r = (int)(u >> 1) ^ -(int)(u & 1);
			if (pr + r < 0 || (uint32_t)(pr + r) >= nv)
				return -1;
			q[i2] = (uint16_t)(pr + r);
			A[cx] += u;
			if (++N[cx] >= 64) {
				A[cx] >>= 1;
				N[cx] >>= 1;
			}
		}
	return 0;
}

// Reads one frame into a 640x480 buffer (each pixel doubled). version is 2
// or 3 (from the magic). 1: a frame, 0: the end, < 0: a damaged file.
static int fndrec_read(FILE* f, fndrec_state* s, int version, uint16_t* depth, uint32_t* frame)
{
	static uint8_t buf[FNDREC_N * 5 + 65536];
	static uint16_t tq[FNDREC_N];
	uint32_t hdr[2];
	int x, y, v, nv = 0;
	if (fread(hdr, sizeof(hdr), 1, f) != 1)
		return 0;
	if (hdr[1] > sizeof(buf))
		return -1;
	if (fread(buf, 1, hdr[1], f) != hdr[1])
		return feof(f) ? 0 : -1; // the last frame cut off (the recorder was stopped): the end
	*frame = hdr[0];
	if (version == 2) {
		if (fndrec_read2(buf, hdr[1], s->q, s->value) < 0)
			return -1;
	} else {
		fndrec_dec d;
		int vmin, vmax;
		fndrec_dec_init(&d, buf, hdr[1]);
		vmin = (int)fndrec_dec_number(&d, s->range_bits[0]);
		vmax = (int)fndrec_dec_number(&d, s->range_bits[1]);
		if (vmin >= FNDREC_VMAX || vmax >= FNDREC_VMAX || vmin > vmax)
			return -1;
		memset(s->present_now, 0, sizeof(s->present_now));
		s->present_now[vmin] = s->present_now[vmax] = 1;
		for (v = vmin + 1; v < vmax; v++)
			s->present_now[v] = (uint8_t)fndrec_dec_bit(&d, &s->present[s->prev_present[v]]);
		for (v = vmin; v <= vmax; v++)
			if (s->present_now[v])
				s->value[nv++] = (uint16_t)v;
		if (s->have_prev)
			fndrec_temporal(s, tq, nv);
		for (y = 0; y < FNDREC_H; y++)
			for (x = 0; x < FNDREC_W; x++) {
				int ctx, p = fndrec_predict(s, s->have_prev ? tq : NULL, x, y, &ctx);
				int q = p + fndrec_dec_residual(&d, s, ctx);
				if (q < 0 || q >= nv)
					return -1;
				s->q[y * FNDREC_W + x] = (uint16_t)q;
			}
		if (d.bad)
			return -1;
		for (x = 0; x < FNDREC_N; x++) {
			uint16_t nvl = s->value[s->q[x]];
			s->flicker[x] = (uint8_t)(s->have_prev && nvl != s->prev_value[x]);
			s->prev_value[x] = nvl;
		}
		memcpy(s->prev_present, s->present_now, sizeof(s->prev_present));
		s->have_prev = 1;
	}
	for (y = 0; y < 480; y++)
		for (x = 0; x < 640; x++)
			depth[y * 640 + x] = s->value[s->q[(y / 2) * FNDREC_W + x / 2]];
	return 1;
}

#endif
