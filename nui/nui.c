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

#include <stdlib.h>
#include <string.h>
#include "nui_internal.h"
#include "models.h"
#include "hands.h"

#define VERIFY_FRAMES 30
#define LOCK_FRAMES 3
#define LOSE_FRAMES 15

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
	if (!n->forest || !n->map || !n->umap || !n->prob || !n->depth || fn_nui_seg_init(&n->seg) < 0) {
		freenect_nui_destroy(n);
		return NULL;
	}
	n->flags = flags;
	n->max_tracked = 2;
	return n;
}

void freenect_nui_destroy(freenect_nui* n)
{
	if (!n)
		return;
	fn_forest_free(n->forest);
	fn_nui_seg_free(&n->seg);
	free(n->map);
	free(n->umap);
	free(n->prob);
	free(n->depth);
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
	n->frame = 0;
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
	for (i = 0; i < NPIX; i++)
		if (n->seg.user[i] == k + 1)
			fn_forest_classify(n->forest, n->umap, scale, i % NW, i / NW, n->prob + (size_t)i * FN_NUI_PARTS);
	fn_nui_proposals(n->map, n->seg.user, k, n->prob, scale[0], &props);
	if (n->fit_id[k] != U->id) {
		fn_nui_fit_reset(&n->fit[k], scale[0]);
		n->fit_id[k] = U->id;
		n->good[k] = n->bad[k] = n->tried[k] = 0;
	}
	good = fn_nui_fit_user(&n->fit[k], &props, n->map, n->seg.user, k, scale[0]);
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

int freenect_nui_process(freenect_nui* n, const uint16_t* depth, uint32_t timestamp, freenect_nui_frame* out)
{
	int k, chosen[NU], fitted[NU], nchosen = 0;
	uint32_t now_tracked[NU];
	if (!n || !depth || !out)
		return -1;
	memcpy(n->depth, depth, 640 * 480 * sizeof(uint16_t));
	fn_nui_make_map(depth, n->map);
	fn_nui_segment(&n->seg, n->map, n->flags & FREENECT_NUI_SEATED);

	// users found without motion: the forest has to see a body
	memset(fitted, 0, sizeof(fitted));
	for (k = 0; k < NU; k++) {
		fn_nui_user_seg* U = &n->seg.u[k];
		if (!U->active || U->verified || U->pixels == 0)
			continue;
		track_user(n, k);
		fitted[k] = 1;
		if (n->good[k] >= LOCK_FRAMES)
			U->verified = 1;
		else if (n->tried[k] >= VERIFY_FRAMES)
			U->active = 0; // not a person
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
			n->hands_id[k] = U->id;
		}
		track_hands(n, k, S);
	}
	memcpy(n->tracked, now_tracked, sizeof(now_tracked));
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
