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
 * The joint filters (nui/output.c): the SDK's Holt filter, One-Euro and
 * Kalman, on a joint with 2 cm of noise, held still and then moving at
 * 0.5 m/s, and the history starting over for a new user. Needs no model.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "libfreenect_nui.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

static unsigned rng = 12345;
static double gauss(void)
{
	double u, v;
	rng = rng * 1103515245u + 12345u;
	u = ((rng >> 8) + 0.5) / 16777216.0;
	rng = rng * 1103515245u + 12345u;
	v = ((rng >> 8) + 0.5) / 16777216.0;
	return sqrt(-2.0 * log(u)) * cos(6.283185307 * v);
}

static void frame_at(freenect_nui_frame* f, uint32_t id, float x)
{
	int j;
	memset(f, 0, sizeof(*f));
	f->users[0].state = FREENECT_NUI_USER_TRACKED;
	f->users[0].tracking_id = id;
	for (j = 0; j < FREENECT_NUI_JOINTS; j++) {
		f->users[0].joints[j].x = x + (float)(0.02 * gauss());
		f->users[0].joints[j].y = 0.3f + (float)(0.02 * gauss());
		f->users[0].joints[j].z = 2.0f + (float)(0.02 * gauss());
		f->users[0].joint_state[j] = FREENECT_NUI_JOINT_TRACKED;
	}
}

// still for 60 frames, then 0.5 m/s for 60: noise (std) when still, the
// lag (mean x error) and the error when moving, for joint HEAD
static void run(freenect_nui_smoother* s, const char* name, double* still_std, double* lag, double* rms_move, double* raw_std)
{
	freenect_nui_frame f;
	double sx = 0, sxx = 0, rx = 0, rxx = 0, lagsum = 0, e2 = 0;
	int i, n = 0, m = 0;
	rng = 12345;
	for (i = 0; i < 120; i++) {
		float truth = i < 60 ? 0.0f : 0.5f * (i - 60) / 30.0f;
		float raw;
		frame_at(&f, 7, truth);
		raw = f.users[0].joints[FREENECT_NUI_HEAD].x;
		freenect_nui_smooth(s, &f, NULL);
		if (i >= 20 && i < 60) {
			double v = f.users[0].joints[FREENECT_NUI_HEAD].x;
			sx += v; sxx += v * v; rx += raw; rxx += (double)raw * raw;
			n++;
		} else if (i >= 75) {
			double e = f.users[0].joints[FREENECT_NUI_HEAD].x - truth;
			lagsum += e;
			e2 += e * e;
			m++;
		}
	}
	*still_std = sqrt(sxx / n - (sx / n) * (sx / n));
	*raw_std = sqrt(rxx / n - (rx / n) * (rx / n));
	*lag = -lagsum / m;
	*rms_move = sqrt(e2 / m);
	printf("    (%s: still %.1f mm (raw %.1f), moving: lag %.1f mm, rms %.1f mm)\n", name, 1000 * *still_std, 1000 * *raw_std,
	       1000 * *lag, 1000 * *rms_move);
}

int main(void)
{
	freenect_nui_smoother* s = freenect_nui_smoother_create();
	double st, lag, rms, raw;
	freenect_nui_frame f;
	char what[128];
	printf("Joint filters (nui/output.c)\n");

	run(s, "Holt (the SDK's defaults)", &st, &lag, &rms, &raw);
	CHECK(st < raw, "Holt: less noise than the raw joints when still");

	CHECK(freenect_nui_smoother_set_one_euro(s, 1.0f, 1.5f, 1.0f) == 0, "One-Euro selected");
	run(s, "One-Euro", &st, &lag, &rms, &raw);
	snprintf(what, sizeof(what), "  still: noise down to %.0f %% of the raw", 100 * st / raw);
	CHECK(st < 0.5 * raw, what);
	snprintf(what, sizeof(what), "  moving at 0.5 m/s: lag %.0f mm", 1000 * lag);
	CHECK(lag < 0.04 && rms < 0.045, what);

	// Kalman: about half the noise when still, the smallest error when moving
	CHECK(freenect_nui_smoother_set_kalman(s, 3.0f, 0.02f) == 0, "Kalman selected");
	run(s, "Kalman", &st, &lag, &rms, &raw);
	snprintf(what, sizeof(what), "  still: noise down to %.0f %% of the raw", 100 * st / raw);
	CHECK(st < 0.6 * raw, what);
	snprintf(what, sizeof(what), "  moving at 0.5 m/s: lag %.0f mm", 1000 * lag);
	CHECK(fabs(lag) < 0.02 && rms < 0.03, what);

	// a new user in the slot starts without history: the first frame is raw
	frame_at(&f, 8, 1.0f);
	{
		float raw_x = f.users[0].joints[FREENECT_NUI_HEAD].x;
		freenect_nui_smooth(s, &f, NULL);
		CHECK(f.users[0].joints[FREENECT_NUI_HEAD].x == raw_x, "a new user in the slot: no history from the last one");
	}
	CHECK(freenect_nui_smoother_set_kalman(s, 0.0f, 0.02f) < 0 && freenect_nui_smoother_set_one_euro(s, 0.0f, 1.0f, 1.0f) < 0,
	      "invalid parameters are refused");
	freenect_nui_smoother_destroy(s);
	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
