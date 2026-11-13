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
 * User segmentation, libfreenect's own (not Kinect10's BGR stage):
 *
 * - The floor: RANSAC on the 80x60 cell points each frame, with Kinect10's
 *   acceptance rules (normal within 30 degrees of vertical, C <= 0.2588,
 *   at least 51 inliers, §30.4.5), refined by least squares, kept from frame
 *   to frame when a new estimate is not better.
 * - A background model per map pixel: the farthest stable depth seen,
 *   following slow changes and absorbing objects that stay still for 10 s.
 * - Connected components of non-floor pixels joined where depth is
 *   continuous; a component is a user when it continues an existing user
 *   (pixel overlap with the previous frame), or when it is person-shaped and
 *   has moved against the background, or when it is person-shaped and the
 *   body-part forest confirms a body (nui.c), so that someone already there
 *   at the start is found without moving. Components that join two users are
 *   split by the nearest previous user centre.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "nui_internal.h"

#define CELLW 80
#define CELLH 60

#define MIN_USER_PIXELS 600
#define FLOOR_EXCLUDE_MM 50.0f
#define FLOOR_MIN_POINTS 120      // 4x4 blocks on a plane before it can be the floor (a seat has fewer)
#define FLOOR_MIN_DROP_MM 350.0f // the floor is at least this far below the Kinect
#define FLOOR_BELOW_MM 150.0f  // points this far under a plane make it not the floor
#define USER_MAX_Z_MM 4500
#define BG_ABSORB_FRAMES 300
#define USER_TIMEOUT 10
#define BG_FARTHER_FRAMES 3   // farther depth must stay this long to become background
#define BG_CONF_FRAMES 30    // matched the background this long: the room, not a user
#define FG_CONF_FRAMES 10    // background seen this long: in front of it is motion
#define NEAR_USER_PX 12      // margin around a user where the room is not learned as empty
#define BG_LEARN_FRAMES 15    // until then, anything person-sized is a candidate
#define TWO_PEOPLE_WIDTH 0.9f // m: a component narrower than this is one person

float fn_nui_v3len(v3 a) { return sqrtf(v3dot(a, a)); }
v3 fn_nui_v3norm(v3 a)
{
	float l = fn_nui_v3len(a);
	return l > 0.0f ? v3scale(a, 1.0f / l) : a;
}

int fn_nui_seg_init(fn_nui_seg* s)
{
	memset(s, 0, sizeof(*s));
	s->bg = (uint16_t*)calloc(NPIX, sizeof(uint16_t));
	s->bgage = (uint16_t*)calloc(NPIX, sizeof(uint16_t));
	s->bgconf = (uint16_t*)calloc(NPIX, sizeof(uint16_t));
	s->near_user = (uint8_t*)calloc(NPIX, 1);
	s->label = (uint16_t*)calloc(NPIX, sizeof(uint16_t));
	s->user = (uint8_t*)calloc(NPIX, 1);
	s->prev_user = (uint8_t*)calloc(NPIX, 1);
	s->parent = (uint32_t*)calloc(NPIX, sizeof(uint32_t));
	if (!s->bg || !s->bgage || !s->bgconf || !s->near_user || !s->label || !s->user || !s->prev_user || !s->parent) {
		fn_nui_seg_free(s);
		return -1;
	}
	fn_nui_seg_reset(s);
	return 0;
}

void fn_nui_seg_free(fn_nui_seg* s)
{
	free(s->bg); free(s->bgage); free(s->bgconf); free(s->near_user); free(s->label); free(s->user); free(s->prev_user); free(s->parent);
	s->bg = s->bgage = s->bgconf = s->label = NULL;
	s->near_user = NULL;
	s->user = s->prev_user = NULL;
	s->parent = NULL;
}

void fn_nui_seg_reset(fn_nui_seg* s)
{
	memset(s->bg, 0, NPIX * sizeof(uint16_t));
	memset(s->bgage, 0, NPIX * sizeof(uint16_t));
	memset(s->bgconf, 0, NPIX * sizeof(uint16_t));
	memset(s->user, 0, NPIX);
	memset(s->prev_user, 0, NPIX);
	memset(s->u, 0, sizeof(s->u));
	memset(s->floor, 0, sizeof(s->floor));
	s->floor_valid = 0;
	s->rng = 0x12345678u;
	s->next_id = 1;
	s->frames = 0;
}

// The CDRP stage's map (§30.1.4): mirrored, 2x2 max pool.
void fn_nui_make_map(const uint16_t* depth, uint16_t* map)
{
	int x, y;
	for (y = 0; y < NH; y++) {
		const uint16_t* r0 = depth + (2 * y) * 640;
		const uint16_t* r1 = r0 + 640;
		for (x = 0; x < NW; x++) {
			int c = 638 - 2 * x;
			uint16_t m = r0[c];
			if (r0[c + 1] > m) m = r0[c + 1];
			if (r1[c] > m) m = r1[c];
			if (r1[c + 1] > m) m = r1[c + 1];
			map[y * NW + x] = m;
		}
	}
}

float fn_nui_floor_dist(const fn_nui_seg* s, v3 p)
{
	if (!s->floor_valid)
		return 1e9f;
	return s->floor[0] * p.x * 1000.0f + s->floor[1] * p.y * 1000.0f + s->floor[2] * p.z * 1000.0f + s->floor[3];
}

/* ------------------------------------------------------------ floor */

static uint32_t lcg(fn_nui_seg* s)
{
	s->rng = s->rng * 0x343fdu + 0x269ec3u; // the MS CRT generator Kinect10 uses
	return (s->rng >> 16) & 0x7fff;
}

static int plane_from(v3 a, v3 b, v3 c, float pl[4])
{
	v3 n = v3cross(v3sub(b, a), v3sub(c, a));
	float l = fn_nui_v3len(n);
	if (l < 1e-6f)
		return -1;
	n = v3scale(n, 1.0f / l);
	if (n.y < 0)
		n = v3scale(n, -1.0f);
	pl[0] = n.x; pl[1] = n.y; pl[2] = n.z;
	pl[3] = -v3dot(n, a);
	return 0;
}

static int plane_ok(const float pl[4])
{
	// within 30 degrees of vertical (§30.4.5), and a plausible distance
	// below the Kinect: not a lap, a seat or a low table seen up close. The
	// motor tilts the Kinect forward and back only; sideways it stands level
	// (within 10 degrees), so a plane rolled further is a slope (a bed, a
	// cushion), not the floor.
	return pl[1] >= 0.8660f && pl[2] <= 0.2588f && fabsf(pl[0]) <= 0.1736f && pl[3] >= FLOOR_MIN_DROP_MM;
}

static int count_inliers(const v3* pts, int n, const float pl[4], float tol)
{
	int i, c = 0;
	for (i = 0; i < n; i++)
		if (fabsf(pl[0] * pts[i].x + pl[1] * pts[i].y + pl[2] * pts[i].z + pl[3]) < tol)
			c++;
	return c;
}

// Nothing is below the floor: a horizontal plane with many points under it
// is a table or a bed, not the floor (whose legs it would cut off).
static int below_floor(const v3* pts, int n, const float pl[4])
{
	int i, c = 0;
	for (i = 0; i < n; i++)
		if (pl[0] * pts[i].x + pl[1] * pts[i].y + pl[2] * pts[i].z + pl[3] < -FLOOR_BELOW_MM)
			c++;
	return c > 20 + n / 50;
}

// least squares y = a x + b z + c over the inliers
static int refine(const v3* pts, int n, float pl[4], float tol)
{
	double S[3][3] = { { 0 } }, r[3] = { 0 }, a, b, c, det;
	int i, k = 0;
	for (i = 0; i < n; i++) {
		const v3* p = &pts[i];
		if (fabsf(pl[0] * p->x + pl[1] * p->y + pl[2] * p->z + pl[3]) >= tol)
			continue;
		S[0][0] += p->x * p->x; S[0][1] += p->x * p->z; S[0][2] += p->x;
		S[1][1] += p->z * p->z; S[1][2] += p->z; S[2][2] += 1.0;
		r[0] += p->x * p->y; r[1] += p->z * p->y; r[2] += p->y;
		k++;
	}
	if (k < 10)
		return -1;
	S[1][0] = S[0][1]; S[2][0] = S[0][2]; S[2][1] = S[1][2];
	det = S[0][0] * (S[1][1] * S[2][2] - S[1][2] * S[2][1]) - S[0][1] * (S[1][0] * S[2][2] - S[1][2] * S[2][0]) +
	      S[0][2] * (S[1][0] * S[2][1] - S[1][1] * S[2][0]);
	if (fabs(det) < 1e-9)
		return -1;
	a = (r[0] * (S[1][1] * S[2][2] - S[1][2] * S[2][1]) - S[0][1] * (r[1] * S[2][2] - S[1][2] * r[2]) +
	     S[0][2] * (r[1] * S[2][1] - S[1][1] * r[2])) / det;
	b = (S[0][0] * (r[1] * S[2][2] - S[1][2] * r[2]) - r[0] * (S[1][0] * S[2][2] - S[1][2] * S[2][0]) +
	     S[0][2] * (S[1][0] * r[2] - r[1] * S[2][0])) / det;
	c = (S[0][0] * (S[1][1] * r[2] - r[1] * S[2][1]) - S[0][1] * (S[1][0] * r[2] - r[1] * S[2][0]) +
	     r[0] * (S[1][0] * S[2][1] - S[1][1] * S[2][0])) / det;
	{
		// y = a x + b z + c  ->  -a x + y - b z - c = 0
		double l = sqrt(a * a + 1.0 + b * b);
		pl[0] = (float)(-a / l); pl[1] = (float)(1.0 / l); pl[2] = (float)(-b / l); pl[3] = (float)(-c / l);
	}
	return 0;
}

static void floor_update(fn_nui_seg* s, const uint16_t* map)
{
	static v3 pts[CELLW * CELLH];
	int n = 0, cx, cy, it, best_in = 0;
	float best[4], tol = 25.0f;
	// one point per 4x4 block: its nearest valid depth, not a user's
	for (cy = 0; cy < CELLH; cy++)
		for (cx = 0; cx < CELLW; cx++) {
			int x, y, zmin = 0, user = 0;
			for (y = 4 * cy; y < 4 * cy + 4; y++)
				for (x = 4 * cx; x < 4 * cx + 4; x++) {
					int d = map[y * NW + x];
					if (s->prev_user[y * NW + x])
						user = 1;
					if (d > 0 && d < 8000 && (zmin == 0 || d < zmin))
						zmin = d;
				}
			if (zmin && !user) {
				v3 p = fn_nui_unproject(4.0f * cx + 1.5f, 4.0f * cy + 1.5f, (float)zmin);
				pts[n++] = v3scale(p, 1000.0f); // mm
			}
		}
	if (n < 200)
		return;
	for (it = 0; it < 32; it++) {
		float pl[4];
		int a = (int)(lcg(s) % (uint32_t)n), b = (int)(lcg(s) % (uint32_t)n), c = (int)(lcg(s) % (uint32_t)n), in;
		if (a == b || b == c || a == c || plane_from(pts[a], pts[b], pts[c], pl) < 0 || !plane_ok(pl))
			continue;
		if (below_floor(pts, n, pl))
			continue;
		in = count_inliers(pts, n, pl, tol);
		if (in > best_in) {
			best_in = in;
			memcpy(best, pl, sizeof(best));
		}
	}
	if (s->floor_valid && below_floor(pts, n, s->floor))
		s->floor_valid = 0;
	if (best_in >= FLOOR_MIN_POINTS && refine(pts, n, best, tol) == 0 && plane_ok(best) && !below_floor(pts, n, best)) {
		int in = count_inliers(pts, n, best, tol);
		// the floor so far, unless it turned out to have things under it
		int prev_in = s->floor_valid && !below_floor(pts, n, s->floor) ? count_inliers(pts, n, s->floor, tol) : 0;
		if (in >= FLOOR_MIN_POINTS && in >= prev_in) {
			memcpy(s->floor, best, sizeof(best));
			s->floor_valid = 1;
		}
	}
}

/* ------------------------------------------------------------ rejection */

// A candidate the forest did not confirm (a chair, a door frame): its pixels
// become background, so it is not found again until something there moves.
void fn_nui_seg_reject(fn_nui_seg* s, int k, const uint16_t* map)
{
	int i;
	for (i = 0; i < NPIX; i++)
		if (s->user[i] == k + 1) {
			s->user[i] = s->prev_user[i] = 0;
			if (map[i]) {
				s->bg[i] = map[i];
				s->bgage[i] = 0;
				s->bgconf[i] = BG_CONF_FRAMES;
			}
		}
	memset(&s->u[k], 0, sizeof(s->u[k]));
}

/* ------------------------------------------------------------ components */

static uint32_t find(uint32_t* p, uint32_t i)
{
	while (p[i] != i) {
		p[i] = p[p[i]];
		i = p[i];
	}
	return i;
}

static void unite(uint32_t* p, uint32_t a, uint32_t b)
{
	a = find(p, a);
	b = find(p, b);
	if (a != b) {
		if (a < b) p[b] = a;
		else p[a] = b;
	}
}

static int joined(int a, int b)
{
	int lo = a < b ? a : b, diff = a > b ? a - b : b - a;
	return a > 0 && b > 0 && diff < 40 + lo * 3 / 100;
}

typedef struct {
	int count, fg, fgsure, overlap[NU];
	double sx, sy, sz;
	float ymin, ymax, xmin, xmax, zmin, zmax;
	int umin, umax, vmin, vmax;
	int user;               // assigned user slot + 1 (-1: split)
	unsigned choosers;      // users that continue in this component (bit k)
} comp;

#define MAXCOMP 4096

void fn_nui_segment(fn_nui_seg* s, const uint16_t* map, int seated)
{
	static comp cs[MAXCOMP];
	uint8_t* valid = s->user; // reused as a scratch flag before it is rebuilt
	int i, k, ncomp = 0;
	(void)seated;

	floor_update(s, map);

	// pixels that can belong to a user: valid, near enough, not floor
	for (i = 0; i < NPIX; i++) {
		int d = map[i];
		valid[i] = 0;
		if (d > 0 && d < USER_MAX_Z_MM) {
			v3 p = fn_nui_unproject((float)(i % NW), (float)(i / NW), (float)d);
			valid[i] = fn_nui_floor_dist(s, p) > FLOOR_EXCLUDE_MM;
		}
		s->parent[i] = (uint32_t)i;
	}
	for (i = 0; i < NPIX; i++) {
		int x = i % NW;
		if (!valid[i])
			continue;
		if (x + 1 < NW && valid[i + 1] && joined(map[i], map[i + 1]))
			unite(s->parent, (uint32_t)i, (uint32_t)i + 1);
		if (i + NW < NPIX && valid[i + NW] && joined(map[i], map[i + NW]))
			unite(s->parent, (uint32_t)i, (uint32_t)(i + NW));
	}

	// component numbers: roots first (in pixel order), then every pixel
	// gets its root's (roots always come before their members' later
	// look-ups, since find() only reads parent[])
	for (i = 0; i < NPIX; i++) {
		s->label[i] = 0;
		if (!valid[i] || find(s->parent, (uint32_t)i) != (uint32_t)i || ncomp >= MAXCOMP - 1)
			continue;
		ncomp++;
		memset(&cs[ncomp], 0, sizeof(comp));
		cs[ncomp].ymin = cs[ncomp].xmin = cs[ncomp].zmin = 1e9f;
		cs[ncomp].ymax = cs[ncomp].xmax = cs[ncomp].zmax = -1e9f;
		cs[ncomp].umin = cs[ncomp].vmin = 100000;
		cs[ncomp].umax = cs[ncomp].vmax = -1;
		s->label[i] = (uint16_t)ncomp;
	}
	for (i = 0; i < NPIX; i++) {
		int c, d, u = i % NW, v = i / NW;
		v3 p;
		comp* C;
		if (!valid[i])
			continue;
		c = s->label[find(s->parent, (uint32_t)i)];
		if (!c)
			continue; // more components than the table holds
		s->label[i] = (uint16_t)c;
		C = &cs[c];
		d = map[i];
		p = fn_nui_unproject((float)u, (float)v, (float)d);
		C->count++;
		C->sx += p.x; C->sy += p.y; C->sz += p.z;
		if (p.y < C->ymin) C->ymin = p.y;
		if (p.y > C->ymax) C->ymax = p.y;
		if (p.x < C->xmin) C->xmin = p.x;
		if (p.x > C->xmax) C->xmax = p.x;
		if (p.z < C->zmin) C->zmin = p.z;
		if (p.z > C->zmax) C->zmax = p.z;
		if (u < C->umin) C->umin = u;
		if (u > C->umax) C->umax = u;
		if (v < C->vmin) C->vmin = v;
		if (v > C->vmax) C->vmax = v;
		if (s->bg[i] && d < s->bg[i] - (60 + d / 50)) {
			C->fg++;
			if (s->bgconf[i] >= FG_CONF_FRAMES)
				C->fgsure++; // in front of a background seen empty for a while
		}
		if (s->prev_user[i])
			C->overlap[s->prev_user[i] - 1]++;
	}

	// Continue existing users: each user takes the one component it overlaps
	// most, at a plausible depth (u[k].pixels is still last frame's count).
	// A component two users take is split between them; other components
	// (a strip of wall a moving user uncovers, say) are new candidates.
	memset(s->user, 0, NPIX);
	for (i = 1; i <= ncomp; i++) {
		cs[i].user = 0;
		cs[i].choosers = 0;
	}
	for (k = 0; k < NU; k++) {
		int best = 0, bo = 0;
		if (!s->u[k].active)
			continue;
		for (i = 1; i <= ncomp; i++) {
			float zc;
			if (cs[i].overlap[k] <= bo || cs[i].count < MIN_USER_PIXELS / 4)
				continue;
			zc = (float)(cs[i].sz / cs[i].count);
			if (s->u[k].pixels && fabsf(zc - s->u[k].centre.z) > 0.6f)
				continue;
			bo = cs[i].overlap[k];
			best = i;
		}
		if (best && (bo * 5 >= cs[best].count || bo * 3 >= s->u[k].pixels + 1))
			cs[best].choosers |= 1u << k;
	}
	for (i = 1; i <= ncomp; i++) {
		comp* C = &cs[i];
		if (C->choosers) {
			int n2 = 0;
			for (k = 0; k < NU; k++)
				if (C->choosers & (1u << k)) {
					n2++;
					C->user = k + 1;
				}
			if (n2 > 1) {
				// Two users in one component: two people touching, or one person
				// that came in as two candidates (an arm and the body, say). Only
				// a component wide enough for two keeps both; otherwise it goes to
				// one user (verified first, then the oldest), and the other loses
				// its pixels and times out.
				if (C->xmax - C->xmin > TWO_PEOPLE_WIDTH) {
					C->user = -1; // split among its choosers
				} else {
					int keep = -1;
					for (k = 0; k < NU; k++) {
						if (!(C->choosers & (1u << k)))
							continue;
						if (keep < 0 || (s->u[k].verified && !s->u[keep].verified) ||
						    (s->u[k].verified == s->u[keep].verified && s->u[k].age > s->u[keep].age))
							keep = k;
					}
					C->user = keep + 1;
					C->choosers = 1u << keep;
				}
			}
			continue;
		}
		if (C->count < MIN_USER_PIXELS / 4)
			continue;
		{
			float h = C->ymax - C->ymin, w = C->xmax - C->xmin, dz = C->zmax - C->zmin;
			float zc = (float)(C->sz / C->count);
			int person = C->count >= MIN_USER_PIXELS && h > 0.4f && h < 2.4f && w > 0.15f && w < 1.6f &&
			             dz < 1.5f && zc < USER_MAX_Z_MM * 0.001f;
			// once the background is known, only something that moved can be a
			// new user: furniture, walls and rejected candidates never did
			if (s->frames > BG_LEARN_FRAMES && C->fg * 3 < C->count)
				person = 0;
			if (!person)
				continue;
			for (k = 0; k < NU && s->u[k].active; k++)
				;
			if (k == NU)
				continue;
			memset(&s->u[k], 0, sizeof(s->u[k]));
			s->u[k].active = 1;
			s->u[k].id = s->next_id++;
			if (s->next_id == 0)
				s->next_id = 1;
			s->u[k].index = k + 1;
			// mostly in front of a background seen empty for a while: it
			// came in, a user at once (even if the forest cannot see a body,
			// cut off at the edge of the view, say). Otherwise the forest has
			// to confirm a body first (nui.c), and if it does not, the
			// candidate becomes background (a chair, a door frame).
			s->u[k].verified = C->fgsure * 2 >= C->count; // (fgsure needs FG_CONF_FRAMES of background)
			C->user = k + 1;
			// what was learned as the room where the new user is may have been
			// the user (sitting there from the start, say): forget it
			{
				int u2, v2;
				for (v2 = C->vmin; v2 <= C->vmax; v2++)
					for (u2 = C->umin; u2 <= C->umax; u2++)
						s->bgconf[v2 * NW + u2] = 0;
			}
		}
	}

	// pixels to users
	for (i = 0; i < NPIX; i++) {
		int c = s->label[i], k2 = 0;
		if (!c || !cs[c].user)
			continue;
		// the empty room, as seen for a while: never a user, even where a
		// user touches it (a hand on the wall, an arm against a cupboard)
		if (s->bgconf[i] >= BG_CONF_FRAMES && abs((int)map[i] - (int)s->bg[i]) < 60 + map[i] / 50)
			continue;
		if (cs[c].user > 0) {
			k2 = cs[c].user - 1;
		} else {
			// merged component: the nearest previous centre in x and z
			v3 p = fn_nui_unproject((float)(i % NW), (float)(i / NW), (float)map[i]);
			float bd = 1e9f;
			for (k = 0; k < NU; k++) {
				float dx, dz2, dd;
				if (!s->u[k].active || !(cs[c].choosers & (1u << k)))
					continue;
				dx = p.x - s->u[k].centre.x;
				dz2 = p.z - s->u[k].centre.z;
				dd = dx * dx + dz2 * dz2;
				if (dd < bd) {
					bd = dd;
					k2 = k;
				}
			}
		}
		s->user[i] = (uint8_t)(k2 + 1);
	}

	// per-user statistics
	{
		double sx[NU] = { 0 }, sy[NU] = { 0 }, sz[NU] = { 0 };
		for (k = 0; k < NU; k++) {
			s->u[k].pixels = 0;
			s->u[k].umin = s->u[k].vmin = 100000;
			s->u[k].umax = s->u[k].vmax = -1;
		}
		for (i = 0; i < NPIX; i++) {
			int u = i % NW, v = i / NW;
			v3 p;
			fn_nui_user_seg* U;
			if (!s->user[i])
				continue;
			U = &s->u[s->user[i] - 1];
			p = fn_nui_unproject((float)u, (float)v, (float)map[i]);
			sx[s->user[i] - 1] += p.x;
			sy[s->user[i] - 1] += p.y;
			sz[s->user[i] - 1] += p.z;
			U->pixels++;
			if (u < U->umin) U->umin = u;
			if (u > U->umax) U->umax = u;
			if (v < U->vmin) U->vmin = v;
			if (v > U->vmax) U->vmax = v;
		}
		for (k = 0; k < NU; k++) {
			fn_nui_user_seg* U = &s->u[k];
			if (!U->active)
				continue;
			if (U->pixels < MIN_USER_PIXELS / 4) {
				if (++U->missing > USER_TIMEOUT)
					memset(U, 0, sizeof(*U));
				continue;
			}
			U->missing = 0;
			U->age++;
			U->centre = v3make((float)(sx[k] / U->pixels), (float)(sy[k] / U->pixels), (float)(sz[k] / U->pixels));
			// Kinect10's size factor for new players (§30.4.7): area from the
			// pixel count at the user's depth, at 320x240
			{
				float z = U->centre.z * 1000.0f, px = z * 0.0017144f * 2.0f;
				float area = px * px * (float)U->pixels * 1e-6f;
				float sc = 0.36392f + 1.41271f * area;
				U->scale = sc < 0.7f ? 0.7f : sc > 1.0f ? 1.0f : sc;
			}
		}
	}

	// near a user (a margin around the box of each) moving limbs come and
	// go: the room seen there is not trusted as empty
	memset(s->near_user, 0, NPIX);
	for (k = 0; k < NU; k++) {
		const fn_nui_user_seg* U = &s->u[k];
		int u0, u1, v0, v1, v2;
		if (!U->active || U->pixels == 0)
			continue;
		u0 = U->umin - NEAR_USER_PX; u1 = U->umax + NEAR_USER_PX;
		v0 = U->vmin - NEAR_USER_PX; v1 = U->vmax + NEAR_USER_PX;
		if (u0 < 0) u0 = 0;
		if (v0 < 0) v0 = 0;
		if (u1 >= NW) u1 = NW - 1;
		if (v1 >= NH) v1 = NH - 1;
		for (v2 = v0; v2 <= v1; v2++)
			memset(s->near_user + v2 * NW + u0, 1, (size_t)(u1 - u0 + 1));
	}
	// background: farthest stable depth; user pixels never enter it
	for (i = 0; i < NPIX; i++) {
		int d = map[i], b = s->bg[i];
		if (!d)
			continue;
		if (s->user[i]) {
			s->bgage[i] = 0;
			continue;
		}
		if (!b) {
			s->bg[i] = (uint16_t)d;
			s->bgage[i] = 0;
			s->bgconf[i] = 0;
		} else if (d > b + 30) {
			if (++s->bgage[i] > BG_FARTHER_FRAMES) { // stays farther: what was in front left
				s->bg[i] = (uint16_t)d;
				s->bgage[i] = 0;
				s->bgconf[i] = 0;
			}
		} else if (d + 100 < b) {
			if (++s->bgage[i] > BG_ABSORB_FRAMES) {
				s->bg[i] = (uint16_t)d; // something that stays put becomes background
				s->bgage[i] = 0;
				s->bgconf[i] = 0;
			}
		} else {
			s->bg[i] = (uint16_t)(b + (d - b) / 32);
			s->bgage[i] = 0;
			// next to a user nothing new is learned, but what was seen of the
			// room before the user came stays (the wall a user steps up to)
			if (!s->near_user[i] && s->bgconf[i] < 65535)
				s->bgconf[i]++;
		}
	}
	memcpy(s->prev_user, s->user, NPIX);
	s->frames++;
}
