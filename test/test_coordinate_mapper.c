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
 * Host-only test of the coordinate mapper (src/coordinate_mapper.c) against
 * Microsoft's runtime: the vectors in mapper_vectors.h come from the
 * reference's tools/kinect10_depth_ref.py, which emulates Kinect10's single
 * precision operation by operation, and must match to the bit.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libfreenect_coordinate_mapper.h"
#include "freenect_internal.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-64s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

#include "mapper_vectors.h"

/* The mapper's internals, for the x shift table. */
#include "../src/coordinate_mapper.c"

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

static uint32_t fbits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float bitsf(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

static freenect_coordinate_mapper* mapper_for(int set)
{
	uint8_t blob[FREENECT_CALIBRATION_SIZE];
	int i;
	memset(blob, 0, sizeof(blob));
	memcpy(blob, "FNC1", 4);
	blob[4] = 1;
	for (i = 0; i < 29; i++) {
		uint32_t v = (uint32_t)cals[set].reg[i];
		blob[8 + 4 * i] = (uint8_t)v; blob[9 + 4 * i] = (uint8_t)(v >> 8);
		blob[10 + 4 * i] = (uint8_t)(v >> 16); blob[11 + 4 * i] = (uint8_t)(v >> 24);
	}
	for (i = 0; i < 4; i++) {
		uint32_t v = fbits(cals[set].zpi[i]);
		blob[132 + 4 * i] = (uint8_t)v; blob[133 + 4 * i] = (uint8_t)(v >> 8);
		blob[134 + 4 * i] = (uint8_t)(v >> 16); blob[135 + 4 * i] = (uint8_t)(v >> 24);
	}
	blob[148] = 0x20; blob[149] = 0x03; /* const shift 800 */
	return freenect_create_coordinate_mapper_from_calibration(blob, sizeof(blob));
}

static int wid(freenect_resolution r) { return r == FREENECT_RESOLUTION_VERY_LOW ? 80 : r == FREENECT_RESOLUTION_LOW ? 320 : r == FREENECT_RESOLUTION_HIGH ? 1280 : 640; }

int main(void)
{
	freenect_coordinate_mapper* m[N(cals)];
	int i, ok, bad;

	printf("Coordinate mapper (src/coordinate_mapper.c)\n");
	for (i = 0; i < N(cals); i++)
		m[i] = mapper_for(i);
	CHECK(m[0] && m[1] && m[2] && m[3] && m[4], "mappers from saved calibrations");

	/* calibration: saved and loaded unchanged; anything else refused */
	{
		uint8_t a[FREENECT_CALIBRATION_SIZE], b[FREENECT_CALIBRATION_SIZE];
		freenect_coordinate_mapper* c;
		CHECK(freenect_mapper_get_calibration(m[1], a, sizeof(a)) == FREENECT_CALIBRATION_SIZE, "calibration saved");
		c = freenect_create_coordinate_mapper_from_calibration(a, sizeof(a));
		CHECK(c && freenect_mapper_get_calibration(c, b, sizeof(b)) == FREENECT_CALIBRATION_SIZE &&
		      memcmp(a, b, sizeof(a)) == 0, "  and loaded again unchanged");
		CHECK(c && c->const_shift == 800 && memcmp(c->X, m[1]->X, sizeof(c->X)) == 0, "  with the same coefficients");
		freenect_destroy_coordinate_mapper(c);
		CHECK(freenect_mapper_get_calibration(m[1], a, sizeof(a) - 1) == -1, "  buffer too small: -1");
		CHECK(freenect_create_coordinate_mapper_from_calibration(a, sizeof(a) - 1) == NULL, "  too short: refused");
		a[0] = 'X';
		CHECK(freenect_create_coordinate_mapper_from_calibration(a, sizeof(a)) == NULL, "  not a calibration: refused");
		a[0] = 'F'; a[4] = 2;
		CHECK(freenect_create_coordinate_mapper_from_calibration(a, sizeof(a)) == NULL, "  unknown version: refused");
	}

	/* x shift by depth (§20.5.1) */
	for (i = 0, bad = 0; i < N(shifts); i++)
		if (m[shifts[i].set]->shift[shifts[i].i] != (float)shifts[i].sh) {
			if (!bad++) printf("    set %d depth %d: %g, expected %d\n", shifts[i].set, shifts[i].i,
			                   m[shifts[i].set]->shift[shifts[i].i], shifts[i].sh);
		}
	CHECK(bad == 0, "x shift by depth: as Kinect10");

	/* depth point -> colour (§20.5.2), mirrored as the runtime */
	for (i = 0; i < N(cals); i++)
		freenect_mapper_set_mirrored(m[i], 1);
	for (i = 0, bad = 0; i < N(d2c); i++) {
		const d2c_vec* v = &d2c[i];
		freenect_depth_point p;
		freenect_color_point c;
		p.x = v->x; p.y = v->y; p.depth_mm = (uint16_t)v->d;
		if (freenect_map_depth_point_to_color(m[v->set], (freenect_resolution)v->dres, (freenect_resolution)v->cres, p, &c) != 0 ||
		    c.x != v->cx || c.y != v->cy) {
			if (!bad++) printf("    set %d %dx: (%d,%d) %d mm -> (%d,%d), expected (%d,%d)\n", v->set, wid((freenect_resolution)v->dres),
			                   v->x, v->y, v->d, c.x, c.y, v->cx, v->cy);
		}
	}
	printf("  (%d depth -> colour points)\n", N(d2c));
	CHECK(bad == 0, "depth point -> colour: as Kinect10, to the pixel");

	/* unmirrored: the same with x' = width - 1 - x on both sides */
	for (i = 0, ok = 1; i < N(d2c); i++) {
		const d2c_vec* v = &d2c[i];
		freenect_depth_point p;
		freenect_color_point c;
		if (v->d == 0)
			continue;
		freenect_mapper_set_mirrored(m[v->set], 0);
		p.x = wid((freenect_resolution)v->dres) - 1 - v->x; p.y = v->y; p.depth_mm = (uint16_t)v->d;
		freenect_map_depth_point_to_color(m[v->set], (freenect_resolution)v->dres, (freenect_resolution)v->cres, p, &c);
		ok &= c.x == wid((freenect_resolution)v->cres) - 1 - v->cx && c.y == v->cy;
		freenect_mapper_set_mirrored(m[v->set], 1);
	}
	CHECK(ok, "  unmirrored: the mirror image of it");

	/* depth frame -> colour agrees with the point version inside the image */
	{
		static uint16_t depth[640 * 480];
		static freenect_color_point fc[640 * 480];
		int x, y, cmp = 0;
		for (y = 0; y < 480; y++)
			for (x = 0; x < 640; x++)
				depth[y * 640 + x] = (uint16_t)(x % 97 == 0 ? 0 : 700 + (x * 7 + y * 13) % 3000);
		ok = 1;
		for (i = 0; i < 2; i++) {
			freenect_mapper_set_mirrored(m[2], i);
			freenect_map_depth_frame_to_color(m[2], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_MEDIUM, depth, fc);
			for (y = 0; y < 480; y += 7)
				for (x = 0; x < 640; x += 5) {
					freenect_depth_point p;
					freenect_color_point c;
					p.x = x; p.y = y; p.depth_mm = depth[y * 640 + x];
					freenect_map_depth_point_to_color(m[2], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_MEDIUM, p, &c);
					if (p.depth_mm == 0) {
						ok &= fc[y * 640 + x].x == FREENECT_MAPPER_INVALID;
						continue;
					}
					if (c.x < 1 || c.x >= 639 || c.y < 0 || c.y >= 480)
						continue;
					cmp++;
					ok &= fc[y * 640 + x].x == c.x && fc[y * 640 + x].y == c.y;
				}
		}
		printf("  (%d pixels compared)\n", cmp);
		CHECK(ok && cmp > 1000, "depth frame -> colour: the point version's pixels, both orientations");
	}

	/* camera space (§20.5.4) */
	for (i = 0, ok = 1; i < N(d2s); i++) {
		freenect_depth_point p;
		freenect_camera_point s;
		p.x = d2s[i].x; p.y = d2s[i].y; p.depth_mm = (uint16_t)d2s[i].d;
		freenect_map_depth_point_to_camera(m[0], (freenect_resolution)d2s[i].dres, p, &s);
		ok &= fbits(s.x) == d2s[i].X && fbits(s.y) == d2s[i].Y && fbits(s.z) == d2s[i].Z;
	}
	CHECK(ok, "depth point -> camera space: as Kinect10, to the bit");
	for (i = 0, ok = 1; i < N(s2d); i++) {
		freenect_camera_point s;
		freenect_depth_point p;
		s.x = bitsf(s2d[i].X); s.y = bitsf(s2d[i].Y); s.z = bitsf(s2d[i].Z);
		freenect_map_camera_point_to_depth(m[0], (freenect_resolution)s2d[i].dres, s, &p);
		if (s2d[i].d == 0)
			ok &= p.x == FREENECT_MAPPER_INVALID && p.y == FREENECT_MAPPER_INVALID && p.depth_mm == 0;
		else
			ok &= p.x == s2d[i].x && p.y == s2d[i].y && p.depth_mm == s2d[i].d;
	}
	CHECK(ok, "camera space -> depth point: as Kinect10");
	{
		freenect_depth_point p, q;
		freenect_camera_point s;
		freenect_mapper_set_mirrored(m[0], 0);
		p.x = 100; p.y = 300; p.depth_mm = 1800;
		freenect_map_depth_point_to_camera(m[0], FREENECT_RESOLUTION_MEDIUM, p, &s);
		freenect_map_camera_point_to_depth(m[0], FREENECT_RESOLUTION_MEDIUM, s, &q);
		CHECK(s.x > 0.0f && s.y < 0.0f && q.x == 100 && q.y == 300 && q.depth_mm == 1800,
		      "  unmirrored: left of centre is +x, below is -y; and back");
		freenect_mapper_set_mirrored(m[0], 1);
	}

	/* camera space -> colour = -> 640x480 depth -> colour */
	{
		freenect_camera_point s;
		freenect_depth_point p;
		freenect_color_point a, b;
		s.x = 0.4f; s.y = -0.3f; s.z = 2.2f;
		freenect_map_camera_point_to_color(m[1], FREENECT_RESOLUTION_HIGH, s, &a);
		freenect_map_camera_point_to_depth(m[1], FREENECT_RESOLUTION_MEDIUM, s, &p);
		freenect_map_depth_point_to_color(m[1], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_HIGH, p, &b);
		CHECK(a.x == b.x && a.y == b.y, "camera space -> colour: through the 640x480 depth pixel");
	}

	/* colour frame -> depth: forward projection and gap filling (§20.5.3) */
	{
		static uint16_t wall[640 * 480];
		static freenect_depth_point out[1280 * 960], out2[1280 * 960];
		static freenect_camera_point cam[640 * 480];
		int x, y, valid = 0, near_ok = 1, checked = 0, cr;
		for (i = 0; i < 640 * 480; i++)
			wall[i] = 2000;
		for (cr = 0; cr < 2; cr++) {
			freenect_resolution cres = cr ? FREENECT_RESOLUTION_HIGH : FREENECT_RESOLUTION_MEDIUM;
			int cw = cr ? 1280 : 640, ch = cr ? 960 : 480;
			freenect_mapper_set_mirrored(m[4], 1);
			CHECK(freenect_map_color_frame_to_depth(m[4], FREENECT_RESOLUTION_MEDIUM, cres, wall, out) == 0,
			      cr ? "colour 1280x960 frame -> depth" : "colour 640x480 frame -> depth");
			valid = 0; near_ok = 1; checked = 0;
			for (y = 0; y < ch; y++)
				for (x = 0; x < cw; x++) {
					freenect_depth_point p = out[y * cw + x];
					freenect_color_point c;
					if (p.x == FREENECT_MAPPER_INVALID)
						continue;
					valid++;
					if ((x * 7 + y) % 53)
						continue;
					/* the depth pixel found maps back to (about) this colour pixel */
					freenect_map_depth_point_to_color(m[4], FREENECT_RESOLUTION_MEDIUM, cres, p, &c);
					checked++;
					near_ok &= abs(c.x - x) <= 2 * (cw / 640) + 1 && abs(c.y - y) <= 2 * (cw / 640);
				}
			printf("  (%d of %d colour pixels have depth; %d checked)\n", valid, cw * ch, checked);
			CHECK(valid > cw * ch * 8 / 10, "  a flat wall at 2 m covers most of the colour image (mild calibration)");
			CHECK(near_ok && checked > 100, "  and each depth pixel maps back to its colour pixel");
			freenect_mapper_set_mirrored(m[4], 0);
			freenect_map_color_frame_to_depth(m[4], FREENECT_RESOLUTION_MEDIUM, cres, wall, out2);
			ok = 1;
			for (y = 0; y < ch; y++)
				for (x = 0; x < cw; x++) {
					freenect_depth_point a = out[y * cw + x], b = out2[y * cw + (cw - 1 - x)];
					ok &= a.depth_mm == b.depth_mm && a.y == b.y &&
					      (a.x == FREENECT_MAPPER_INVALID ? b.x == a.x : b.x == 639 - a.x);
				}
			CHECK(ok, "  unmirrored: the mirror image of it");
		}

		/* gaps: one missing depth column is filled from its neighbours */
		freenect_mapper_set_mirrored(m[0], 1);
		for (y = 0; y < 480; y++)
			for (x = 0; x < 640; x++)
				wall[y * 640 + x] = 1500;
		freenect_map_color_frame_to_depth(m[0], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_MEDIUM, wall, out);
		valid = 0;
		for (i = 0; i < 640 * 480; i++)
			valid += out[i].x != FREENECT_MAPPER_INVALID;
		for (y = 0; y < 480; y++)
			wall[y * 640 + 300] = 0;
		freenect_map_color_frame_to_depth(m[0], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_MEDIUM, wall, out2);
		ok = 0;
		for (i = 0; i < 640 * 480; i++)
			ok += out2[i].x != FREENECT_MAPPER_INVALID;
		CHECK(ok == valid, "  one missing depth column: the gap it leaves is filled");
		ok = 1;
		for (i = 0; i < 640 * 480; i++)
			if (out2[i].x != FREENECT_MAPPER_INVALID)
				ok &= out2[i].depth_mm == 1500;
		CHECK(ok, "  with the neighbours' depth");

		/* colour frame -> camera space = colour -> depth, then each to camera */
		freenect_mapper_set_mirrored(m[4], 0);
		for (i = 0; i < 640 * 480; i++)
			wall[i] = (uint16_t)(1000 + (i % 640));
		freenect_map_color_frame_to_depth(m[4], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_MEDIUM, wall, out);
		freenect_map_color_frame_to_camera(m[4], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_MEDIUM, wall, cam);
		ok = 1;
		for (i = 0; i < 640 * 480; i += 11) {
			freenect_camera_point s = { 0.0f, 0.0f, 0.0f };
			if (out[i].x == FREENECT_MAPPER_INVALID) {
				ok &= cam[i].z == 0.0f;
				continue;
			}
			freenect_map_depth_point_to_camera(m[4], FREENECT_RESOLUTION_MEDIUM, out[i], &s);
			ok &= s.x == cam[i].x && s.y == cam[i].y && s.z == cam[i].z;
		}
		CHECK(ok, "colour frame -> camera space: through the depth points");
	}

	/* what is refused */
	{
		freenect_depth_point p;
		freenect_color_point c;
		static uint16_t d[640 * 480];
		static freenect_depth_point o[640 * 480];
		p.x = p.y = 0; p.depth_mm = 1000;
		CHECK(freenect_map_depth_point_to_color(m[0], FREENECT_RESOLUTION_HIGH, FREENECT_RESOLUTION_MEDIUM, p, &c) == -1 &&
		      freenect_map_depth_point_to_color(m[0], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_LOW, p, &c) == -1 &&
		      freenect_map_depth_point_to_color(NULL, FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_MEDIUM, p, &c) == -1 &&
		      freenect_map_color_frame_to_depth(m[0], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_MEDIUM, NULL, o) == -1 &&
		      freenect_map_color_frame_to_depth(m[0], FREENECT_RESOLUTION_MEDIUM, FREENECT_RESOLUTION_MEDIUM, d, NULL) == -1,
		      "unsupported resolutions and NULL pointers: -1");
		CHECK(freenect_create_coordinate_mapper(NULL) == NULL, "no device: no mapper");
	}

	for (i = 0; i < N(cals); i++)
		freenect_destroy_coordinate_mapper(m[i]);
	freenect_destroy_coordinate_mapper(NULL);
	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
