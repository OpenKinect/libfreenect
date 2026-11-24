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
 * The coordinate mapper of Microsoft's Kinect runtime, from the formulas of
 * the reference's §20.5:
 * - the calibration polynomial and the depth-dependent x shift (§20.5.1),
 * - depth -> colour, point and frame (§20.5.2),
 * - colour -> depth: forward projection and gap filling (§20.5.3),
 * - depth <-> camera space with the nominal intrinsics (§20.5.4).
 *
 * The runtime works in single precision; so does this, operation by
 * operation in the same order, so that the results are the same to the bit.
 * This file must be built without floating-point contraction (CMake sets
 * -ffp-contract=off), or a compiler may fuse a multiply and an add.
 *
 * Internally everything is in the runtime's orientation: depth and colour x
 * as in its mirrored images. Unmirrored coordinates are converted at the
 * edges (x' = width - 1 - x).
 *
 * Deviations, where the runtime's behaviour is an accident:
 * - The x shift is looked up with the depth itself (shift[min(d, 2047)]);
 *   the runtime uses (d << 3) as a 16-bit value, which wraps above 8191 mm,
 *   beyond its 4 m range but within libfreenect's full range.
 * - Depth 0 gives FREENECT_MAPPER_INVALID in the frame functions too (the
 *   runtime maps it with shift 0, to a meaningless pixel).
 */

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "libfreenect.h"
#include "libfreenect_coordinate_mapper.h"
#include "freenect_internal.h"

#define CAL_MAGIC "FNC1"
#define CAL_VERSION 1

struct freenect_coordinate_mapper {
	// The calibration, as libfreenect reads it from the device.
	freenect_reg_info reg;
	freenect_reg_pad_info pad;
	freenect_zero_plane_info zpi;
	uint16_t const_shift;
	// Derived (§20.5.1)
	float X[10], Y[10];   // polynomial coefficients (CDRPTable +0x08.., +0x30..)
	float shift[2048];    // x shift by depth in mm
	int mirrored;
	uint8_t* hit;         // colour -> depth: pixels a depth pixel landed on
	size_t hit_size;
};

/* ------------------------------------------------------------ calibration */

static int32_t sext(int32_t v, int bits)
{
	uint32_t m = 1u << (bits - 1);
	uint32_t u = (uint32_t)v & ((1u << bits) - 1);
	return (int32_t)((u ^ m) - m);
}

// One axis of the polynomial (FUN_18009fac0), in double, stored as float.
// a..d, start, dxdx, dydx, dxdxdx, dydxdx, dydydx as for the x axis; the y
// axis passes its own fields in the same places.
static void axis_coeffs(float out[10], int32_t a, int32_t b, int32_t c, int32_t d, int32_t start,
                        int32_t dxdx, int32_t dydx, int32_t dxdxdx, int32_t dydxdx, int32_t dydydx)
{
	double a_ = a / 2147483648.0 * (1.0 / 6), b_ = b / 2147483648.0 * (1.0 / 6);
	double c_ = c / 2147483648.0 * 0.5, d_ = d / 2147483648.0 * 0.5;
	double t20 = sext(dydxdx, 27) / 8388608.0 - c_ * -639.0 - d_ * -513.0;
	double t18 = (sext(dxdxdx, 27) / 8388608.0 - a_ * -1914.0 - c_ * -514.0) * 0.5;
	double t1c = (sext(dydydx, 27) / 8388608.0 - b_ * -1536.0 - d_ * -640.0) * 0.5;
	double t24 = sext(dxdx, 21) / 32768.0 - a_ * 306241.0 - c_ * 164223.0 - d_ * 66049.0
	             - t18 * -639.0 - t20 * -257.0;
	double t28 = sext(dydx, 21) / 32768.0 - b_ * 197377.0 - c_ * 102400.0 - d_ * 164160.0
	             - t1c * -513.0 - t20 * -320.0;
	double t2c = (sext(start, 19) / 256.0 - a_ * -32768000.0 - b_ * -16974593.0 - c_ * -26316800.0
	              - d_ * -21135680.0 - t18 * 102400.0 - t1c * 66049.0 - t20 * 82240.0
	              - t24 * -320.0 - t28 * -257.0) + 1.0;
	out[0] = (float)a_;  out[1] = (float)b_;  out[2] = (float)c_;  out[3] = (float)d_;
	out[4] = (float)t18; out[5] = (float)t1c; out[6] = (float)t20; out[7] = (float)t24;
	out[8] = (float)t28; out[9] = (float)t2c;
}

static void derive(freenect_coordinate_mapper* m)
{
	const freenect_reg_info* r = &m->reg;
	double P = m->zpi.reference_pixel_size, Zref = m->zpi.reference_distance;
	double k, A, B;
	int i;

	axis_coeffs(m->X, r->ax, r->bx, r->cx, r->dx, r->dx_start, r->dxdx_start, r->dydx_start,
	            r->dxdxdx_start, r->dydxdx_start, r->dydydx_start);
	axis_coeffs(m->Y, r->ay, r->by, r->cy, r->dy, r->dy_start, r->dxdy_start, r->dydy_start,
	            r->dxdxdy_start, r->dydxdy_start, r->dydydy_start);

	// x shift by depth; 2.4 cm is the runtime's fixed IR-RGB baseline
	k = 1.0 / ((P * 2.0) * 10.0);
	A = (Zref * k) * 10.0;
	B = (k * 2.4) * 10.0;
	for (i = 0; i < 2048; i++) {
		double d = (double)i * k, v;
		int32_t iv = INT32_MIN; // cvttsd2si of -inf, NaN or out of range
		int16_t s;
		if (i > 0) {
			v = (((d - A) * B) / d + 0.375) * 16.0;
			if (v > -2147483649.0 && v < 2147483648.0)
				iv = (int32_t)v;
		}
		s = (int16_t)(uint16_t)((uint32_t)iv & 0xffff);
		// arithmetic shift right by 4: floor(s / 16)
		m->shift[i] = (float)(s >= 0 ? s / 16 : -((-s + 15) / 16));
	}
}

static freenect_coordinate_mapper* mapper_new(void)
{
	freenect_coordinate_mapper* m = (freenect_coordinate_mapper*)calloc(1, sizeof(*m));
	return m;
}

freenect_coordinate_mapper* freenect_create_coordinate_mapper(freenect_device* dev)
{
	freenect_coordinate_mapper* m;
	if (!dev)
		return NULL;
	m = mapper_new();
	if (!m)
		return NULL;
	m->reg = dev->registration.reg_info;
	m->pad = dev->registration.reg_pad_info;
	m->zpi = dev->registration.zero_plane_info;
	m->const_shift = (uint16_t)dev->registration.const_shift;
	derive(m);
	return m;
}

static void put32(uint8_t* p, uint32_t v)
{
	p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t* p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put16(uint8_t* p, uint16_t v)
{
	p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

static uint16_t get16(const uint8_t* p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t float_bits(float f)
{
	uint32_t u;
	memcpy(&u, &f, 4);
	return u;
}

static float bits_float(uint32_t u)
{
	float f;
	memcpy(&f, &u, 4);
	return f;
}

/*
 * Layout (little endian):
 *   0  "FNC1"
 *   4  u32 version (1)
 *   8  29 x i32 registration block, in freenect_reg_info order (§20.1.3)
 * 124  3 x u16 start_lines, end_lines, cropping_lines; u16 0
 * 132  4 x f32 dcmos_emitter_dist, dcmos_rcmos_dist, reference_distance,
 *      reference_pixel_size
 * 148  u16 const shift; u16 0
 */
int freenect_mapper_get_calibration(freenect_coordinate_mapper* m, void* buf, size_t size)
{
	uint8_t* p = (uint8_t*)buf;
	int32_t words[29];
	int i;
	if (!m || !buf || size < FREENECT_CALIBRATION_SIZE)
		return -1;
	memset(p, 0, FREENECT_CALIBRATION_SIZE);
	memcpy(p, CAL_MAGIC, 4);
	put32(p + 4, CAL_VERSION);
	memcpy(words, &m->reg, sizeof(words));
	for (i = 0; i < 29; i++)
		put32(p + 8 + 4 * i, (uint32_t)words[i]);
	put16(p + 124, m->pad.start_lines);
	put16(p + 126, m->pad.end_lines);
	put16(p + 128, m->pad.cropping_lines);
	put32(p + 132, float_bits(m->zpi.dcmos_emitter_dist));
	put32(p + 136, float_bits(m->zpi.dcmos_rcmos_dist));
	put32(p + 140, float_bits(m->zpi.reference_distance));
	put32(p + 144, float_bits(m->zpi.reference_pixel_size));
	put16(p + 148, m->const_shift);
	return FREENECT_CALIBRATION_SIZE;
}

freenect_coordinate_mapper* freenect_create_coordinate_mapper_from_calibration(const void* data, size_t size)
{
	const uint8_t* p = (const uint8_t*)data;
	freenect_coordinate_mapper* m;
	int32_t words[29];
	int i;
	if (!data || size < FREENECT_CALIBRATION_SIZE || memcmp(p, CAL_MAGIC, 4) != 0 || get32(p + 4) != CAL_VERSION)
		return NULL;
	m = mapper_new();
	if (!m)
		return NULL;
	for (i = 0; i < 29; i++)
		words[i] = (int32_t)get32(p + 8 + 4 * i);
	memcpy(&m->reg, words, sizeof(words));
	m->pad.start_lines = get16(p + 124);
	m->pad.end_lines = get16(p + 126);
	m->pad.cropping_lines = get16(p + 128);
	m->zpi.dcmos_emitter_dist = bits_float(get32(p + 132));
	m->zpi.dcmos_rcmos_dist = bits_float(get32(p + 136));
	m->zpi.reference_distance = bits_float(get32(p + 140));
	m->zpi.reference_pixel_size = bits_float(get32(p + 144));
	m->const_shift = get16(p + 148);
	derive(m);
	return m;
}

void freenect_destroy_coordinate_mapper(freenect_coordinate_mapper* m)
{
	if (!m)
		return;
	free(m->hit);
	free(m);
}

void freenect_mapper_set_mirrored(freenect_coordinate_mapper* m, int mirrored)
{
	if (m)
		m->mirrored = mirrored != 0;
}

/* ------------------------------------------------------------ resolutions */

static int depth_size(freenect_resolution r, int* w, int* h)
{
	switch (r) {
	case FREENECT_RESOLUTION_VERY_LOW: *w = 80;  *h = 60;  return 0;
	case FREENECT_RESOLUTION_LOW:      *w = 320; *h = 240; return 0;
	case FREENECT_RESOLUTION_MEDIUM:   *w = 640; *h = 480; return 0;
	default: return -1;
	}
}

static int color_size(freenect_resolution r, int* w, int* h)
{
	switch (r) {
	case FREENECT_RESOLUTION_MEDIUM: *w = 640;  *h = 480; return 0;
	case FREENECT_RESOLUTION_HIGH:   *w = 1280; *h = 960; return 0;
	default: return -1;
	}
}

/* ------------------------------------------------------------ the mapping */

// Px(u, v) and Py(u, v) of §20.5.1, single precision, in the runtime's
// order of operations (FUN_18009e800). Each statement rounds to float.
static float poly(const float c[10], float u, float u2, float v)
{
	float t1, t2, t3, s;
	t1 = v * c[2];  t1 = t1 + c[4];  t1 = t1 * u2;     // (v*c10 + c18) * u2
	t2 = u2 * u;    t2 = t2 * c[0];                    // u^3 * c08
	s = t1 + t2;
	t1 = v * c[3];  t1 = t1 + c[6];  t1 = t1 * v;      // ((v*c14 + c20) * v
	t1 = t1 + c[7]; t1 = t1 * u;                       //  + c24) * u
	s = s + t1;
	t3 = v * c[1];  t3 = t3 + c[5];  t3 = t3 * v;      // ((v*c0c + c1c) * v
	t3 = t3 + c[8]; t3 = t3 * v;                       //  + c28) * v
	s = s + t3;
	s = s + c[9];
	return s;
}

static float shift_of(const freenect_coordinate_mapper* m, unsigned depth)
{
	return m->shift[depth < 2048 ? depth : 2047];
}

// Depth pixel (x, y) of a depth image w wide, runtime orientation, in the
// 1280-wide grid: colour position in 640x480 units (FUN_18009e800 before
// its final scaling), as floats.
static void project(const freenect_coordinate_mapper* m, int X, int Y, unsigned depth, float* fx, float* fy)
{
	float y480 = (float)Y * 0.5f;
	float v = y480 - 256.0f;
	float x640 = (float)X * 0.5f;
	float u = 319.0f - x640;
	float u2 = u * u;
	float px = poly(m->X, u, u2, v);
	float py = poly(m->Y, u, u2, v);
	float t = 319.0f - u;
	t = t - px;
	*fx = t - shift_of(m, depth);
	*fy = py + y480;
}

// (int) of a float as cvttss2si does it: INT32_MIN when out of range or NaN
// (a plain cast would be undefined there).
static int32_t f2i(float f)
{
	if (!(f > -2147483648.0f && f < 2147483648.0f))
		return INT32_MIN;
	return (int32_t)f;
}

int freenect_map_depth_point_to_color(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_resolution color_res,
	freenect_depth_point p, freenect_color_point* out)
{
	int dw, dh, cw, ch, s;
	int32_t cx1280, cy1280;
	float fx, fy;
	if (!m || !out || depth_size(depth_res, &dw, &dh) < 0 || color_size(color_res, &cw, &ch) < 0)
		return -1;
	if (p.depth_mm == 0) {
		out->x = out->y = FREENECT_MAPPER_INVALID;
		return 0;
	}
	s = 1280 / dw;
	if (!m->mirrored)
		p.x = dw - 1 - p.x;
	project(m, p.x * s, p.y * s, p.depth_mm, &fx, &fy);
	fx = fx * 2.0f;
	fy = fy * 2.0f;
	cx1280 = f2i(fx);
	cy1280 = f2i(fy);
	out->x = (int32_t)((int64_t)cw * cx1280 / 1280);
	out->y = (int32_t)((int64_t)ch * cy1280 / 960);
	if (!m->mirrored)
		out->x = cw - 1 - out->x;
	return 0;
}

int freenect_map_depth_frame_to_color(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_resolution color_res,
	const uint16_t* depth_mm, freenect_color_point* out)
{
	int dw, dh, cw, ch, s, r, c;
	float scale;
	if (!m || !depth_mm || !out || depth_size(depth_res, &dw, &dh) < 0 || color_size(color_res, &cw, &ch) < 0)
		return -1;
	s = 1280 / dw;
	scale = (float)cw / 640.0f;
	// FUN_18009d670: one row and column loop, scaled by colourW/640.
	for (r = 0; r < dh; r++) {
		for (c = 0; c < dw; c++) {
			int src = m->mirrored ? c : dw - 1 - c;  // depth pixel at runtime column c
			unsigned d = depth_mm[r * dw + src];
			freenect_color_point* o = &out[r * dw + src];
			float fx, fy;
			if (d == 0) {
				o->x = o->y = FREENECT_MAPPER_INVALID;
				continue;
			}
			project(m, c * s, r * s, d, &fx, &fy);
			o->x = f2i(fx * scale);
			o->y = f2i(fy * scale);
			if (!m->mirrored)
				o->x = cw - 1 - o->x;
		}
	}
	return 0;
}

int freenect_map_color_frame_to_depth(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_resolution color_res,
	const uint16_t* depth_mm, freenect_depth_point* out)
{
	int dw, dh, cw, ch, R, C, i, rows, rstep;
	float scale, du;
	size_t n;
	uint8_t* hit;
	if (!m || !depth_mm || !out || depth_size(depth_res, &dw, &dh) < 0 || color_size(color_res, &cw, &ch) < 0)
		return -1;
	n = (size_t)cw * (size_t)ch;
	if (m->hit_size < n) {
		uint8_t* h = (uint8_t*)realloc(m->hit, n);
		if (!h)
			return -1;
		m->hit = h;
		m->hit_size = n;
	}
	hit = m->hit;
	memset(hit, 0, n);
	for (i = 0; i < (int)n; i++) {
		out[i].x = out[i].y = FREENECT_MAPPER_INVALID;
		out[i].depth_mm = 0;
	}

	// Forward projection (FUN_18009de70): every depth pixel that a colour
	// pixel's column and row select is projected; the last one to land on
	// a colour pixel wins. Rows in the 960-high grid; column 0 is never
	// visited, and the polynomial is evaluated one column to the left of
	// the depth pixel's (as the runtime does).
	scale = (float)cw / 640.0f;
	du = 640.0f / (float)cw;
	rows = dh * 1280 / dw;
	rstep = 1280 / cw;
	for (R = 0; R < rows; R += rstep) {
		float y480 = (float)R * 0.5f;
		float v = y480 - 256.0f;
		int dy = R / (1280 / dw);
		float u = -319.0f;
		for (C = cw - 1; C >= 1; C--, u = u + du) {
			int dx = C / (cw / dw);
			int src = m->mirrored ? dx : dw - 1 - dx;
			unsigned d = depth_mm[dy * dw + src];
			float u2, px, py, fx, fy, t;
			int64_t idx;
			if (d == 0)
				continue;
			u2 = u * u;
			px = poly(m->X, u, u2, v);
			py = poly(m->Y, u, u2, v);
			t = 319.0f - u;
			t = t - px;
			t = t - shift_of(m, d);
			fx = t * scale;
			fy = py + y480;
			fy = fy * scale;
			if (!(fx >= 0.0f && fx < (float)cw && fy >= 0.0f && fy < (float)ch))
				continue;
			idx = (int64_t)(floorf(fy) * (float)cw + fx);
			if (idx < 0 || idx >= (int64_t)n)
				continue;
			out[idx].x = dx;
			out[idx].y = dy;
			out[idx].depth_mm = (uint16_t)d;
			hit[idx] = 1;
		}
	}

	// Gap filling (FUN_18009df70), left to right along each row, so a
	// filled pixel counts for the next one.
	for (R = 0; R < ch; R++) {
		freenect_depth_point* row = out + (size_t)R * cw;
		uint8_t* h = hit + (size_t)R * cw;
		for (i = 1; i < cw - 2; i++) {
			if (h[i] || !h[i - 1])
				continue;
			if (h[i + 1]) {
				row[i].x = (int32_t)(0.5f * (float)(row[i - 1].x + row[i + 1].x));
				row[i].y = (int32_t)(0.5f * (float)(row[i - 1].y + row[i + 1].y));
				row[i].depth_mm = (uint16_t)(int)(0.5f * (float)(row[i - 1].depth_mm + row[i + 1].depth_mm));
				h[i] = 1;
			} else if (h[i + 2]) {
				row[i].x = (int32_t)((2.0f / 3.0f) * (float)row[i - 1].x + (1.0f / 3.0f) * (float)row[i + 2].x);
				row[i].y = (int32_t)((2.0f / 3.0f) * (float)row[i - 1].y + (1.0f / 3.0f) * (float)row[i + 2].y);
				row[i].depth_mm = (uint16_t)(int)((2.0f / 3.0f) * (float)row[i - 1].depth_mm +
				                                  (1.0f / 3.0f) * (float)row[i + 2].depth_mm);
				h[i] = 1;
			}
		}
	}

	// Back to unmirrored images: reverse each colour row, and the depth x.
	if (!m->mirrored) {
		for (R = 0; R < ch; R++) {
			freenect_depth_point* row = out + (size_t)R * cw;
			int a, b;
			for (a = 0, b = cw - 1; a < b; a++, b--) {
				freenect_depth_point t = row[a];
				row[a] = row[b];
				row[b] = t;
			}
			for (a = 0; a < cw; a++)
				if (row[a].x != FREENECT_MAPPER_INVALID)
					row[a].x = dw - 1 - row[a].x;
		}
	}
	return 0;
}

/* ------------------------------------------------------- camera space */

// FUN_180028650: nominal inverse focal length 0.003501 at 320x240.
static void depth_to_camera(int x, int y, unsigned depth, int w, int h, freenect_camera_point* out)
{
	float z, zf, t;
	if (depth == 0) {
		out->x = out->y = out->z = 0.0f;
		return;
	}
	z = (float)depth * 0.001f;
	zf = z * 0.003501f;
	t = (float)x / (float)w;
	t = t - 0.5f;
	t = t * zf;
	out->x = t * 320.0f;
	t = (float)y / (float)h;
	t = 0.5f - t;
	t = t * zf;
	out->y = t * 240.0f;
	out->z = z;
}

int freenect_map_depth_point_to_camera(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_depth_point p, freenect_camera_point* out)
{
	int dw, dh;
	if (!m || !out || depth_size(depth_res, &dw, &dh) < 0)
		return -1;
	depth_to_camera(m->mirrored ? p.x : dw - 1 - p.x, p.y, p.depth_mm, dw, dh, out);
	return 0;
}

int freenect_map_depth_frame_to_camera(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, const uint16_t* depth_mm, freenect_camera_point* out)
{
	int dw, dh, x, y;
	if (!m || !depth_mm || !out || depth_size(depth_res, &dw, &dh) < 0)
		return -1;
	for (y = 0; y < dh; y++)
		for (x = 0; x < dw; x++)
			depth_to_camera(m->mirrored ? x : dw - 1 - x, y, depth_mm[y * dw + x], dw, dh, &out[y * dw + x]);
	return 0;
}

// FUN_180028a00: nominal focal length 285.63 px at 320x240.
static void camera_to_depth(freenect_camera_point p, int w, int h, freenect_depth_point* out)
{
	const float F = 285.63f;
	float t, q;
	if ((p.x == 0.0f && p.y == 0.0f && p.z == 0.0f) || p.z < 0.0f) {
		out->x = out->y = FREENECT_MAPPER_INVALID;
		out->depth_mm = 0;
		return;
	}
	t = p.x * F;
	q = p.z * 320.0f;
	t = t / q;
	t = t + 0.5f;
	t = t * (float)w;
	t = t + 0.5f;
	out->x = f2i(t);
	t = p.y * F;
	q = p.z * 240.0f;
	t = t / q;
	t = 0.5f - t;
	t = t * (float)h;
	t = t + 0.5f;
	out->y = f2i(t);
	t = p.z * 1000.0f;
	t = t + 0.5f;
	out->depth_mm = (uint16_t)f2i(t);
}

int freenect_map_camera_point_to_depth(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_camera_point p, freenect_depth_point* out)
{
	int dw, dh;
	if (!m || !out || depth_size(depth_res, &dw, &dh) < 0)
		return -1;
	camera_to_depth(p, dw, dh, out);
	if (!m->mirrored && out->x != FREENECT_MAPPER_INVALID)
		out->x = dw - 1 - out->x;
	return 0;
}

int freenect_map_camera_point_to_color(freenect_coordinate_mapper* m,
	freenect_resolution color_res, freenect_camera_point p, freenect_color_point* out)
{
	freenect_depth_point d;
	int mirrored, r;
	if (!m || !out)
		return -1;
	// FUN_180028870: to a 640x480 depth pixel, then to colour.
	mirrored = m->mirrored;
	m->mirrored = 1;
	camera_to_depth(p, 640, 480, &d);
	r = freenect_map_depth_point_to_color(m, FREENECT_RESOLUTION_MEDIUM, color_res, d, out);
	m->mirrored = mirrored;
	if (r == 0 && !mirrored && out->x != FREENECT_MAPPER_INVALID) {
		int cw = 0, ch = 0;
		color_size(color_res, &cw, &ch);
		out->x = cw - 1 - out->x;
	}
	return r;
}

int freenect_map_color_frame_to_camera(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_resolution color_res,
	const uint16_t* depth_mm, freenect_camera_point* out)
{
	int dw, dh, cw, ch, i, n;
	freenect_depth_point* pts;
	if (!m || !depth_mm || !out || depth_size(depth_res, &dw, &dh) < 0 || color_size(color_res, &cw, &ch) < 0)
		return -1;
	n = cw * ch;
	pts = (freenect_depth_point*)malloc(sizeof(*pts) * (size_t)n);
	if (!pts)
		return -1;
	// FUN_180029430: colour -> depth points, then each point to camera space.
	if (freenect_map_color_frame_to_depth(m, depth_res, color_res, depth_mm, pts) < 0) {
		free(pts);
		return -1;
	}
	for (i = 0; i < n; i++) {
		if (pts[i].x == FREENECT_MAPPER_INVALID)
			out[i].x = out[i].y = out[i].z = 0.0f;
		else
			depth_to_camera(m->mirrored ? pts[i].x : dw - 1 - pts[i].x, pts[i].y, pts[i].depth_mm, dw, dh, &out[i]);
	}
	free(pts);
	return 0;
}
