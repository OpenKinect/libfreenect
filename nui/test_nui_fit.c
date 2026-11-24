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
 * The skeleton fit's joint limits (nui/fit.c): joint proposals of a
 * standing person, with a knee proposal confidently behind the leg and a
 * wrist proposal folded onto the shoulder. The fit must keep the knee from
 * bending backwards and the elbow from folding shut. Needs no model.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "nui_internal.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

// a person standing 2.5 m away, facing the Kinect, arms down
static void standing(v3 J[NJ])
{
	float x0 = 0.0f, z0 = 2.5f, hip = -0.05f;
	J[0] = v3make(x0, hip, z0);
	J[2] = v3make(x0, hip + 0.46f, z0);
	J[1] = v3add(J[0], v3scale(v3sub(J[2], J[0]), 1.0f / 6.0f));
	J[3] = v3make(x0, hip + 0.68f, z0);
	J[4] = v3make(x0 - 0.17f, hip + 0.44f, z0);
	J[5] = v3make(x0 - 0.30f, hip + 0.20f, z0);
	J[6] = v3make(x0 - 0.38f, hip - 0.04f, z0 - 0.05f);
	J[7] = v3make(x0 - 0.41f, hip - 0.12f, z0 - 0.07f);
	J[8] = v3make(x0 + 0.17f, hip + 0.44f, z0);
	J[9] = v3make(x0 + 0.30f, hip + 0.20f, z0);
	J[10] = v3make(x0 + 0.38f, hip - 0.04f, z0 - 0.05f);
	J[11] = v3make(x0 + 0.41f, hip - 0.12f, z0 - 0.07f);
	J[12] = v3make(x0 - 0.09f, hip - 0.03f, z0);
	J[13] = v3make(x0 - 0.11f, hip - 0.45f, z0 - 0.03f);
	J[14] = v3make(x0 - 0.11f, -0.87f, z0);
	J[15] = v3make(x0 - 0.12f, -0.91f, z0 - 0.12f);
	J[16] = v3make(x0 + 0.09f, hip - 0.03f, z0);
	J[17] = v3make(x0 + 0.11f, hip - 0.45f, z0 - 0.03f);
	J[18] = v3make(x0 + 0.11f, -0.87f, z0);
	J[19] = v3make(x0 + 0.12f, -0.91f, z0 - 0.12f);
}

static void props_from(const v3 J[NJ], fn_nui_props* P)
{
	int j;
	memset(P, 0, sizeof(*P));
	for (j = 0; j < NJ; j++) {
		if (j == 1)
			continue; // SPINE is derived
		P->c[j][0].p = J[j];
		P->c[j][0].conf = 0.9f;
		P->n[j] = 1;
	}
}

int main(void)
{
	static uint16_t map[NW * NH];
	static uint8_t mask[NW * NH];
	static fn_nui_fit fit;
	fn_nui_props P;
	v3 J[NJ], fwd = v3make(0.0f, 0.0f, -1.0f);
	int f;
	char what[128];
	printf("Skeleton fit: joint limits (nui/fit.c)\n");

	// the right knee's proposal 10 cm behind the leg, confidently, every frame
	fn_nui_fit_reset(&fit, 1.0f);
	standing(J);
	J[17].z += 0.10f;
	props_from(J, &P);
	for (f = 0; f < 20; f++)
		fn_nui_fit_user(&fit, &P, map, mask, 0, 1.0f);
	{
		v3 h = fit.joint[16], k = fit.joint[17], a = fit.joint[18], d = v3sub(a, h);
		float t = v3dot(v3sub(k, h), d) / v3dot(d, d), behind = -v3dot(v3sub(k, v3add(h, v3scale(d, t))), fwd);
		snprintf(what, sizeof(what), "knee proposal 10 cm behind the leg: the knee %.1f cm behind", 100 * behind);
		CHECK(behind < 0.035f, what);
	}

	// the left wrist's proposal on the shoulder: the elbow folded shut
	fn_nui_fit_reset(&fit, 1.0f);
	standing(J);
	J[6] = v3add(J[4], v3make(0.0f, -0.02f, -0.02f));
	J[7] = v3add(J[4], v3make(0.0f, -0.05f, -0.03f));
	props_from(J, &P);
	for (f = 0; f < 20; f++)
		fn_nui_fit_user(&fit, &P, map, mask, 0, 1.0f);
	{
		float l1 = fit.bone[5], l2 = fit.bone[6], dmin = sqrtf(l1 * l1 + l2 * l2 - 2 * l1 * l2 * cosf(25 * 0.01745329f));
		float d = fn_nui_v3len(v3sub(fit.joint[6], fit.joint[4]));
		snprintf(what, sizeof(what), "wrist proposal on the shoulder: shoulder-wrist %.1f cm (limit %.1f)", 100 * d, 100 * dmin);
		CHECK(d > 0.85f * dmin, what);
	}

	// a natural pose is left as it is
	fn_nui_fit_reset(&fit, 1.0f);
	standing(J);
	props_from(J, &P);
	for (f = 0; f < 20; f++)
		fn_nui_fit_user(&fit, &P, map, mask, 0, 1.0f);
	{
		float worst = 0;
		int j;
		for (j = 0; j < NJ; j++)
			if (j != 1 && fn_nui_v3len(v3sub(fit.joint[j], J[j])) > worst)
				worst = fn_nui_v3len(v3sub(fit.joint[j], J[j]));
		snprintf(what, sizeof(what), "a natural standing pose: every joint within %.1f cm of its proposal", 100 * worst);
		CHECK(worst < 0.05f, what);
	}
	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
