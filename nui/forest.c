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

#include <stdlib.h>
#include <string.h>
#include "forest.h"
#include "inflate.h"

typedef struct {
	int32_t child[2];          // >= 0: split node; < 0: leaf -1 - child
	int8_t ux, uy, vx, vy;
	int16_t thr;
	int16_t pad;
} split;

typedef struct {
	uint8_t cls[5];
	uint8_t prob[5];
} leaf;

struct fn_forest {
	int ntrees;
	split* tree[4];
	int nsplit[4];
	leaf* leaves;
	uint32_t nleaves;
};

static uint32_t le32(const uint8_t* p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// The file layout (§30.2.2): "MSODF   ", version 0x72, 3 skipped words,
// nTrees, then per tree {nNodes, nBytes, 6-byte records in heap order}, then
// the shared leaf table of 8-byte records.
static fn_forest* parse(const uint8_t* d, size_t n)
{
	fn_forest* f;
	size_t pos = 0x1c;
	uint32_t t, i;
	if (n < 0x1c || memcmp(d, "MSODF   ", 8) != 0 || le32(d + 8) != 0x72 || le32(d + 0x18) >= 4 || le32(d + 0x18) == 0)
		return NULL;
	f = (fn_forest*)calloc(1, sizeof(*f));
	if (!f)
		return NULL;
	f->ntrees = (int)le32(d + 0x18);
	for (t = 0; t < (uint32_t)f->ntrees; t++) {
		uint32_t nnodes, nbytes, half;
		const uint8_t* rec;
		if (pos + 8 > n)
			goto bad;
		nnodes = le32(d + pos);
		nbytes = le32(d + pos + 4);
		pos += 8;
		if (nnodes < 3 || (uint64_t)nnodes * 6 > nbytes || pos + nbytes > n)
			goto bad;
		rec = d + pos;
		half = nnodes >> 1;
		f->nsplit[t] = (int)half;
		f->tree[t] = (split*)calloc(half, sizeof(split));
		if (!f->tree[t])
			goto bad;
		for (i = 0; i < nnodes; i++) {
			const uint8_t* b = rec + 6 * (size_t)i;
			unsigned w = b[4] | (b[5] << 8);
			int32_t link;
			if ((w & 0x8000) || i >= half) {
				link = -1 - (int32_t)le32(b);
			} else {
				split* s = &f->tree[t][i];
				int thr = (int)(w & 0x3fff);
				if (thr & 0x2000)
					thr -= 0x4000;
				s->ux = (int8_t)(b[0] - 0x7f);
				s->uy = (int8_t)(b[1] - 0x7f);
				s->vx = (w & 0x4000) ? (int8_t)(b[2] - 0x7f) : 0;
				s->vy = (w & 0x4000) ? (int8_t)(b[3] - 0x7f) : 0;
				s->thr = (int16_t)thr;
				link = (int32_t)i;
			}
			if (i > 0)
				f->tree[t][(i - 1) / 2].child[(i & 1) ? 0 : 1] = link;
		}
		pos += nbytes;
	}
	f->nleaves = (uint32_t)((n - pos) / 8);
	f->leaves = (leaf*)malloc(sizeof(leaf) * (f->nleaves ? f->nleaves : 1));
	if (!f->leaves)
		goto bad;
	for (i = 0; i < f->nleaves; i++) {
		const uint8_t* b = d + pos + 8 * (size_t)i;
		leaf* l = &f->leaves[i];
		uint8_t b4 = b[4], b5 = b[5], b6 = b[6], b7 = b[7];
		l->cls[0] = (uint8_t)(((b5 & 15) << 1) | (b5 >> 7));
		l->cls[1] = (uint8_t)(((b6 >> 4) << 1) | ((b5 >> 6) & 1));
		l->cls[2] = (uint8_t)(((b6 & 15) << 1) | ((b5 >> 5) & 1));
		l->cls[3] = (uint8_t)(((b7 >> 4) << 1) | ((b5 >> 4) & 1));
		l->cls[4] = (uint8_t)(((b7 & 15) << 1) | (b4 & 1));
		l->prob[0] = b[0];
		l->prob[1] = b[1];
		l->prob[2] = b[2];
		l->prob[3] = b[3];
		l->prob[4] = (uint8_t)(b4 & 0xfe);
		{
			int k;
			for (k = 0; k < 5; k++)
				if (l->cls[k] >= FN_NUI_PARTS)
					goto bad;
		}
	}
	// every leaf reference must be inside the table
	for (t = 0; t < (uint32_t)f->ntrees; t++)
		for (i = 0; i < (uint32_t)f->nsplit[t]; i++) {
			int c;
			for (c = 0; c < 2; c++) {
				int32_t link = f->tree[t][i].child[c];
				if ((link < 0 && (uint32_t)(-1 - link) >= f->nleaves) || link >= f->nsplit[t])
					f->tree[t][i].child[c] = -1; // unreachable padding; points at leaf 0
			}
		}
	return f;
bad:
	fn_forest_free(f);
	return NULL;
}

fn_forest* fn_forest_load(const uint8_t* data, size_t len)
{
	fn_forest* f;
	uint8_t* raw;
	size_t n;
	if (len >= 8 && memcmp(data, "MSODF   ", 8) == 0)
		return parse(data, len);
	if (fn_zlib_inflate(data, len, &raw, &n) != 0)
		return NULL;
	f = parse(raw, n);
	free(raw);
	return f;
}

void fn_forest_free(fn_forest* f)
{
	int t;
	if (!f)
		return;
	for (t = 0; t < 4; t++)
		free(f->tree[t]);
	free(f->leaves);
	free(f);
}

int fn_forest_trees(const fn_forest* f)
{
	return f ? f->ntrees : 0;
}

// probe (§30.2.3): the depth of the pixel at the scaled offset if it belongs
// to the same tracker slot, else 6000 mm.
static int probe(const uint16_t* map, int x, int y, int ox, int oy, float s, int slot)
{
	int px = x + (int)((float)ox * s), py = y + (int)((float)oy * s);
	uint16_t v;
	if (px < 0 || px >= FN_NUI_MAP_W || py < 0 || py >= FN_NUI_MAP_H)
		return FN_NUI_BACKGROUND_MM;
	v = map[py * FN_NUI_MAP_W + px];
	return (v & 3) == slot ? (int)(v >> 2) : FN_NUI_BACKGROUND_MM;
}

void fn_forest_classify(const fn_forest* f, const uint16_t* map, const float scale[3], int x, int y,
                        uint8_t out[FN_NUI_PARTS])
{
	uint16_t v = map[y * FN_NUI_MAP_W + x];
	int slot = v & 3, d = v >> 2, t, c;
	uint16_t acc[FN_NUI_PARTS];
	float s, inv;
	if (!slot || !d) {
		memset(out, 8, FN_NUI_PARTS);
		return;
	}
	s = scale[slot - 1] * 2048.0f / (float)d;
	memset(acc, 0, sizeof(acc));
	for (t = 0; t < f->ntrees; t++) {
		int32_t n = 0;
		const leaf* l;
		while (n >= 0) {
			const split* N = &f->tree[t][n];
			int a = probe(map, x, y, N->ux, N->uy, s, slot);
			int b = (N->vx || N->vy) ? probe(map, x, y, N->vx, N->vy, s, slot) : d;
			n = (a - b < N->thr) ? N->child[0] : N->child[1];
		}
		l = &f->leaves[-1 - n];
		for (c = 0; c < 5; c++)
			acc[l->cls[c]] = (uint16_t)(acc[l->cls[c]] + l->prob[c]);
	}
	inv = 1.0f / (float)f->ntrees;
	for (c = 0; c < FN_NUI_PARTS; c++)
		out[c] = (uint8_t)(int64_t)((float)acc[c] * inv);
}

void fn_forest_label(const fn_forest* f, const uint16_t* map, const float scale[3], uint8_t* out)
{
	int i, j;
	for (j = 0; j < FN_NUI_LABEL_H; j++)
		for (i = 0; i < FN_NUI_LABEL_W; i++)
			fn_forest_classify(f, map, scale, 2 * i, 2 * j, out + ((size_t)j * FN_NUI_LABEL_W + i) * FN_NUI_PARTS);
}
