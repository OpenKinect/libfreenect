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
 * The seated-mode head detector (§30.3): Microsoft's head forest
 * HEADPOSITION.MSHDF on an 80x60 grid, region growing into head candidates,
 * and the SDK's head tracks.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "headforest.h"
#include "inflate.h"

#define NO_DEPTH 6000
#define SEED 80          // probability to seed and grow a region
#define STEP_MM 80       // depth step within a region
#define STACK 255
#define MATCH_M 0.1f     // max match distance
#define START_SCORE 0.05f
#define PROMOTE 10       // hits: tentative -> confirmed
#define DROP 10          // misses: tentative dropped
#define LOSE 30          // misses: confirmed -> lost
#define FORGET 60        // misses: lost freed
#define PX 0.0140041f    // 4 / 285.63: metres per 80x60 cell at 1 m

static const uint8_t format_id[20] = { 0x33, 0x8d, 0x01, 0x5c, 0x74, 0x15, 0x3e, 0xd1, 0x9c, 0x2a,
	                                   0xfd, 0xef, 0x2d, 0x17, 0xc6, 0xeb, 0xda, 0x62, 0x04, 0xb1 };

static uint32_t rd32(const uint8_t* p, int be)
{
	return be ? (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]
	          : (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
}

static int parse(fn_hd_forest* f, const uint8_t* d, size_t len)
{
	static const uint8_t magic[8] = { 'M', 'S', 'H', 'D', 'F', 0, 0, 0 };
	static const uint8_t magic_le[8] = { 0, 0, 0, 'F', 'D', 'H', 'S', 'M' };
	int be, i, n;
	uint32_t shift, ntrees, size;
	float scale, bias;
	memset(f, 0, sizeof(*f));
	if (len < 0x2c)
		return -1;
	if (memcmp(d, magic, 8) == 0)
		be = 1; // as shipped
	else if (memcmp(d, magic_le, 8) == 0)
		be = 0;
	else
		return -1;
	if (rd32(d + 8, be) != 2 || memcmp(d + 12, format_id, 20) != 0)
		return -1;
	shift = rd32(d + 0x20, be);
	ntrees = rd32(d + 0x24, be);
	size = rd32(d + 0x28, be);
	if (shift > 16 || ntrees == 0 || ntrees > 16 || size > len - 0x2c || size % ntrees || (size / ntrees) % 8)
		return -1;
	f->ntrees = (int)ntrees;
	f->nodes_per_tree = (int)(size / ntrees / 8);
	for (f->depth = 0, n = f->nodes_per_tree; n; n >>= 1)
		f->depth++;
	scale = (float)(1u << shift) * 80.0f * 0.003125f; // 256
	bias = scale * -127.0f;
	f->nodes = malloc((size_t)f->ntrees * f->nodes_per_tree * sizeof(fn_hd_node));
	if (!f->nodes)
		return -1;
	for (i = 0; i < f->ntrees * f->nodes_per_tree; i++) {
		const uint8_t* p = d + 0x2c + 8 * (size_t)i;
		uint16_t u = be ? (uint16_t)(p[5] << 8 | p[6]) : (uint16_t)(p[6] << 8 | p[5]);
		fn_hd_node* q = &f->nodes[i];
		q->by = p[0] * scale + bias;
		q->ax = p[1] * scale + bias;
		q->ay = p[2] * scale + bias;
		q->bx = p[3] * scale + bias;
		q->leaf_r = p[4];
		q->thr = (int16_t)((u & 0x3fff) - 0x2000);
		q->two = (u & 0x4000) != 0;
		q->leaf_l = p[7];
	}
	return 0;
}

// as the fetchers install it (zlib, as in Kinect10.dll) or inflated
int fn_hd_forest_load(fn_hd_forest* f, const uint8_t* d, size_t len)
{
	uint8_t* raw;
	size_t n;
	int r;
	if (len >= 8 && (memcmp(d, "MSHDF", 5) == 0 || memcmp(d + 3, "FDHSM", 5) == 0))
		return parse(f, d, len);
	memset(f, 0, sizeof(*f));
	if (fn_zlib_inflate(d, len, &raw, &n) != 0)
		return -1;
	r = parse(f, raw, n);
	free(raw);
	return r;
}

void fn_hd_forest_free(fn_hd_forest* f)
{
	free(f->nodes);
	memset(f, 0, sizeof(*f));
}

/* ------------------------------------------------------------ detection */

static int at(const fn_hd* h, int x, int y)
{
	return x >= 0 && x < FN_HD_W && y >= 0 && y < FN_HD_H ? h->D[y * FN_HD_W + x] : NO_DEPTH;
}

static uint8_t evaluate(const fn_hd* h, int x, int y)
{
	const fn_hd_forest* f = &h->forest;
	int d = h->D[y * FN_HD_W + x], t, k, sum = 0;
	float inv = 1.0f / (float)d;
	for (t = 0; t < f->ntrees; t++) {
		const fn_hd_node* tree = f->nodes + (size_t)t * f->nodes_per_tree;
		int i = 0, last = 0, bit = 0;
		for (k = 0; k < f->depth && i < f->nodes_per_tree; k++) {
			const fn_hd_node* n = &tree[i];
			int a = at(h, x + (int)(n->ax * inv), y + (int)(n->ay * inv));
			int b = n->two ? at(h, x + (int)(n->bx * inv), y + (int)(n->by * inv)) : d;
			bit = n->thr <= a - b;
			last = i;
			i = 2 * i + 1 + bit;
		}
		sum += bit ? tree[last].leaf_r : tree[last].leaf_l;
	}
	return (uint8_t)((float)sum * (1.0f / (float)f->ntrees) + 0.5f);
}

static void blur(fn_hd* h)
{
	static uint8_t t[FN_HD_W * FN_HD_H];
	int x, y;
	memcpy(t, h->P, sizeof(t));
	for (y = 0; y < FN_HD_H; y++)
		for (x = 1; x < FN_HD_W - 1; x++) {
			const uint8_t* r = h->P + y * FN_HD_W;
			t[y * FN_HD_W + x] = (uint8_t)((r[x - 1] + 2 * r[x] + r[x + 1]) >> 2);
		}
	memcpy(h->B, t, sizeof(t));
	for (y = 1; y < FN_HD_H - 1; y++)
		for (x = 0; x < FN_HD_W; x++)
			h->B[y * FN_HD_W + x] = (uint8_t)((t[(y - 1) * FN_HD_W + x] + 2 * t[y * FN_HD_W + x] + t[(y + 1) * FN_HD_W + x]) >> 2);
}

static void candidate(fn_hd* h, double sx, double sy, double sz, double W, const int box[4])
{
	fn_hd_candidate* c;
	double cx = sx / W, cy = sy / W, z = sz / (W * 1000.0), mean = 0, var = 0, s, hexp, wexp, dh, dw;
	int x, y, n = 0, bw = box[2] - box[0] + 1, bh = box[3] - box[1] + 1;
	if (h->ncand >= FN_HD_POOL || z <= 0)
		return;
	c = &h->cand[h->ncand++];
	c->pos = v3make((float)((cx - 40.0) * z * PX), (float)((30.0 - cy) * z * PX), (float)z);
	memcpy(c->box, box, sizeof(c->box));
	c->mw = (float)(bw * z * PX);
	c->mh = (float)(bh * z * PX);
	// score: mass over the probability's spread in the box (unblurred)
	for (y = box[1]; y <= box[3]; y++)
		for (x = box[0]; x <= box[2]; x++, n++)
			mean += h->P[y * FN_HD_W + x];
	mean /= n;
	for (y = box[1]; y <= box[3]; y++)
		for (x = box[0]; x <= box[2]; x++)
			var += (h->P[y * FN_HD_W + x] - mean) * (h->P[y * FN_HD_W + x] - mean);
	s = sqrt(var / n);
	c->score = (float)(W * z * 0.001 / (s > 1.0 ? s : 1.0));
	// size prior (mode 1): a head 0.138 m wide, twice as tall as wide in cells
	s = 0.138 / z * 71.40747;
	hexp = (int)(s * 2.0 + 0.5);
	wexp = (int)(s * 1.5 + 0.5);
	if (hexp < 1) hexp = 1;
	if (wexp < 1) wexp = 1;
	dh = fabs(hexp - bh) < hexp ? fabs(hexp - bh) : hexp;
	dw = fabs(wexp - bw) < wexp ? fabs(wexp - bw) : wexp;
	c->score *= (float)((1.0 - dh / hexp) * (1.0 - dw / wexp));
}

void fn_hd_detect(fn_hd* h, const uint16_t* map)
{
	static int stack[STACK];
	int x, y;
	h->ncand = 0;
	if (!h->forest.nodes)
		return;
	// 1. 4x4 min-pool of the 320x240 map; 0 is "no depth"
	for (y = 0; y < FN_HD_H; y++)
		for (x = 0; x < FN_HD_W; x++) {
			int m = NO_DEPTH, i, j;
			for (j = 0; j < 4; j++)
				for (i = 0; i < 4; i++) {
					int v = map[(4 * y + j) * NW + 4 * x + i];
					if (v == 0)
						v = NO_DEPTH;
					if (v < m)
						m = v;
				}
			h->D[y * FN_HD_W + x] = (uint16_t)m;
		}
	// 2. the forest at every cell with depth
	for (y = 0; y < FN_HD_H; y++)
		for (x = 0; x < FN_HD_W; x++)
			h->P[y * FN_HD_W + x] = h->D[y * FN_HD_W + x] == NO_DEPTH ? 0 : evaluate(h, x, y);
	// 3. blur
	blur(h);
	// 4. regions of probable head
	for (y = 0; y < FN_HD_H; y++)
		for (x = 0; x < FN_HD_W; x++) {
			double sx = 0, sy = 0, sz = 0, W = 0;
			int box[4] = { x, y, x, y }, sp = 0;
			if (h->B[y * FN_HD_W + x] < SEED)
				continue;
			stack[sp++] = y * FN_HD_W + x;
			while (sp > 0) {
				int c = stack[--sp], cx = c % FN_HD_W, cy = c / FN_HD_W, k, w = h->B[c];
				static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
				if (w < SEED)
					continue; // reached twice
				h->B[c] = 0;
				sx += (double)cx * w;
				sy += (double)cy * w;
				sz += (double)h->D[c] * w;
				W += w;
				if (cx < box[0]) box[0] = cx;
				if (cy < box[1]) box[1] = cy;
				if (cx > box[2]) box[2] = cx;
				if (cy > box[3]) box[3] = cy;
				for (k = 0; k < 4; k++) {
					int nx = cx + dx[k], ny = cy + dy[k], nc;
					if (nx < 0 || nx >= FN_HD_W || ny < 0 || ny >= FN_HD_H)
						continue;
					nc = ny * FN_HD_W + nx;
					if (h->B[nc] >= SEED && abs((int)h->D[nc] - (int)h->D[c]) < STEP_MM && sp < STACK)
						stack[sp++] = nc;
				}
			}
			if (box[2] > box[0] && box[3] > box[1] && W > 0)
				candidate(h, sx, sy, sz, W, box);
		}
}

/* ------------------------------------------------------------ tracking */

static int boxes_meet(const int a[4], const int b[4])
{
	return a[0] <= b[2] && b[0] <= a[2] && a[1] <= b[3] && b[1] <= a[3];
}

static float distance(const fn_hd_track* t, const fn_hd_candidate* c)
{
	float d3 = fn_nui_v3len(v3sub(t->pos, c->pos)), dz = fabsf(t->pos.z - c->pos.z);
	return boxes_meet(t->box, c->box) && dz < d3 ? dz : d3;
}

static void take(fn_hd_track* t, const fn_hd_candidate* c)
{
	float sum = 0;
	int i;
	t->pos = c->pos;
	memcpy(t->box, c->box, sizeof(t->box));
	t->hist[t->hpos] = c->score;
	t->hpos = (t->hpos + 1) % 15;
	if (t->nhist < 15)
		t->nhist++;
	for (i = 0; i < t->nhist; i++)
		sum += t->hist[i];
	t->score = (t->score + sum) * (1.0f / 15.0f); // as the SDK: the previous score is added too
	t->hits++;
	t->misses = 0;
	t->matched = 1;
}

static int free_slot(const fn_hd* h)
{
	int s, i, used;
	for (s = 0; s < FN_HD_SLOTS; s++) {
		used = 0;
		for (i = 0; i < FN_HD_POOL; i++)
			if (h->track[i].state >= 2 && h->track[i].slot == s)
				used = 1;
		if (!used)
			return s;
	}
	return -1;
}

void fn_hd_track_update(fn_hd* h)
{
	int i, j, used[FN_HD_POOL];
	memset(used, 0, sizeof(used));
	for (i = 0; i < FN_HD_POOL; i++)
		h->track[i].matched = 0;
	// greedy: the closest pairs first, each track and candidate once
	for (;;) {
		int bi = -1, bj = -1;
		float bd = MATCH_M;
		for (i = 0; i < FN_HD_POOL; i++) {
			if (!h->track[i].state || h->track[i].matched)
				continue;
			for (j = 0; j < h->ncand; j++) {
				float d;
				if (used[j])
					continue;
				d = distance(&h->track[i], &h->cand[j]);
				if (d < bd) {
					bd = d;
					bi = i;
					bj = j;
				}
			}
		}
		if (bi < 0)
			break;
		take(&h->track[bi], &h->cand[bj]);
		used[bj] = 1;
	}
	// unmatched candidates: new tentative tracks
	for (j = 0; j < h->ncand; j++) {
		const fn_hd_candidate* c = &h->cand[j];
		if (used[j] || c->score < START_SCORE || c->mw < 0.04f || c->mw > 0.5f || c->mh < 0.04f || c->mh > 0.5f)
			continue;
		for (i = 0; i < FN_HD_POOL && h->track[i].state; i++)
			;
		if (i == FN_HD_POOL)
			break;
		memset(&h->track[i], 0, sizeof(h->track[i]));
		h->track[i].state = 1;
		h->track[i].slot = -1;
		take(&h->track[i], c);
		h->track[i].hits = 1;
	}
	// misses and the life cycle
	for (i = 0; i < FN_HD_POOL; i++) {
		fn_hd_track* t = &h->track[i];
		if (!t->state)
			continue;
		if (!t->matched)
			t->misses++;
		if (t->state == 1) {
			if (t->misses > DROP) {
				t->state = 0;
			} else if (t->hits > PROMOTE) {
				// back from the lost list if one is near; else a new head
				int k, re = -1;
				for (k = 0; k < FN_HD_POOL; k++)
					if (h->track[k].state == 3 && fn_nui_v3len(v3sub(h->track[k].pos, t->pos)) < MATCH_M)
						re = k;
				if (re >= 0) {
					t->id = h->track[re].id;
					t->slot = h->track[re].slot;
					h->track[re].state = 0;
				} else {
					t->slot = free_slot(h);
					if (t->slot < 0) {
						t->state = 0;
						continue;
					}
					t->id = ++h->next_id;
				}
				t->state = 2;
			}
		} else if (t->state == 2) {
			int k;
			if (t->misses > LOSE) {
				t->state = 3;
				continue;
			}
			for (k = 0; k < FN_HD_POOL; k++)
				if (h->track[k].state == 3 && fn_nui_v3len(v3sub(h->track[k].pos, t->pos)) < MATCH_M)
					h->track[k].state = 0;
		} else if (t->state == 3 && t->misses > FORGET) {
			t->state = 0;
		}
	}
}

int fn_hd_output(const fn_hd* h, freenect_nui_head out[FN_HD_SLOTS])
{
	int order[FN_HD_POOL], n = 0, i, j, rank = 0;
	memset(out, 0, FN_HD_SLOTS * sizeof(freenect_nui_head));
	for (i = 0; i < FN_HD_POOL; i++)
		if (h->track[i].state >= 2 && h->track[i].slot >= 0)
			order[n++] = i;
	// confirmed before lost, then by score
	for (i = 1; i < n; i++)
		for (j = i; j > 0; j--) {
			const fn_hd_track *a = &h->track[order[j - 1]], *b = &h->track[order[j]];
			int swap = a->state != b->state ? a->state > b->state : a->score < b->score;
			int t;
			if (!swap)
				break;
			t = order[j];
			order[j] = order[j - 1];
			order[j - 1] = t;
		}
	for (i = 0; i < n; i++) {
		const fn_hd_track* t = &h->track[order[i]];
		freenect_nui_head* o = &out[t->slot];
		o->id = t->id;
		o->observed = t->misses == 0;
		o->rank = ++rank;
		o->score = t->score;
		o->position.x = t->pos.x;
		o->position.y = t->pos.y;
		o->position.z = t->pos.z;
		// 80x60 cells of the mirrored map -> 640x480 depth pixels, libfreenect's orientation
		o->bbox[0] = 639 - (8 * t->box[2] + 7);
		o->bbox[1] = 8 * t->box[1];
		o->bbox[2] = 639 - 8 * t->box[0];
		o->bbox[3] = 8 * t->box[3] + 7;
	}
	return n;
}
