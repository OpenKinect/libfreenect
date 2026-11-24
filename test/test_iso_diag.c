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

/* Host-only test of the isochronous stream health check (src/iso_diag.c). */
#include <stdio.h>
#include <string.h>
#include "freenect_internal.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

/* Feed `seconds` of transfers (one per ms, 16 packets each), `empty` of them
 * without data and `failed` of those with an error status; return the
 * verdicts seen, as a bit mask. */
static int feed(fn_iso_diag *d, uint64_t *t, int seconds, int empty, int failed)
{
	int ms, seen = 0;
	for (ms = 0; ms <= seconds * 1000; ms++, (*t)++) /* windows close at whole seconds */
		seen |= 1 << fn_iso_diag_add(d, *t, 16, empty, failed);
	return seen & ~1; /* drop FN_ISO_OK */
}

int main(void)
{
	fn_iso_diag d;
	uint64_t t = 50000;
	char vm[128];

	printf("1. nothing arrives (a VM's USB passthrough)\n");
	memset(&d, 0, sizeof(d));
	CHECK(feed(&d, &t, 2, 16, 0) == 0, "first window is a warm-up: no verdict");
	CHECK(feed(&d, &t, 2, 16, 0) == (1 << FN_ISO_NO_DATA), "second window: no data");
	CHECK(d.last_pkts > 0 && d.last_empty == d.last_pkts, "window counts are kept for the message");
	CHECK(feed(&d, &t, 6, 16, 0) == 0, "reported only once per stream start");

	printf("2. empty packets alone are normal\n");
	memset(&d, 0, sizeof(d));
	CHECK(feed(&d, &t, 4, 6, 0) == 0, "38 percent empty (640x480 colour, as on a 1414): fine");
	memset(&d, 0, sizeof(d));
	CHECK(feed(&d, &t, 4, 14, 0) == 0, "88 percent empty (high-res colour): fine");

	printf("3. failed packets are loss\n");
	memset(&d, 0, sizeof(d));
	CHECK(feed(&d, &t, 4, 6, 2) == (1 << FN_ISO_HEAVY_LOSS), "12 percent failed: heavy loss");
	CHECK(d.last_failed > 0 && d.last_failed < d.last_empty, "failed counts are kept for the message");
	memset(&d, 0, sizeof(d));
	CHECK(feed(&d, &t, 4, 6, 1) == 0, "6 percent failed: below the threshold");

	printf("4. loss that turns into no data is reported as both\n");
	memset(&d, 0, sizeof(d));
	CHECK(feed(&d, &t, 4, 8, 4) == (1 << FN_ISO_HEAVY_LOSS) && feed(&d, &t, 4, 16, 16) == (1 << FN_ISO_NO_DATA), "heavy loss, then no data");

	printf("5. a restarted stream reports again\n");
	memset(&d, 0, sizeof(d));
	CHECK(feed(&d, &t, 4, 16, 0) == (1 << FN_ISO_NO_DATA), "no data after restart");

	printf("6. VM detection runs: %s\n", fn_detect_vm(vm, sizeof(vm)) ? vm : "(not a VM)");

	printf("\n%s\n", pass ? "ALL PASS" : "SOME FAILED");
	return pass ? 0 : 1;
}
