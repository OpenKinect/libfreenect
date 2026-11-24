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
 * A small decoder for zlib streams (RFC 1950 wrapper, RFC 1951 deflate), for
 * the models Kinect10.dll stores compressed. Straightforward canonical
 * Huffman decoding; speed is not a concern (the models load once).
 */

#include <stdlib.h>
#include <string.h>
#include "inflate.h"

typedef struct {
	const uint8_t* in;
	size_t in_len, pos;
	uint32_t bitbuf;
	int bitcnt;
	uint8_t* out;
	size_t out_len, out_cap;
	int error;
} state;

typedef struct {
	uint16_t count[16];   // codes per length
	uint16_t symbol[320]; // symbols ordered by code
} huffman;

static int bits(state* s, int need)
{
	uint32_t v = s->bitbuf;
	while (s->bitcnt < need) {
		if (s->pos >= s->in_len) {
			s->error = 1;
			return 0;
		}
		v |= (uint32_t)s->in[s->pos++] << s->bitcnt;
		s->bitcnt += 8;
	}
	s->bitbuf = v >> need;
	s->bitcnt -= need;
	return (int)(v & ((1u << need) - 1));
}

static int put(state* s, uint8_t b)
{
	if (s->out_len == s->out_cap) {
		size_t cap = s->out_cap ? s->out_cap * 2 : 1 << 16;
		uint8_t* p = (uint8_t*)realloc(s->out, cap);
		if (!p) {
			s->error = 1;
			return -1;
		}
		s->out = p;
		s->out_cap = cap;
	}
	s->out[s->out_len++] = b;
	return 0;
}

static int build(huffman* h, const uint8_t* length, int n)
{
	uint16_t offs[16];
	int len, sym, left = 1;
	memset(h->count, 0, sizeof(h->count));
	for (sym = 0; sym < n; sym++)
		h->count[length[sym]]++;
	if (h->count[0] == n)
		return 0;
	for (len = 1; len < 16; len++) {
		left <<= 1;
		left -= h->count[len];
		if (left < 0)
			return -1; // over-subscribed
	}
	offs[1] = 0;
	for (len = 1; len < 15; len++)
		offs[len + 1] = (uint16_t)(offs[len] + h->count[len]);
	for (sym = 0; sym < n; sym++)
		if (length[sym])
			h->symbol[offs[length[sym]]++] = (uint16_t)sym;
	return left;
}

static int decode(state* s, const huffman* h)
{
	int code = 0, first = 0, index = 0, len;
	for (len = 1; len < 16; len++) {
		code |= bits(s, 1);
		if (s->error)
			return -1;
		{
			int count = h->count[len];
			if (code - count < first)
				return h->symbol[index + (code - first)];
			index += count;
			first += count;
			first <<= 1;
			code <<= 1;
		}
	}
	s->error = 1;
	return -1;
}

static const uint16_t len_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t len_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t dist_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const uint8_t dist_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static int codes(state* s, const huffman* lit, const huffman* dist)
{
	for (;;) {
		int sym = decode(s, lit);
		if (s->error || sym < 0)
			return -1;
		if (sym < 256) {
			if (put(s, (uint8_t)sym) < 0)
				return -1;
		} else if (sym == 256) {
			return 0;
		} else {
			int len, d;
			size_t from;
			sym -= 257;
			if (sym >= 29)
				return -1;
			len = len_base[sym] + bits(s, len_extra[sym]);
			sym = decode(s, dist);
			if (s->error || sym < 0 || sym >= 30)
				return -1;
			d = dist_base[sym] + bits(s, dist_extra[sym]);
			if (s->error || (size_t)d > s->out_len)
				return -1;
			from = s->out_len - (size_t)d;
			while (len--)
				if (put(s, s->out[from++]) < 0)
					return -1;
		}
	}
}

static int stored(state* s)
{
	unsigned len, nlen;
	s->bitbuf = 0;
	s->bitcnt = 0;
	if (s->pos + 4 > s->in_len)
		return -1;
	len = s->in[s->pos] | (s->in[s->pos + 1] << 8);
	nlen = s->in[s->pos + 2] | (s->in[s->pos + 3] << 8);
	s->pos += 4;
	if (len != (~nlen & 0xffffu) || s->pos + len > s->in_len)
		return -1;
	while (len--)
		if (put(s, s->in[s->pos++]) < 0)
			return -1;
	return 0;
}

static int fixed(state* s)
{
	static huffman lit, dist;
	static int ready;
	if (!ready) {
		uint8_t l[320];
		int i;
		for (i = 0; i < 144; i++) l[i] = 8;
		for (; i < 256; i++) l[i] = 9;
		for (; i < 280; i++) l[i] = 7;
		for (; i < 288; i++) l[i] = 8;
		build(&lit, l, 288);
		for (i = 0; i < 30; i++) l[i] = 5;
		build(&dist, l, 30);
		ready = 1;
	}
	return codes(s, &lit, &dist);
}

static int dynamic(state* s)
{
	static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
	uint8_t lengths[320];
	huffman lencode, lit, dist;
	int nlen, ndist, ncode, i;
	nlen = bits(s, 5) + 257;
	ndist = bits(s, 5) + 1;
	ncode = bits(s, 4) + 4;
	if (s->error || nlen > 286 || ndist > 30)
		return -1;
	memset(lengths, 0, sizeof(lengths));
	for (i = 0; i < ncode; i++)
		lengths[order[i]] = (uint8_t)bits(s, 3);
	if (s->error || build(&lencode, lengths, 19) != 0)
		return -1;
	for (i = 0; i < nlen + ndist;) {
		int sym = decode(s, &lencode), rep;
		uint8_t val = 0;
		if (s->error || sym < 0)
			return -1;
		if (sym < 16) {
			lengths[i++] = (uint8_t)sym;
			continue;
		}
		if (sym == 16) {
			if (i == 0)
				return -1;
			val = lengths[i - 1];
			rep = 3 + bits(s, 2);
		} else if (sym == 17) {
			rep = 3 + bits(s, 3);
		} else {
			rep = 11 + bits(s, 7);
		}
		if (s->error || i + rep > nlen + ndist)
			return -1;
		while (rep--)
			lengths[i++] = val;
	}
	if (lengths[256] == 0)
		return -1;
	if (build(&lit, lengths, nlen) < 0 || build(&dist, lengths + nlen, ndist) < 0)
		return -1;
	return codes(s, &lit, &dist);
}

static uint32_t adler32(const uint8_t* p, size_t n)
{
	uint32_t a = 1, b = 0;
	size_t i;
	for (i = 0; i < n; i++) {
		a = (a + p[i]) % 65521u;
		b = (b + a) % 65521u;
	}
	return (b << 16) | a;
}

int fn_zlib_inflate(const uint8_t* in, size_t in_len, uint8_t** out, size_t* out_len)
{
	state s;
	int last;
	memset(&s, 0, sizeof(s));
	*out = NULL;
	*out_len = 0;
	if (in_len < 6 || (in[0] & 0x0f) != 8 || ((in[0] << 8) | in[1]) % 31 != 0 || (in[1] & 0x20))
		return -1;
	s.in = in;
	s.in_len = in_len;
	s.pos = 2;
	do {
		int type;
		last = bits(&s, 1);
		type = bits(&s, 2);
		if (s.error)
			break;
		if (type == 0) {
			if (stored(&s) < 0) s.error = 1;
		} else if (type == 1) {
			if (fixed(&s) < 0) s.error = 1;
		} else if (type == 2) {
			if (dynamic(&s) < 0) s.error = 1;
		} else {
			s.error = 1;
		}
	} while (!last && !s.error);
	if (!s.error) {
		// Adler-32 of the output, big endian, after the deflate data
		uint32_t want;
		if (s.pos + 4 > s.in_len)
			s.error = 1;
		else {
			want = ((uint32_t)in[s.pos] << 24) | ((uint32_t)in[s.pos + 1] << 16) | ((uint32_t)in[s.pos + 2] << 8) | in[s.pos + 3];
			if (want != adler32(s.out, s.out_len))
				s.error = 1;
		}
	}
	if (s.error) {
		free(s.out);
		return -1;
	}
	*out = s.out;
	*out_len = s.out_len;
	return 0;
}
