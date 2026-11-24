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
 * Hotplug bookkeeping: compares the Kinect cameras found by a USB scan with
 * the previous scan. The USB side (scanning, libusb hotplug registration) is
 * in usb_libusb10.c; the public functions are in core.c.
 */

#include <string.h>
#include "freenect_internal.h"

const fn_hotplug_entry *fn_hotplug_find(const fn_hotplug_state *st, uint8_t bus, uint8_t addr)
{
	int i;
	for (i = 0; i < st->n_known; i++)
		if (st->known[i].bus == bus && st->known[i].addr == addr)
			return &st->known[i];
	return NULL;
}

static int contains(const fn_hotplug_entry *list, int n, const fn_hotplug_entry *e)
{
	int i;
	for (i = 0; i < n; i++)
		if (list[i].bus == e->bus && list[i].addr == e->addr)
			return 1;
	return 0;
}

int fn_hotplug_update(fn_hotplug_state *st, const fn_hotplug_entry *now, int n_now,
                      fn_hotplug_change *changes, int max_changes)
{
	int i, n = 0;
	if (n_now > FN_HOTPLUG_MAX)
		n_now = FN_HOTPLUG_MAX;

	for (i = 0; i < st->n_known; i++) {
		if (!contains(now, n_now, &st->known[i]) && n < max_changes) {
			changes[n].event = FREENECT_HOTPLUG_LEFT;
			changes[n].dev = st->known[i];
			n++;
		}
	}
	for (i = 0; i < n_now; i++) {
		if (!contains(st->known, st->n_known, &now[i]) && n < max_changes) {
			changes[n].event = FREENECT_HOTPLUG_ARRIVED;
			changes[n].dev = now[i];
			n++;
		}
	}

	memcpy(st->known, now, sizeof(fn_hotplug_entry) * (size_t)n_now);
	st->n_known = n_now;
	return n;
}

static int find_pending(const fn_hotplug_state *st, const fn_hotplug_entry *e)
{
	int j;
	for (j = 0; j < st->n_pending; j++)
		if (st->pending[j].bus == e->bus && st->pending[j].addr == e->addr)
			return j;
	return -1;
}

int fn_hotplug_hold_unready(fn_hotplug_state *st, fn_hotplug_entry *now, int n_now, uint64_t now_ms)
{
	int i, j, n = 0, np = 0;
	if (n_now > FN_HOTPLUG_MAX)
		n_now = FN_HOTPLUG_MAX;

	// Forget the waiting cameras that are gone.
	for (j = 0; j < st->n_pending; j++) {
		for (i = 0; i < n_now; i++)
			if (now[i].bus == st->pending[j].bus && now[i].addr == st->pending[j].addr)
				break;
		if (i < n_now)
			st->pending[np++] = st->pending[j];
	}
	st->n_pending = np;

	for (i = 0; i < n_now; i++) {
		int hold = 0;
		j = find_pending(st, &now[i]);
		if (now[i].serial[0] == '\0' && !fn_hotplug_find(st, now[i].bus, now[i].addr)) {
			if (j < 0 && st->n_pending < FN_HOTPLUG_MAX) {
				j = st->n_pending++;
				st->pending[j].bus = now[i].bus;
				st->pending[j].addr = now[i].addr;
				st->pending[j].since_ms = now_ms;
			}
			hold = j >= 0 && now_ms - st->pending[j].since_ms < FN_HOTPLUG_SERIAL_WAIT_MS;
		}
		if (hold)
			continue;
		if (j >= 0) // reported now, with or without its serial
			st->pending[j] = st->pending[--st->n_pending];
		now[n++] = now[i];
	}
	return n;
}
