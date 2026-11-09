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

/* Host-only test of the hotplug bookkeeping (src/hotplug.c). No USB. */
#include <stdio.h>
#include <string.h>
#include "freenect_internal.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

static fn_hotplug_entry E(uint8_t bus, uint8_t addr, const char *serial)
{
	fn_hotplug_entry e;
	e.bus = bus; e.addr = addr;
	snprintf(e.serial, sizeof(e.serial), "%s", serial);
	return e;
}

int main(void)
{
	fn_hotplug_state st;
	fn_hotplug_change ch[2 * FN_HOTPLUG_MAX];
	fn_hotplug_entry now[FN_HOTPLUG_MAX + 4];
	int n, i;
	memset(&st, 0, sizeof(st));

	printf("1. first scan reports what is connected\n");
	now[0] = E(1, 5, "A00366A00000000A"); now[1] = E(2, 9, "B0000000000000B");
	n = fn_hotplug_update(&st, now, 2, ch, 32);
	CHECK(n == 2 && ch[0].event == FREENECT_HOTPLUG_ARRIVED && ch[1].event == FREENECT_HOTPLUG_ARRIVED, "two arrivals");
	CHECK(strcmp(ch[0].dev.serial, "A00366A00000000A") == 0 && ch[1].dev.addr == 9, "serials and addresses passed on");

	printf("2. nothing changed\n");
	n = fn_hotplug_update(&st, now, 2, ch, 32);
	CHECK(n == 0, "no events");
	CHECK(fn_hotplug_find(&st, 2, 9) && strcmp(fn_hotplug_find(&st, 2, 9)->serial, "B0000000000000B") == 0, "find known camera");
	CHECK(fn_hotplug_find(&st, 2, 10) == NULL, "unknown address not found");

	printf("3. one unplugged\n");
	n = fn_hotplug_update(&st, now, 1, ch, 32);
	CHECK(n == 1 && ch[0].event == FREENECT_HOTPLUG_LEFT && ch[0].dev.addr == 9, "one departure");
	CHECK(strcmp(ch[0].dev.serial, "B0000000000000B") == 0, "departure carries the remembered serial");

	printf("4. unplugged and plugged in again between two scans (new address)\n");
	now[0] = E(1, 7, "A00366A00000000A");
	n = fn_hotplug_update(&st, now, 1, ch, 32);
	CHECK(n == 2 && ch[0].event == FREENECT_HOTPLUG_LEFT && ch[0].dev.addr == 5 &&
	      ch[1].event == FREENECT_HOTPLUG_ARRIVED && ch[1].dev.addr == 7, "departure first, then arrival");

	printf("5. everything unplugged\n");
	n = fn_hotplug_update(&st, now, 0, ch, 32);
	CHECK(n == 1 && ch[0].event == FREENECT_HOTPLUG_LEFT && st.n_known == 0, "last departure, nothing known");

	printf("6. more cameras than the table holds\n");
	for (i = 0; i < FN_HOTPLUG_MAX + 4; i++) now[i] = E(3, (uint8_t)(i + 1), "");
	n = fn_hotplug_update(&st, now, FN_HOTPLUG_MAX + 4, ch, 32);
	CHECK(n == FN_HOTPLUG_MAX && st.n_known == FN_HOTPLUG_MAX, "capped at FN_HOTPLUG_MAX");
	n = fn_hotplug_update(&st, now, 0, ch, 3);
	CHECK(n == 3 && st.n_known == 0, "change list capped at max_changes, table still updated");

	printf("7. a new camera whose serial cannot be read yet (Windows, just after replugging)\n");
	memset(&st, 0, sizeof(st));
	now[0] = E(1, 42, "");
	n = fn_hotplug_hold_unready(&st, now, 1, 10000);
	CHECK(n == 0 && st.n_pending == 1, "held back at first");
	n = fn_hotplug_update(&st, now, n, ch, 32);
	CHECK(n == 0, "  no arrival reported");
	now[0] = E(1, 42, "");
	n = fn_hotplug_hold_unready(&st, now, 1, 12000);
	CHECK(n == 0, "  still held 2 s later");
	now[0] = E(1, 42, "C00000000000001C");
	n = fn_hotplug_hold_unready(&st, now, 1, 13000);
	CHECK(n == 1 && st.n_pending == 0, "  serial readable: passed on");
	n = fn_hotplug_update(&st, now, n, ch, 32);
	CHECK(n == 1 && ch[0].event == FREENECT_HOTPLUG_ARRIVED && strcmp(ch[0].dev.serial, "C00000000000001C") == 0,
	      "  one arrival, with its serial");
	n = fn_hotplug_hold_unready(&st, now, 1, 14000);
	CHECK(n == 1 && fn_hotplug_update(&st, now, n, ch, 32) == 0, "  then no further event");

	memset(&st, 0, sizeof(st));
	now[0] = E(1, 43, "");
	fn_hotplug_hold_unready(&st, now, 1, 20000);
	n = fn_hotplug_hold_unready(&st, now, 1, 20000 + FN_HOTPLUG_SERIAL_WAIT_MS);
	CHECK(n == 1 && st.n_pending == 0, "never readable: reported after the wait, without a serial");
	CHECK(fn_hotplug_update(&st, now, n, ch, 32) == 1 && ch[0].dev.serial[0] == '\0', "  one arrival");
	n = fn_hotplug_hold_unready(&st, now, 1, 30000);
	CHECK(n == 1 && st.n_pending == 0, "  known now, so not held again");

	memset(&st, 0, sizeof(st));
	now[0] = E(1, 44, "");
	fn_hotplug_hold_unready(&st, now, 1, 40000);
	n = fn_hotplug_hold_unready(&st, now, 0, 41000);
	CHECK(n == 0 && st.n_pending == 0, "unplugged while waiting: forgotten, nothing reported");

	printf("\n%s\n", pass ? "ALL PASS" : "SOME FAILED");
	return pass ? 0 : 1;
}
