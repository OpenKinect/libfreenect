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
 * Joint proposals: for each joint, a weighted mean shift over the 3-D points
 * of the user's pixels (Shotton et al. 2011), with weights P(part) z^2 so
 * that each square metre of body counts the same at any distance.
 *
 * - The joints come from Kinect10's parts (§30.5.4.2): HEAD from parts 0+1,
 *   SHOULDER_CENTER from 2, the shoulders, elbows, wrists, knees and ankles
 *   from 21-30, hands from 11/14, hips from 8/9, feet from 17/18; SPINE is
 *   derived in the fit.
 * - Points are pooled in 4x4-pixel cells first (as Kinect10's centroid
 *   stage), and probabilities of 4/255 or less are dropped (§30.5.4.1.1).
 * - Bandwidth 65 mm (45 mm for hands and wrists).
 * - Confidence as Kinect10's: mean probability around the mode times
 *   min(pixels / 32, 1).
 * - Each candidate is pushed back along +z by half of Kinect10's per-joint
 *   table (§30.5.5.6), scaled by the user's size: the parts are seen on the
 *   body's surface, the joints are inside.
 */

#include <math.h>
#include <string.h>
#include "nui_internal.h"

#define CELL 4
#define CW (NW / CELL)
#define CH (NH / CELL)
#define MAX_SEEDS 48

static const signed char joint_parts[NJ][3] = {
	{ 8, 9, -1 },   // HIP_CENTER
	{ -1, -1, -1 }, // SPINE (derived)
	{ 2, -1, -1 },  // SHOULDER_CENTER
	{ 0, 1, -1 },   // HEAD
	{ 21, -1, -1 }, { 23, -1, -1 }, { 27, -1, -1 }, { 11, -1, -1 },  // left arm
	{ 22, -1, -1 }, { 24, -1, -1 }, { 28, -1, -1 }, { 14, -1, -1 },  // right arm
	{ 8, -1, -1 }, { 25, -1, -1 }, { 29, -1, -1 }, { 17, -1, -1 },   // left leg
	{ 9, -1, -1 }, { 26, -1, -1 }, { 30, -1, -1 }, { 18, -1, -1 },   // right leg
};

// §30.5.5.6, metres
static const float pushback[NJ] = {
	.160f, .150f, .133f, .213f, .133f, .080f, .053f, .053f, .133f, .080f,
	.053f, .053f, .160f, .133f, .080f, .080f, .160f, .133f, .080f, .080f,
};

typedef struct {
	v3 p;         // weighted mean position
	float w;      // sum of p z^2
	float psum;   // sum of p
	int n;        // pixels with p > 4/255
} cell;

void fn_nui_proposals(const uint16_t* map, const uint8_t* mask, int slot, const uint8_t* prob,
                      float scale, fn_nui_props* out)
{
	static cell cells[CW * CH];
	int j;
	memset(out, 0, sizeof(*out));
	for (j = 0; j < NJ; j++) {
		int ncell = 0, cx, cy, k, nseeds = 0, nmodes = 0;
		float b = (j == 6 || j == 7 || j == 10 || j == 11) ? 0.045f : 0.065f, b2;
		fn_nui_cand modes[MAX_SEEDS];
		int seeds[MAX_SEEDS];
		if (joint_parts[j][0] < 0)
			continue;
		b2 = b * b;
		// pool the user's pixels into 4x4 cells
		for (cy = 0; cy < CH; cy++)
			for (cx = 0; cx < CW; cx++) {
				cell c;
				int x, y;
				double sx = 0, sy = 0, sz = 0;
				memset(&c, 0, sizeof(c));
				for (y = cy * CELL; y < cy * CELL + CELL; y++)
					for (x = cx * CELL; x < cx * CELL + CELL; x++) {
						int i = y * NW + x, q;
						float p = 0.0f, w;
						v3 pt;
						if (mask[i] != slot + 1 || !map[i])
							continue;
						for (q = 0; q < 3 && joint_parts[j][q] >= 0; q++)
							p += prob[(size_t)i * FN_NUI_PARTS + joint_parts[j][q]];
						if (p <= 4.0f)
							continue;
						p = p > 255.0f ? 1.0f : p / 255.0f;
						pt = fn_nui_unproject((float)x, (float)y, (float)map[i]);
						w = p * pt.z * pt.z;
						sx += w * pt.x; sy += w * pt.y; sz += w * pt.z;
						c.w += w;
						c.psum += p;
						c.n++;
					}
				if (c.n == 0)
					continue;
				c.p = v3make((float)(sx / c.w), (float)(sy / c.w), (float)(sz / c.w));
				cells[ncell] = c;
				ncell++;
			}
		if (!ncell)
			continue;
		// seeds: the cells with mean probability above 0.14, strongest first
		for (k = 0; k < ncell; k++) {
			int s, pos;
			if (cells[k].psum / (float)cells[k].n < 0.14f)
				continue;
			for (pos = nseeds; pos > 0 && cells[seeds[pos - 1]].w < cells[k].w; pos--)
				;
			if (pos >= MAX_SEEDS)
				continue;
			for (s = (nseeds < MAX_SEEDS ? nseeds : MAX_SEEDS - 1); s > pos; s--)
				seeds[s] = seeds[s - 1];
			seeds[pos] = k;
			if (nseeds < MAX_SEEDS)
				nseeds++;
		}
		// mean shift from each seed
		for (k = 0; k < nseeds; k++) {
			v3 m = cells[seeds[k]].p;
			int it, c, dup = 0;
			float f = 0.0f, ps = 0.0f;
			int n = 0;
			for (it = 0; it < 12; it++) {
				double sx = 0, sy = 0, sz = 0, sw = 0;
				v3 nm;
				for (c = 0; c < ncell; c++) {
					v3 d = v3sub(cells[c].p, m);
					float r2 = v3dot(d, d), w;
					if (r2 > 9.0f * b2)
						continue;
					w = cells[c].w * expf(-r2 / b2);
					sx += w * cells[c].p.x; sy += w * cells[c].p.y; sz += w * cells[c].p.z;
					sw += w;
				}
				if (sw <= 0.0)
					break;
				nm = v3make((float)(sx / sw), (float)(sy / sw), (float)(sz / sw));
				if (fn_nui_v3len(v3sub(nm, m)) < 0.001f) {
					m = nm;
					break;
				}
				m = nm;
			}
			// support of the mode: pixels within the bandwidth
			for (c = 0; c < ncell; c++) {
				v3 d = v3sub(cells[c].p, m);
				if (v3dot(d, d) <= b2) {
					ps += cells[c].psum;
					n += cells[c].n;
					f += cells[c].w;
				}
			}
			if (n == 0)
				continue;
			for (c = 0; c < nmodes; c++)
				if (fn_nui_v3len(v3sub(modes[c].p, m)) < b) {
					dup = 1;
					break;
				}
			if (dup)
				continue;
			modes[nmodes].p = m;
			modes[nmodes].conf = (ps / (float)n) * (n >= 32 ? 1.0f : (float)n / 32.0f);
			nmodes++;
		}
		// strongest first, at most FN_NUI_MAX_CAND, pushed back
		for (k = 0; k < nmodes; k++) {
			int a, best = k;
			for (a = k + 1; a < nmodes; a++)
				if (modes[a].conf > modes[best].conf)
					best = a;
			if (best != k) {
				fn_nui_cand t = modes[k];
				modes[k] = modes[best];
				modes[best] = t;
			}
			if (k < FN_NUI_MAX_CAND) {
				fn_nui_cand c = modes[k];
				c.p.z += 0.5f * pushback[j] * scale;
				out->c[j][k] = c;
				out->n[j] = k + 1;
			}
		}
	}
}
