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
 * Host-only test of the depth/colour phase alignment (src/stream_timing.h):
 * the nudge command value of Microsoft's driver (§10C.4, §20.1.6) and
 * Kinect10's phase estimate (§20.7.3) on simulated 30 fps frame trains.
 */
#include <stdio.h>
#include <string.h>
#include "stream_timing.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

/* Colour frames every 33333 µs from 0, depth frames `offset` µs after each,
 * fed in time order. Returns the number of colour frames fed before a nudge
 * was due (0: none within 40), and the nudge. */
static int run(int64_t offset, int rate, int32_t *nudge)
{
	fn_phase_state s;
	int i;
	fn_phase_reset(&s);
	for (i = 0; i < 40; i++) {
		int64_t c = (int64_t)i * 33333, d = c + offset;
		if (offset >= 0) {
			if (fn_phase_update(&s, 0, c, rate, nudge)) return i + 1;
			if (fn_phase_update(&s, 1, d, rate, nudge)) return i + 1;
		} else {
			if (fn_phase_update(&s, 1, d, rate, nudge)) return i + 1;
			if (fn_phase_update(&s, 0, c, rate, nudge)) return i + 1;
		}
	}
	return 0;
}

int main(void)
{
	int32_t nudge = 0;
	int n;

	printf("Depth/colour phase alignment (src/stream_timing.h)\n");

	/* v = ((nudge - k3 + 33333) % 33333) * 1000 / k2, clamped to 25..2047 */
	CHECK(fn_timing_nudge_value(1400, 16000, 0) == 87, "nudge 1400 us, k2 16000, k3 0: 87");
	CHECK(fn_timing_nudge_value(1400, 16000, 500) == 56, "  k3 500: 56");
	CHECK(fn_timing_nudge_value(0, 16000, 0) == 25, "  nudge 0: at least 25");
	CHECK(fn_timing_nudge_value(32000, 16000, 0) == 2000, "  nudge 32000: 2000");
	CHECK(fn_timing_nudge_value(33000, 16000, 0) == 2047, "  nudge 33000: at most 2047");
	CHECK(fn_timing_nudge_value(-1000, 16000, 0) == 2020, "  nudge -1000 wraps to 32333: 2020");
	CHECK(fn_timing_nudge_value(1400, 0, 0) == -1, "  no timing parameters: unavailable");

	n = run(30000, 30, &nudge);
	CHECK(n > 0 && nudge == 1400, "depth 30 ms after colour (3.3 ms before the next): nudge 1400");
	n = run(10000, 30, &nudge);
	CHECK(n > 0 && nudge == 32000, "depth 10 ms after colour (23.3 ms before the next): nudge 32000");
	CHECK(n == 5, "  due on the 6th sample (colour frame 5)");
	n = run(0, 30, &nudge);
	CHECK(n == 0, "depth and colour in phase: no nudge");
	n = run(10000, 10, &nudge);
	CHECK(n == 0, "colour at 10 fps (hi-res): no estimate");

	/* when a suggestion is acted on */
	{
		fn_timing_policy p;
		uint64_t t = 1000000;
		int sent = 0, i;
		memset(&p, 0, sizeof(p));
		for (i = 0; i < 50; i++)
			sent += fn_timing_accept(&p, 100, t += 200000);
		CHECK(sent == 0, "phase within 0.1 ms, 50 times: never nudged");
		for (i = 0; i < 50; i++)
			sent += fn_timing_accept(&p, i % 8 == 0 ? 32000 : 120, t += 200000);
		CHECK(sent == 0, "  nor with an odd 32000 among them (the noise seen on Windows)");
		sent = fn_timing_accept(&p, 1400, t += 200000);
		sent += fn_timing_accept(&p, 1400, t += 200000);
		CHECK(sent == 0, "depth late twice: not yet");
		CHECK(fn_timing_accept(&p, 1400, t += 200000) == 1, "  three times in a row: nudged");
		sent = 0;
		for (i = 0; i < 20; i++)
			sent += fn_timing_accept(&p, 1400, t += 200000);
		CHECK(sent == 0, "  then not again within 5 s");
		sent = 0;
		for (i = 0; i < 6; i++)
			sent += fn_timing_accept(&p, 32000, t += 1000000);
		CHECK(sent >= 1, "depth early (32000) consistently, 5 s later: nudged");
		memset(&p, 0, sizeof(p));
		sent = 0;
		for (i = 0; i < 6; i++)
			sent += fn_timing_accept(&p, 33000, t += 1000000);
		CHECK(sent == 0, "depth early by only 0.3 ms (33000): never nudged");

		/* R3 on the 1414: the same 1.3 ms error after every nudge */
		memset(&p, 0, sizeof(p));
		{
			int stops = 0, r;
			sent = 0;
			for (i = 0; i < 150; i++) {
				r = fn_timing_accept(&p, 32000, t += 200000);
				if (r > 0) sent++;
				if (r < 0) stops++;
			}
			CHECK(sent == 2 && stops == 1 && p.stopped, "nudges that change nothing: two, then it stops, once");
		}
		memset(&p, 0, sizeof(p));
		sent = 0;
		for (i = 0; i < 6; i++)
			sent += fn_timing_accept(&p, 32000, t += 1000000) > 0;
		for (i = 0; i < 6; i++)
			sent += fn_timing_accept(&p, 33200, t += 1000000) > 0;
		for (i = 0; i < 6; i++)
			sent += fn_timing_accept(&p, 32000, t += 1000000) > 0;
		CHECK(sent == 2 && !p.stopped, "  a nudge that brought it into phase does not count against it");
	}

	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
