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
 * The tracker: per depth frame
 *   map (mirrored 320x240) -> segmentation (users)
 *   -> for each user that needs a skeleton: the body-part forest on every
 *      one of its pixels (Kinect10 used every second pixel) -> joint
 *      proposals -> fit
 *   -> the SDK's frame: up to 6 users, position only or with a skeleton;
 *      the two nearest get skeletons, with 0.3 m hysteresis (§30.6.5).
 * A user found without moving needs the forest to confirm a body (three
 * good fits within 30 frames) before it is reported.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "nui_internal.h"
#include "models.h"
#include "hands.h"
#include "gestures.h"
#include "headforest.h"
#include "pool.h"
#include <stdio.h>
#include <time.h>

// LIBFREENECT_NUI_PROFILE: the time of each stage, printed when the tracker is destroyed
enum { P_MAP, P_HEADS, P_SEGMENT, P_CLASSIFY, P_PROPOSALS, P_FIT, P_HANDS, P_TOTAL, P_COUNT };
static const char* prof_names[P_COUNT] = { "map", "heads", "segment", "classify", "proposals", "fit", "hands", "total" };

static double prof_now(void)
{
	struct timespec t;
	timespec_get(&t, TIME_UTC);
	return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
#define PROF(n, stage, stmt)                          \
	do {                                              \
		double t0_ = (n)->profile ? prof_now() : 0.0; \
		stmt;                                         \
		if ((n)->profile)                             \
			(n)->prof[stage] += prof_now() - t0_;     \
	} while (0)

#define VERIFY_FRAMES 30
#define LOCK_FRAMES 3
#define MIN_PERSON_HEIGHT 1.0f // m, for a new user (seated, from the seat up)
#define MIN_PERSON_WIDTH 0.25f // m: the shoulders, or a body seen from the side (not a strip of wall)
#define MAX_PERSON_WIDTH 2.1f  // m: wider than the arms spread out
#define STATIC_FRAMES 150      // a user 2 m wide that has not moved for this long (5 s) is the room
#define NEVER_MOVED_FRAMES 300 // a user that never moved in this long (10 s) is not a person (framing ignores it before)
#define FRAME_MOVED 2          // framing follows only users that moved at least this often (not a wall)
#define MOVING_FRAMES 5 // a candidate this often moving its outline: a person
#define LOSE_FRAMES 15
// framing with the tilt motor
#define TILT_HALF_FOV 22.8f   // degrees from the centre to the top or bottom of the view
#define TILT_MARGIN 3.0f      // keep the framed body this far inside the view
#define TILT_MIN_STEP 2.0f    // smaller changes are not worth a move
#define TILT_UNKNOWN_STEP 6.0f // up, for a head out of view and nothing to estimate it from
#define TILT_REST_FRAMES 45   // no new suggestion this soon after a move (1.5 s)
#define TILT_PERSIST_FRAMES 10 // the same move wanted this many frames in a row
#define SETTLE_MIN_FRAMES 8   // a move takes at least this long
#define SETTLE_START_FRAMES 20 // a small move may change little of the view: still after this long, it is over
#define SETTLE_MAX_FRAMES 90  // and is taken as done after this long (3 s)
#define SETTLE_STILL_FRAMES 3 // frames in a row the measured angle stays: the motor stopped
#define SETTLE_STILL_DEG 0.2f // degrees it may change in them
#define TILT_MOVED_MIN 0.75f  // degrees: a smaller measured move is no move (the motor refused)
#define TILT_BLIND_MOVES 2    // moves in a row the same way without seeing the top of the user

struct freenect_nui {
	fn_forest* forest;
	int flags;
	fn_nui_seg seg;
	uint16_t* map;
	uint16_t* umap;          // one user's classifier map (depth << 2 | 1)
	uint8_t* prob;           // 320 x 240 x 31
	fn_nui_fit fit[NU];
	uint32_t fit_id[NU];     // the user id each fit belongs to
	int good[NU], bad[NU], tried[NU];
	int max_tracked;
	uint32_t want[NU];       // app-chosen tracking ids (0: free)
	int nwant;
	uint32_t tracked[NU];    // ids with skeletons this frame
	uint32_t frame;
	uint16_t* depth;         // the last 640x480 frame, for the hands
	int hand_hist[NU][2][5]; // the last hand states, for smoothing
	int hand_pos[NU][2];
	freenect_nui_hand hands[NU][2]; // this frame's hands of tracked users
	uint32_t hands_frame[NU];       // the frame they belong to
	uint32_t hands_id[NU];          // and the user
	v3 hand_dir[NU][2], hand_nrm[NU][2]; // smoothed hand orientation (0: none yet)
	fn_nui_manip manip[NU];
	freenect_nui_manipulation manip_out[NU];
	uint32_t manip_frame[NU];
	freenect_nui_grip grip;          // grips that start a manipulation
	freenect_nui_clasp clasp[NU];    // this frame's clasped hands (with hands_frame)
	fn_nui_pointer ptr[NU][2];       // each hand as a pointer
	freenect_nui_hand_pointer ptr_out[NU][2];
	fn_hd* hd;                       // the head detector (seated mode, or FREENECT_NUI_HEADS), NULL without its model
	fn_pool* pool;                   // threads for the forest
	// the tilt motor (freenect_nui_tilt_changed): while it moves, the last
	// output is repeated; then everything is carried over to the new view
	float tilt_deg;                  // the change on its way (degrees, up)
	int settling;                    // frames since the move began (0: none)
	int still;                       // frames in a row the measured angle stayed, while settling
	float settle_angle;              // the measured angle they stayed at
	uint32_t tilt_frame;             // the frame the last move began
	uint16_t* tilt_map;              // the last map before the move
	float measured_tilt;             // the last move as measured in the depth image (degrees, up)
	int blind_moves;                 // moves in a row one way while the user's top was out of view
	float want_tilt;                 // the last suggestion (freenect_nui_suggest_tilt)
	int want_frames;                 // how many frames in a row it was wanted
	uint32_t want_frame;             // the frame it was asked in
	freenect_nui_frame held;         // the last output
	int profile;
	double prof[P_COUNT];
	unsigned prof_frames;
};

static void track_hands(freenect_nui* n, int k, freenect_nui_skeleton* S);

freenect_nui* freenect_nui_create(const char* models_dir, int flags)
{
	freenect_nui* n;
	uint8_t* db;
	size_t len;
	db = fn_nui_read_model(models_dir, FN_NUI_FOREST_FILE, &len, NULL, 0);
	if (!db)
		return NULL;
	n = (freenect_nui*)calloc(1, sizeof(*n));
	if (!n) {
		free(db);
		return NULL;
	}
	n->forest = fn_forest_load(db, len);
	free(db);
	n->map = (uint16_t*)calloc(NPIX, sizeof(uint16_t));
	n->umap = (uint16_t*)calloc(NPIX, sizeof(uint16_t));
	n->prob = (uint8_t*)calloc((size_t)NPIX * FN_NUI_PARTS, 1);
	n->depth = (uint16_t*)calloc(640 * 480, sizeof(uint16_t));
	n->tilt_map = (uint16_t*)calloc(NPIX, sizeof(uint16_t));
	if (!n->forest || !n->map || !n->umap || !n->prob || !n->depth || !n->tilt_map ||
	    fn_nui_seg_init(&n->seg) < 0) {
		freenect_nui_destroy(n);
		return NULL;
	}
	n->flags = flags;
	n->max_tracked = 2;
	{
		const char* e = getenv("LIBFREENECT_NUI_THREADS");
		int t = e ? atoi(e) : fn_pool_cpus();
		n->pool = fn_pool_create(t < 1 ? 1 : t > 8 && !e ? 8 : t);
	}
	n->profile = getenv("LIBFREENECT_NUI_PROFILE") != NULL;
	n->grip = FREENECT_NUI_GRIP_ANY;
	// the head detector, in seated mode or on request, when its model is there
	if (flags & (FREENECT_NUI_SEATED | FREENECT_NUI_HEADS)) {
		uint8_t* hdb = fn_nui_read_model(models_dir, FN_NUI_HEAD_FOREST_FILE, &len, NULL, 0);
		if (hdb) {
			n->hd = (fn_hd*)calloc(1, sizeof(fn_hd));
			if (n->hd && fn_hd_forest_load(&n->hd->forest, hdb, len) < 0) {
				free(n->hd);
				n->hd = NULL;
			}
			free(hdb);
		}
	}
	return n;
}

void freenect_nui_destroy(freenect_nui* n)
{
	if (!n)
		return;
	if (n->profile && n->prof_frames) {
		int s;
		fprintf(stderr, "freenect_nui: %u frames, %d threads; ms per frame:", n->prof_frames, fn_pool_threads(n->pool));
		for (s = 0; s < P_COUNT; s++)
			fprintf(stderr, " %s %.2f", prof_names[s], n->prof[s] / n->prof_frames);
		fprintf(stderr, "\n");
	}
	fn_pool_destroy(n->pool);
	fn_forest_free(n->forest);
	if (n->hd) {
		fn_hd_forest_free(&n->hd->forest);
		free(n->hd);
	}
	fn_nui_seg_free(&n->seg);
	free(n->map);
	free(n->umap);
	free(n->prob);
	free(n->depth);
	free(n->tilt_map);
	free(n);
}

void freenect_nui_reset(freenect_nui* n)
{
	if (!n)
		return;
	fn_nui_seg_reset(&n->seg);
	memset(n->fit, 0, sizeof(n->fit));
	memset(n->fit_id, 0, sizeof(n->fit_id));
	memset(n->good, 0, sizeof(n->good));
	memset(n->bad, 0, sizeof(n->bad));
	memset(n->tried, 0, sizeof(n->tried));
	memset(n->tracked, 0, sizeof(n->tracked));
	memset(n->hand_hist, 0, sizeof(n->hand_hist));
	memset(n->hands_id, 0, sizeof(n->hands_id));
	memset(n->hands_frame, 0, sizeof(n->hands_frame));
	memset(n->hand_dir, 0, sizeof(n->hand_dir));
	memset(n->hand_nrm, 0, sizeof(n->hand_nrm));
	memset(n->manip, 0, sizeof(n->manip));
	memset(n->ptr, 0, sizeof(n->ptr));
	memset(n->manip_frame, 0, sizeof(n->manip_frame));
	if (n->hd) {
		memset(n->hd->track, 0, sizeof(n->hd->track));
		n->hd->ncand = 0;
	}
	n->frame = 0;
	n->settling = 0;
	n->tilt_deg = 0.0f;
	n->tilt_frame = 0;
	n->measured_tilt = 0.0f;
	n->blind_moves = 0;
	memset(&n->held, 0, sizeof(n->held));
}

int freenect_nui_set_max_tracked(freenect_nui* n, int k)
{
	if (!n || k < 0 || k > NU)
		return -1;
	n->max_tracked = k;
	return 0;
}

int freenect_nui_set_tracked_users(freenect_nui* n, const uint32_t* ids, int k)
{
	if (!n || k < 0 || k > NU)
		return -1;
	memset(n->want, 0, sizeof(n->want));
	n->nwant = ids ? k : 0;
	if (ids)
		memcpy(n->want, ids, sizeof(uint32_t) * (size_t)k);
	return 0;
}

typedef struct {
	freenect_nui* n;
	const float* scale;
	int slot; // user k + 1
} classify_job;

// The forest on rows [y0, y1) of one user's pixels (one pool chunk).
static void classify_rows(void* ctx, int y0, int y1)
{
	const classify_job* J = ctx;
	freenect_nui* n = J->n;
	int i;
	for (i = y0 * NW; i < y1 * NW; i++)
		if (n->seg.user[i] == J->slot)
			fn_forest_classify(n->forest, n->umap, J->scale, i % NW, i / NW, n->prob + (size_t)i * FN_NUI_PARTS);
}

// The forest on every pixel of user slot k, then proposals and the fit.
static int track_user(freenect_nui* n, int k)
{
	fn_nui_user_seg* U = &n->seg.u[k];
	fn_nui_props props;
	float scale[3];
	int i, good;
	scale[0] = U->scale > 0.0f ? U->scale : 1.0f;
	scale[1] = scale[2] = 1.0f;
	for (i = 0; i < NPIX; i++)
		n->umap[i] = n->seg.user[i] == k + 1 ? (uint16_t)((n->map[i] << 2) | 1) : 0;
	{
		classify_job J;
		J.n = n;
		J.scale = scale;
		J.slot = k + 1;
		PROF(n, P_CLASSIFY, fn_pool_run(n->pool, classify_rows, &J, NH, 4));
	}
	PROF(n, P_PROPOSALS, fn_nui_proposals(n->map, n->seg.user, k, n->prob, scale[0], &props, n->pool));
	if (n->fit_id[k] != U->id) {
		fn_nui_fit_reset(&n->fit[k], scale[0]);
		n->fit_id[k] = U->id;
		n->good[k] = n->bad[k] = n->tried[k] = 0;
	}
	// A user that came in, or a candidate that keeps moving (arms, hands),
	// is a person even when the view cuts off the head (standing close):
	// the fit may then take it from a shoulder or an elbow and the hips, as
	// for a user whose upper body it has seen. A cupboard at the edge never
	// moves.
	if ((U->verified || U->moving >= MOVING_FRAMES) && n->fit[k].body_seen < 3)
		n->fit[k].body_seen = 3;
	PROF(n, P_FIT, good = fn_nui_fit_user(&n->fit[k], &props, n->map, n->seg.user, k, scale[0]));
	n->tried[k]++;
	if (good) {
		n->good[k]++;
		n->bad[k] = 0;
	} else {
		n->bad[k]++;
		if (n->bad[k] > LOSE_FRAMES)
			n->good[k] = 0;
	}
	return good;
}

static int wanted(const freenect_nui* n, uint32_t id)
{
	int i;
	for (i = 0; i < n->nwant; i++)
		if (n->want[i] == id)
			return 1;
	return 0;
}

static int was_tracked(const freenect_nui* n, uint32_t id)
{
	int i;
	for (i = 0; i < NU; i++)
		if (n->tracked[i] == id)
			return 1;
	return 0;
}

/* ------------------------------------------------------------ tilt */

float freenect_nui_tilt_measured(freenect_nui* n)
{
	return n ? n->measured_tilt : 0.0f;
}

int freenect_nui_tilt_changed(freenect_nui* n, float degrees)
{
	if (!n || !(degrees > -60.0f && degrees < 60.0f))
		return -1;
	if (!n->settling)
		memcpy(n->tilt_map, n->map, NPIX * sizeof(uint16_t)); // the view before the move
	n->tilt_deg += degrees;
	n->measured_tilt = NAN;
	n->settling = 1;
	n->still = 0;
	n->settle_angle = 0.0f;
	n->tilt_frame = n->frame;
	return 0;
}

// The move is over: background, floor, users and hands to the new view.
// Each fit starts afresh (its user keeps its id and its skeleton): a move
// is usually made because the view cut the body off, and what the fit
// learned from that view (bone lengths above all) does not hold for the
// new one.
static void tilt_done(freenect_nui* n)
{
	float a = n->tilt_deg * 3.14159265f / 180.0f, c = cosf(a), sn = sinf(a);
	int k, h;
	fn_nui_seg_tilt(&n->seg, a, n->tilt_map, n->map);
	for (k = 0; k < NU; k++) {
		const fn_nui_user_seg* U = &n->seg.u[k];
		fn_nui_fit_reset(&n->fit[k], U->scale > 0.0f ? U->scale : 1.0f);
		for (h = 0; h < 2; h++) {
			n->hand_dir[k][h] = fn_nui_tilt_point(n->hand_dir[k][h], c, sn);
			n->hand_nrm[k][h] = fn_nui_tilt_point(n->hand_nrm[k][h], c, sn);
			fn_nui_pointer_reset(&n->ptr[k][h]); // no swipe from the view moving
		}
		fn_nui_manip_reset(&n->manip[k]);
	}
	if (n->hd) {
		memset(n->hd->track, 0, sizeof(n->hd->track));
		n->hd->ncand = 0;
	}
	n->tilt_deg = 0.0f;
	n->settling = 0;
}

// How far the view turned: the angle that best maps the view before the
// move onto this one (most of a room stands still). The motor does not move
// by the angle it is told: a 19-degree move turns the Kinect by about 17, one
// near the end of its range by much less, and carrying the background over
// by the wrong angle turns walls and floor into users.
// Only points where the depth changes from row to row tell how far the view
// turned up or down (the floor, edges, furniture): a flat wall facing the
// Kinect looks the same a few degrees higher or lower.
static float score_tilt(const freenect_nui* n, float deg, int structured)
{
	float a = deg * 3.14159265f / 180.0f, c = cosf(a), sn = sinf(a);
	int u, v, both = 0, same = 0;
	for (v = 3; v < NH - 2; v += 3)
		for (u = 1; u < NW; u += 3) {
			int d = n->tilt_map[v * NW + u], x, y, now, up, down;
			float u2, v2;
			v3 p;
			if (!d)
				continue;
			up = n->tilt_map[(v - 2) * NW + u];
			down = n->tilt_map[(v + 2) * NW + u];
			if (structured && (!up || !down || abs(up - down) < 8 + d / 100))
				continue;
			p = fn_nui_tilt_point(fn_nui_unproject((float)u, (float)v, (float)d), c, sn);
			if (p.z <= 0.1f)
				continue;
			fn_nui_project(p, &u2, &v2);
			x = (int)lrintf(u2);
			y = (int)lrintf(v2);
			if (x < 0 || x >= NW || y < 0 || y >= NH || !(now = n->map[y * NW + x]))
				continue;
			both++;
			same += abs(now - (int)(p.z * 1000.0f)) < 15 + now / 80;
		}
	return both > 200 ? (float)same / (float)both : -1.0f;
}

// the best angle from lo to hi in steps; of angles that score the same (to
// within TILT_SCORE_TIE), the one nearest the angle the motor was told
#define TILT_SCORE_TIE 0.01f
static float best_tilt(const freenect_nui* n, float lo, float hi, float step, float told, int structured, float* score)
{
	float sc[128], a, bs = -1.0f, best = lo;
	int i, m = 0, found = 0;
	for (a = lo; a <= hi + step * 0.1f && m < 128; a += step)
		sc[m++] = score_tilt(n, a, structured);
	for (i = 0; i < m; i++)
		if (sc[i] > bs)
			bs = sc[i];
	for (i = 0; i < m; i++) {
		a = lo + step * (float)i;
		if (sc[i] >= bs - TILT_SCORE_TIE && (!found || fabsf(a - told) < fabsf(best - told))) {
			best = a;
			found = 1;
		}
	}
	*score = bs;
	return best;
}

static float measure_tilt(const freenect_nui* n, float told)
{
	float lo = (told < 0.0f ? told : 0.0f) - 4.0f, hi = (told > 0.0f ? told : 0.0f) + 4.0f, best, bs, at0;
	int structured = score_tilt(n, 0.0f, 1) >= 0.0f; // enough of them
	at0 = score_tilt(n, 0.0f, structured);
	best = best_tilt(n, lo, hi, 0.5f, told, structured, &bs);
	best = best_tilt(n, best - 0.4f, best + 0.4f, 0.1f, told, structured, &bs);
	// no better than not moving at all: it did not move
	if (bs < at0 + 0.05f)
		return 0.0f;
	return best;
}

// One frame while the motor moves: 1 to hold the output, 0 when the move
// is over (and carried over) and the frame is to be processed.
// The move is over when the measured angle stops changing: a slow turn
// changes a flat wall too little from one frame to the next to tell.
static int settle_step(freenect_nui* n)
{
	float m = measure_tilt(n, n->tilt_deg);
	if (n->settling > 1 && fabsf(m - n->settle_angle) < SETTLE_STILL_DEG) {
		n->still++;
	} else {
		n->settle_angle = m;
		n->still = 0;
	}
	n->settling++;
	if ((n->settling > SETTLE_MIN_FRAMES && n->still >= SETTLE_STILL_FRAMES &&
	     (fabsf(m) >= TILT_MOVED_MIN || n->settling > SETTLE_START_FRAMES)) ||
	    n->settling > SETTLE_MAX_FRAMES) {
		// over (or never came to rest): carry over the move as measured; none
		// at all when the motor refused (tipped over, or too many moves)
		n->measured_tilt = m;
		if (fabsf(n->measured_tilt) >= TILT_MOVED_MIN) {
			n->tilt_deg = n->measured_tilt;
			tilt_done(n);
		} else
			n->measured_tilt = 0.0f;
		n->settling = 0;
		n->tilt_deg = 0.0f;
		return 0;
	}
	return 1;
}

// the elevation (degrees, up) of a point seen from the Kinect
static float elevation(v3 p)
{
	return atan2f(p.y, p.z) * 57.29578f;
}

// the elevation of a row of the map
static float row_elevation(float v)
{
	return atanf((FN_NUI_CY - v) / FN_NUI_F) * 57.29578f;
}

// a user at least this wide (m): a person, not a strip of wall along an
// edge where the depth flickers
static int wide_enough(const fn_nui_user_seg* U)
{
	return (float)(U->umax - U->umin + 1) * U->centre.z / FN_NUI_F >= MIN_PERSON_WIDTH;
}

static float suggest_raw(freenect_nui* n, freenect_nui_framing mode, int* blind)
{
	int k, best = -1;
	float bd = 1e9f, top, bot, lim = TILT_HALF_FOV - TILT_MARGIN, delta;
	const fn_nui_fit* F;
	const fn_nui_user_seg* U;
	// the nearest user whose skeleton fitted in this frame (a failed fit's
	// joints say nothing about where the body is)
	for (k = 0; k < NU; k++) {
		const fn_nui_user_seg* u = &n->seg.u[k];
		float d;
		if (!u->active || !u->verified || u->pixels == 0 || (u->moving < FRAME_MOVED && !u->came_in) || n->fit_id[k] != u->id ||
		    n->good[k] < LOCK_FRAMES ||
		    n->bad[k] != 0)
			continue;
		d = fn_nui_v3len(u->centre);
		if (d < bd) {
			bd = d;
			best = k;
		}
	}
	if (best < 0) {
		// no skeleton yet: someone moving whose head is above the view
		for (k = 0; k < NU; k++) {
			const fn_nui_user_seg* u = &n->seg.u[k];
			if (u->active && u->pixels > 0 && u->vmin <= 1 && wide_enough(u) && (u->moving >= MOVING_FRAMES || u->came_in)) {
				*blind = 1;
				return TILT_UNKNOWN_STEP;
			}
		}
		return 0.0f;
	}
	F = &n->fit[best];
	U = &n->seg.u[best];
	// the top: the user's outline, or where the head must be when the view
	// cuts it off
	*blind = U->vmin <= 1;
	if (U->vmin > 1)
		top = row_elevation((float)U->vmin);
	else if (F->jstate[4] == 2 || F->jstate[8] == 2) {
		// the top of the head about 30 cm above the shoulders
		v3 sh = F->jstate[4] == 2 && F->jstate[8] == 2 ? v3scale(v3add(F->joint[4], F->joint[8]), 0.5f)
		                                                : F->joint[F->jstate[4] == 2 ? 4 : 8];
		top = elevation(v3add(sh, v3make(0.0f, 0.30f, 0.0f)));
	} else if (F->jstate[2])
		top = elevation(v3add(F->joint[2], v3make(0.0f, 0.40f, 0.0f))); // or 40 cm above the shoulder centre
	else
		return TILT_UNKNOWN_STEP;
	if (mode == FREENECT_NUI_FRAME_UPPER_BODY || (n->flags & FREENECT_NUI_SEATED)) {
		// down to below the hips, and the hands wherever they are
		float y = F->joint[0].y - 0.12f;
		v3 p = v3make(F->joint[0].x, y, F->joint[0].z);
		int hand;
		bot = elevation(p);
		for (hand = 7; hand <= 11; hand += 4)
			if (F->jstate[hand] && elevation(F->joint[hand]) - 3.0f < bot)
				bot = elevation(F->joint[hand]) - 3.0f;
	} else if (U->vmax < NH - 2) {
		bot = row_elevation((float)U->vmax);
	} else if (n->seg.floor_valid) {
		// the feet: the floor below the user
		const float* pl = n->seg.floor;
		v3 c = U->centre;
		float y = -(pl[0] * c.x * 1000.0f + pl[2] * c.z * 1000.0f + pl[3]) / (pl[1] * 1000.0f);
		bot = elevation(v3make(c.x, y, c.z));
	} else {
		bot = elevation(v3add(F->joint[0], v3make(0.0f, -0.95f, 0.0f)));
	}
	if (top <= lim + 0.5f && bot >= -lim - 0.5f)
		return 0.0f; // in view
	// centred if it fits, else the head at the top
	delta = top - bot <= 2.0f * lim ? (top + bot) * 0.5f : top - lim;
	if (fabsf(delta) < TILT_MIN_STEP)
		return 0.0f;
	return floorf(delta + 0.5f);
}

// A move only when the same is wanted for TILT_PERSIST_FRAMES frames in a
// row (raising the arms for a moment is no reason to move the motor).
float freenect_nui_suggest_tilt(freenect_nui* n, freenect_nui_framing mode)
{
	float d;
	int blind = 0;
	if (!n)
		return 0.0f;
	if (n->settling || (n->tilt_frame && n->frame - n->tilt_frame < TILT_REST_FRAMES)) {
		n->want_frames = 0;
		return 0.0f;
	}
	d = suggest_raw(n, mode, &blind);
	// Moves that cannot see where the user ends (the top out of view) stop
	// after TILT_BLIND_MOVES the same way: a person's head comes into view by
	// then, but what grows with every move up (a wall taken for a user, a
	// tall cupboard) would take the Kinect to the end of its range.
	if (!blind)
		n->blind_moves = 0;
	else if (d != 0.0f && n->blind_moves >= TILT_BLIND_MOVES)
		d = 0.0f;
	if (d == 0.0f || (d > 0.0f) != (n->want_tilt > 0.0f) || n->want_frame + 1 != n->frame)
		n->want_frames = 0;
	n->want_frames += d != 0.0f;
	n->want_tilt = d;
	n->want_frame = n->frame;
	if (n->want_frames < TILT_PERSIST_FRAMES)
		return 0.0f;
	n->blind_moves = blind ? n->blind_moves + 1 : 0;
	return d;
}

int freenect_nui_process(freenect_nui* n, const uint16_t* depth, uint32_t timestamp, freenect_nui_frame* out)
{
	int k, chosen[NU], fitted[NU], nchosen = 0;
	uint32_t now_tracked[NU];
	double t_total;
	if (!n || !depth || !out)
		return -1;
	t_total = n->profile ? prof_now() : 0.0;
	memcpy(n->depth, depth, 640 * 480 * sizeof(uint16_t));
	PROF(n, P_MAP, fn_nui_make_map(depth, n->map));
	if (n->settling && settle_step(n)) {
		// the motor is still moving: the last users and skeletons again
		*out = n->held;
		out->frame_number = n->frame++;
		out->timestamp = timestamp;
		return 0;
	}
	if (n->hd) {
		PROF(n, P_HEADS, fn_hd_detect(n->hd, n->map); fn_hd_track_update(n->hd));
	}
	PROF(n, P_SEGMENT, fn_nui_segment(&n->seg, n->map, n->flags & FREENECT_NUI_SEATED));

	// What never moves is not a person, whatever the forest saw in it: a
	// strip of wall along an edge where the depth flickers, taken in the
	// first frames, before the room was known. And no person is 2 m wide: a
	// user that is, and has stood still for a while, is the wall a user left
	// behind (it walked away from where the room had not been learned).
	for (k = 0; k < NU; k++) {
		fn_nui_user_seg* U = &n->seg.u[k];
		if (U->active && U->verified && U->pixels &&
		    ((U->moving == 0 && !U->came_in && U->age >= NEVER_MOVED_FRAMES) ||
		     (U->still >= STATIC_FRAMES && (float)(U->umax - U->umin + 1) * U->centre.z / FN_NUI_F > MAX_PERSON_WIDTH))) {
			fn_nui_seg_reject(&n->seg, k, n->map);
			n->fit_id[k] = 0;
		}
	}

	// users found without motion: the forest has to see a body
	memset(fitted, 0, sizeof(fitted));
	for (k = 0; k < NU; k++) {
		fn_nui_user_seg* U = &n->seg.u[k];
		if (!U->active || U->verified || U->pixels == 0)
			continue;
		track_user(n, k);
		fitted[k] = 1;
		// a person is at least 1 m tall from the seat or the floor up, unless
		// the view cuts it off at the top, or at the bottom below a head the
		// fit tracks (someone close, with the Kinect tilted up): a chair or a
		// box is not
		if (n->good[k] >= LOCK_FRAMES && wide_enough(U) &&
		    (U->vmin <= 1 || (U->vmax - U->vmin) * U->centre.z / FN_NUI_F >= MIN_PERSON_HEIGHT ||
		     (U->vmax >= NH - 2 && n->fit[k].jstate[3] == 2)))
			U->verified = 1;
		else if (n->tried[k] >= VERIFY_FRAMES && U->moving * 4 < n->tried[k])
			fn_nui_seg_reject(&n->seg, k, n->map); // not a person: remembered as background
		// (one that keeps moving keeps being tried: a person the forest
		// cannot see well yet, too close or turned away)
	}

	// which users get skeletons: chosen by the app, else the nearest
	memset(chosen, 0, sizeof(chosen));
	if (n->nwant) {
		for (k = 0; k < NU && nchosen < n->max_tracked; k++)
			if (n->seg.u[k].active && n->seg.u[k].verified && wanted(n, n->seg.u[k].id))
				chosen[k] = 1, nchosen++;
	} else {
		while (nchosen < n->max_tracked) {
			int best = -1;
			float bd = 1e9f;
			for (k = 0; k < NU; k++) {
				fn_nui_user_seg* U = &n->seg.u[k];
				float d;
				if (!U->active || !U->verified || chosen[k] || U->pixels == 0)
					continue;
				d = fn_nui_v3len(U->centre) - (was_tracked(n, U->id) ? 0.3f : 0.0f);
				if (d < bd) {
					bd = d;
					best = k;
				}
			}
			if (best < 0)
				break;
			chosen[best] = 1;
			nchosen++;
		}
	}

	memset(out, 0, sizeof(*out));
	memset(now_tracked, 0, sizeof(now_tracked));
	out->frame_number = n->frame++;
	out->timestamp = timestamp;
	if (n->seg.floor_valid) {
		out->floor_plane[0] = n->seg.floor[0];
		out->floor_plane[1] = n->seg.floor[1];
		out->floor_plane[2] = n->seg.floor[2];
		out->floor_plane[3] = n->seg.floor[3] * 0.001f;
	}
	for (k = 0; k < NU; k++) {
		fn_nui_user_seg* U = &n->seg.u[k];
		freenect_nui_skeleton* S = &out->users[k];
		int j;
		if (!U->active || !U->verified || U->pixels == 0)
			continue;
		S->state = FREENECT_NUI_USER_POSITION_ONLY;
		S->tracking_id = U->id;
		S->position.x = U->centre.x;
		S->position.y = U->centre.y;
		S->position.z = U->centre.z;
		if (!chosen[k])
			continue;
		if (!fitted[k]) // not already fitted by the verification above
			track_user(n, k);
		now_tracked[k] = U->id;
		if (n->good[k] < LOCK_FRAMES)
			continue;
		S->state = FREENECT_NUI_USER_TRACKED;
		S->quality = n->fit[k].quality;
		for (j = 0; j < NJ; j++) {
			int seated_hidden = (n->flags & FREENECT_NUI_SEATED) && (j < 2 || j >= 12);
			if (seated_hidden)
				continue;
			S->joints[j].x = n->fit[k].joint[j].x;
			S->joints[j].y = n->fit[k].joint[j].y;
			S->joints[j].z = n->fit[k].joint[j].z;
			S->joint_state[j] = (freenect_nui_joint_state)n->fit[k].jstate[j];
		}
		if (n->hands_id[k] != U->id) {
			memset(n->hand_hist[k], 0, sizeof(n->hand_hist[k]));
			memset(n->hand_dir[k], 0, sizeof(n->hand_dir[k]));
			memset(n->hand_nrm[k], 0, sizeof(n->hand_nrm[k]));
			fn_nui_manip_reset(&n->manip[k]);
			fn_nui_pointer_reset(&n->ptr[k][0]);
			fn_nui_pointer_reset(&n->ptr[k][1]);
			n->hands_id[k] = U->id;
		}
		PROF(n, P_HANDS, track_hands(n, k, S));
		// the hands clasped together: one region, one state for both; each
		// hand's own fingers are then not separable
		fn_nui_clasp_analyze(n->depth, n->fit[k].joint[7], n->fit[k].joint[11], n->fit[k].joint[6], n->fit[k].joint[10],
		                     n->fit[k].joint[2], &n->clasp[k]);
		if (n->clasp[k].clasped) {
			int h, f;
			S->hands_clasped = 1;
			for (h = 0; h < 2; h++) {
				int first = h ? FREENECT_NUI_PALM_RIGHT : FREENECT_NUI_PALM_LEFT;
				S->hand_state[h] = n->clasp[k].state;
				for (f = 1; f <= 5; f++)
					S->joint_state[first + f] = FREENECT_NUI_JOINT_NOT_TRACKED;
				if (n->clasp[k].state == FREENECT_NUI_HAND_POINTING || n->clasp[k].state == FREENECT_NUI_HAND_GUN) {
					S->joints[first + 2] = n->clasp[k].tip; // the index fingers, together
					S->joint_state[first + 2] = FREENECT_NUI_JOINT_TRACKED;
				}
			}
		}
		// two-hand manipulation; a gap in tracking starts it over
		{
			const freenect_nui_hand* H = n->hands[k];
			v3 L = v3make(H[0].palm.x, H[0].palm.y, H[0].palm.z), R = v3make(H[1].palm.x, H[1].palm.y, H[1].palm.z);
			freenect_nui_grip g;
			if (n->manip_frame[k] + 1 != n->frame) {
				fn_nui_manip_reset(&n->manip[k]);
				fn_nui_pointer_reset(&n->ptr[k][0]);
				fn_nui_pointer_reset(&n->ptr[k][1]);
			}
			g = fn_nui_grip_of(n->grip, S->joint_state[FREENECT_NUI_PALM_LEFT] == FREENECT_NUI_JOINT_TRACKED,
			                   S->joint_state[FREENECT_NUI_PALM_RIGHT] == FREENECT_NUI_JOINT_TRACKED, H[0].state, H[1].state, L,
			                   R, v3make(H[0].normal.x, H[0].normal.y, H[0].normal.z),
			                   v3make(H[1].normal.x, H[1].normal.y, H[1].normal.z),
			                   v3make(H[0].fingertips[0].x, H[0].fingertips[0].y, H[0].fingertips[0].z),
			                   v3make(H[1].fingertips[0].x, H[1].fingertips[0].y, H[1].fingertips[0].z), n->fit[k].joint[2]);
			if (n->clasp[k].clasped)
				g = FREENECT_NUI_GRIP_NONE;
			fn_nui_manip_step(&n->manip[k], g, L, R, v3make(H[0].direction.x, H[0].direction.y, H[0].direction.z),
			                  v3make(H[1].direction.x, H[1].direction.y, H[1].direction.z), &n->manip_out[k]);
			n->manip_frame[k] = n->frame;
		}
		// each hand as a pointer, from the body's hand joints; quiet while
		// both hands do something together
		{
			const fn_nui_fit* F = &n->fit[k];
			int quiet = n->manip_out[k].active || n->manip_out[k].ended || n->clasp[k].clasped, h;
			freenect_nui_hand_pointer* P = n->ptr_out[k];
			for (h = 0; h < 2; h++)
				fn_nui_pointer_step(&n->ptr[k][h], h, F->jstate[h ? 11 : 7] != 0, F->joint[h ? 11 : 7], S->hand_state[h],
				                    F->joint[h ? 8 : 4], F->joint[2], F->joint[0], quiet, &P[h]);
			// the primary hand: the active one; of two, the one further forward
			if (P[0].active && P[1].active)
				P[F->joint[11].z < F->joint[7].z ? 1 : 0].primary = 1;
			else if (P[0].active || P[1].active)
				P[P[1].active ? 1 : 0].primary = 1;
		}
	}
	memcpy(n->tracked, now_tracked, sizeof(now_tracked));
	if (n->profile) {
		n->prof[P_TOTAL] += prof_now() - t_total;
		n->prof_frames++;
	}
	n->held = *out;
	return 0;
}

int freenect_nui_get_player_index(freenect_nui* n, uint8_t* out)
{
	int X, Y;
	if (!n || !out)
		return -1;
	// back to libfreenect's orientation: depth column X is map column (639 - X) / 2
	for (Y = 0; Y < 480; Y++)
		for (X = 0; X < 640; X++) {
			int i = (Y / 2) * NW + (639 - X) / 2, p = n->seg.user[i];
			out[Y * 640 + X] = (uint8_t)((p && n->seg.u[p - 1].active && n->seg.u[p - 1].verified) ? p : 0);
		}
	return 0;
}

// Both hands of tracked user k from this frame's full-resolution depth, into
// the skeleton's hand joints (PALM, THUMB .. LITTLE) and hand states. The
// state is the most frequent of the last 5 frames.
static void track_hands(freenect_nui* n, int k, freenect_nui_skeleton* S)
{
	static const int hand_joint[2] = { 7, 11 }, wrist_joint[2] = { 6, 10 }, first[2] = { 20, 26 };
	const fn_nui_fit* F = &n->fit[k];
	int h;
	for (h = 0; h < 2; h++) {
		freenect_nui_hand* H = &n->hands[k][h];
		int counts[5] = { 0, 0, 0, 0, 0 }, i, best = 0, f;
		fn_nui_hand_analyze(n->depth, F->joint[hand_joint[h]], F->joint[wrist_joint[h]], F->joint[2], H);
		n->hand_hist[k][h][n->hand_pos[k][h]] = (int)H->state;
		n->hand_pos[k][h] = (n->hand_pos[k][h] + 1) % 5;
		for (i = 0; i < 5; i++)
			counts[n->hand_hist[k][h][i] % 5]++;
		for (i = 1; i < 5; i++)
			if (counts[i] > counts[best])
				best = i;
		if (H->state != FREENECT_NUI_HAND_UNKNOWN && counts[best] >= 3)
			H->state = (freenect_nui_hand_state)best;
		S->hand_state[h] = H->state;
		S->hand_holding[h] = H->holding;
		// orientation, smoothed over frames (the hand's own pixels are noisy)
		if (H->state != FREENECT_NUI_HAND_UNKNOWN && (H->normal.x != 0.0f || H->normal.y != 0.0f || H->normal.z != 0.0f)) {
			v3 d = v3make(H->direction.x, H->direction.y, H->direction.z), m = v3make(H->normal.x, H->normal.y, H->normal.z);
			v3* sd = &n->hand_dir[k][h];
			v3* sn = &n->hand_nrm[k][h];
			if (fn_nui_v3len(*sd) > 0.0f) {
				if (v3dot(m, *sn) < 0.0f) // the normal's sign is a guess edge-on: keep it continuous
					m = v3scale(m, -1.0f);
				d = fn_nui_v3norm(v3add(*sd, v3scale(v3sub(d, *sd), 0.5f)));
				m = fn_nui_v3norm(v3add(*sn, v3scale(v3sub(m, *sn), 0.5f)));
			}
			*sd = d;
			*sn = m;
			fn_nui_rotation_from_axes(d, m, &H->orientation);
			d = v3make(H->orientation.matrix[1][0], H->orientation.matrix[1][1], H->orientation.matrix[1][2]);
			m = v3make(H->orientation.matrix[2][0], H->orientation.matrix[2][1], H->orientation.matrix[2][2]);
			H->direction.x = d.x; H->direction.y = d.y; H->direction.z = d.z;
			H->normal.x = m.x; H->normal.y = m.y; H->normal.z = m.z;
		}
		S->hand_orientation[h] = H->orientation;
		if (H->state != FREENECT_NUI_HAND_UNKNOWN) {
			S->joints[first[h]] = H->palm;
			S->joint_state[first[h]] = FREENECT_NUI_JOINT_TRACKED;
		}
		for (f = 0; f < 5; f++)
			if (H->finger_found[f]) {
				S->joints[first[h] + 1 + f] = H->fingertips[f];
				S->joint_state[first[h] + 1 + f] = FREENECT_NUI_JOINT_TRACKED;
			}
	}
	n->hands_frame[k] = n->frame;
}

int freenect_nui_get_hands(freenect_nui* n, int user, freenect_nui_hand hands[2])
{
	if (!n || !hands || user < 0 || user >= NU || !n->seg.u[user].active || n->hands_frame[user] != n->frame ||
	    n->hands_id[user] != n->seg.u[user].id) {
		if (hands)
			memset(hands, 0, 2 * sizeof(freenect_nui_hand));
		return -1;
	}
	memcpy(hands, n->hands[user], 2 * sizeof(freenect_nui_hand));
	return 0;
}

int freenect_nui_set_threads(freenect_nui* n, int threads)
{
	fn_pool* p;
	if (!n || threads < 0)
		return -1;
	if (threads == 0)
		threads = fn_pool_cpus() > 8 ? 8 : fn_pool_cpus();
	p = fn_pool_create(threads);
	if (!p)
		return -1;
	fn_pool_destroy(n->pool);
	n->pool = p;
	return fn_pool_threads(p);
}

int freenect_nui_get_heads(freenect_nui* n, freenect_nui_head heads[FREENECT_NUI_MAX_HEADS])
{
	if (!n || !heads)
		return -1;
	if (!n->hd) {
		memset(heads, 0, FREENECT_NUI_MAX_HEADS * sizeof(freenect_nui_head));
		return -1;
	}
	return fn_hd_output(n->hd, heads);
}

int freenect_nui_get_hand_pointers(freenect_nui* n, int user, freenect_nui_hand_pointer out[2])
{
	if (!n || !out || user < 0 || user >= NU || !n->seg.u[user].active || n->manip_frame[user] != n->frame ||
	    n->hands_id[user] != n->seg.u[user].id) {
		if (out)
			memset(out, 0, 2 * sizeof(freenect_nui_hand_pointer));
		return -1;
	}
	memcpy(out, n->ptr_out[user], 2 * sizeof(freenect_nui_hand_pointer));
	return 0;
}

int freenect_nui_get_clasp(freenect_nui* n, int user, freenect_nui_clasp* out)
{
	if (!n || !out || user < 0 || user >= NU || !n->seg.u[user].active || n->hands_frame[user] != n->frame ||
	    n->hands_id[user] != n->seg.u[user].id) {
		if (out)
			memset(out, 0, sizeof(*out));
		return -1;
	}
	*out = n->clasp[user];
	return 0;
}

int freenect_nui_get_manipulation(freenect_nui* n, int user, freenect_nui_manipulation* out)
{
	if (!n || !out || user < 0 || user >= NU || !n->seg.u[user].active || n->manip_frame[user] != n->frame ||
	    n->hands_id[user] != n->seg.u[user].id) {
		if (out) {
			memset(out, 0, sizeof(*out));
			out->scale = out->total_scale = 1.0f;
			out->rotation[3] = out->total_rotation[3] = 1.0f;
		}
		return -1;
	}
	*out = n->manip_out[user];
	return 0;
}

int freenect_nui_set_grip(freenect_nui* n, freenect_nui_grip grip)
{
	if (!n || grip < FREENECT_NUI_GRIP_CLOSED || grip > FREENECT_NUI_GRIP_ANY)
		return -1;
	n->grip = grip;
	return 0;
}
