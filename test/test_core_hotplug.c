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

/* Host-only test of the hotplug path in src/core.c and src/hotplug.c against
 * a simulated USB layer: arrivals and departures, the callback, open devices
 * losing their Kinect, the timeout cap when polling, and return values. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freenect_internal.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-66s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

/* ---- simulated USB ------------------------------------------------------ */
static fn_hotplug_entry bus_now[4];
static int bus_n = 0, native = 1, scans = 0, deregs = 0;
static long last_timeout_us = -1;
static uint64_t clock_ms = 100000;
static int stops_depth = 0, stops_video = 0, stops_audio = 0;

static void plug(uint8_t bus, uint8_t addr, const char *serial)
{
	bus_now[bus_n].bus = bus; bus_now[bus_n].addr = addr;
	snprintf(bus_now[bus_n].serial, sizeof(bus_now[bus_n].serial), "%s", serial);
	bus_n++;
}
static void unplug(uint8_t addr)
{
	int i;
	for (i = 0; i < bus_n; i++)
		if (bus_now[i].addr == addr) { memmove(&bus_now[i], &bus_now[i + 1], sizeof(bus_now[0]) * (size_t)(bus_n - i - 1)); bus_n--; return; }
}

int fnusb_init(fnusb_ctx *ctx, freenect_usb_context *u) { (void)ctx; (void)u; return 0; }
void fnusb_check_version(freenect_context *ctx) { (void)ctx; }
int fnusb_shutdown(fnusb_ctx *ctx) { (void)ctx; return 0; }
int fnusb_num_devices(freenect_context *ctx) { (void)ctx; return bus_n; }
int fnusb_process_events_timeout(fnusb_ctx *ctx, struct timeval *tv)
{
	(void)ctx;
	last_timeout_us = tv ? (long)tv->tv_sec * 1000000L + tv->tv_usec : -1;
	return 0;
}
int fnusb_scan_cameras(freenect_context *ctx, fn_hotplug_entry *out, int max)
{
	(void)ctx; (void)max;
	scans++;
	memcpy(out, bus_now, sizeof(bus_now[0]) * (size_t)bus_n);
	return bus_n;
}
void fnusb_hotplug_register(freenect_context *ctx) { ctx->hotplug.native = native; }
void fnusb_hotplug_deregister(freenect_context *ctx) { if (ctx->hotplug.native) deregs++; ctx->hotplug.native = 0; }
int fnusb_list_device_attributes(freenect_context *ctx, struct freenect_device_attributes **list)
{
	int i;
	struct freenect_device_attributes **next = list;
	(void)ctx;
	*list = NULL;
	for (i = 0; i < bus_n; i++) {
		struct freenect_device_attributes *a = calloc(1, sizeof(*a));
		a->camera_serial = strdup(bus_now[i].serial);
		*next = a; next = &a->next;
	}
	return bus_n;
}
int fnusb_open_subdevices(freenect_device *dev, int index)
{
	if (index >= bus_n) return -1;
	dev->cam_bus = bus_now[index].bus;
	dev->cam_addr = bus_now[index].addr;
	dev->usb_cam.dev = (libusb_device_handle *)(void *)&bus_now[index]; /* any non-NULL */
	snprintf(dev->serial, sizeof(dev->serial), "%s", bus_now[index].serial);
	return 0;
}
int fnusb_close_subdevices(freenect_device *dev) { dev->usb_cam.dev = NULL; return 0; }
static int recover_calls = 0, recover_result = 0;
int fnusb_recover_audio(freenect_device *dev)
{
	recover_calls++;
	if (recover_result == 0)
		dev->usb_audio.device_dead = 0; /* reopened */
	return recover_result;
}
int freenect_camera_init(freenect_device *dev) { (void)dev; return 0; }
int freenect_camera_teardown(freenect_device *dev) { (void)dev; return 0; }
int freenect_stop_depth(freenect_device *dev) { (void)dev; stops_depth++; return 0; }
int freenect_stop_video(freenect_device *dev) { (void)dev; stops_video++; return 0; }
int freenect_stop_audio(freenect_device *dev) { (void)dev; stops_audio++; return 0; }
int freenect_start_depth(freenect_device *dev) { (void)dev; return 0; }
int freenect_start_video(freenect_device *dev) { (void)dev; return 0; }
int freenect_set_led(freenect_device *dev, freenect_led_options o) { (void)dev; (void)o; return 0; }
int fn_led_forced_off(void) { return 0; }
void fn_color_init(freenect_device *dev) { (void)dev; }
void fn_color_apply_pending(freenect_device *dev) { (void)dev; }
void fn_timing_process(freenect_device *dev) { (void)dev; }
uint64_t fn_now_ms(void) { return clock_ms; }

/* ---- application -------------------------------------------------------- */
static int arrived = 0, left = 0;
static char last_serial[64];
static freenect_device *app_dev = NULL;
static int close_on_left = 0, open_on_arrival = 0;

static void cb(freenect_context *ctx, freenect_hotplug_event ev, const char *serial, void *user)
{
	(void)user;
	snprintf(last_serial, sizeof(last_serial), "%s", serial);
	if (ev == FREENECT_HOTPLUG_ARRIVED) {
		arrived++;
		if (open_on_arrival && !app_dev && freenect_open_device_by_camera_serial(ctx, &app_dev, serial) < 0)
			app_dev = NULL;
	} else {
		left++;
		if (close_on_left && app_dev && !freenect_device_connected(app_dev)) {
			freenect_close_device(app_dev);
			app_dev = NULL;
		}
	}
}

static int pump(freenect_context *ctx, long timeout_us)
{
	struct timeval tv;
	tv.tv_sec = timeout_us / 1000000L; tv.tv_usec = timeout_us % 1000000L;
	return freenect_process_events_timeout(ctx, &tv);
}

int main(void)
{
	freenect_context *ctx;
	freenect_device *d;
	int r;

	freenect_init(&ctx, NULL);
	freenect_set_log_level(ctx, FREENECT_LOG_FATAL);

	printf("1. native hotplug: Kinects already connected are reported once\n");
	plug(1, 5, "SERIAL-A");
	freenect_set_hotplug_callback(ctx, cb, NULL);
	r = pump(ctx, 60000000L);
	CHECK(r == 0 && arrived == 1 && strcmp(last_serial, "SERIAL-A") == 0, "ARRIVED for the connected Kinect, with its serial");
	CHECK(last_timeout_us == 60000000L, "native: the caller's timeout is not capped");
	pump(ctx, 0); pump(ctx, 0);
	CHECK(arrived == 1 && scans == 1, "no rescans without a libusb event");

	printf("2. a second Kinect arrives (libusb event)\n");
	plug(2, 3, "SERIAL-B");
	ctx->hotplug.dirty = 1; /* what fnusb_hotplug_event() does */
	pump(ctx, 0);
	CHECK(arrived == 2 && strcmp(last_serial, "SERIAL-B") == 0, "ARRIVED for the new one only");

	printf("3. an open Kinect is unplugged\n");
	CHECK(freenect_open_device(ctx, &d, 0) == 0 && freenect_device_connected(d), "open Kinect A, connected");
	unplug(5);
	ctx->hotplug.dirty = 1;
	r = pump(ctx, 0);
	CHECK(left == 1 && strcmp(last_serial, "SERIAL-A") == 0, "LEFT with the remembered serial");
	CHECK(!freenect_device_connected(d), "freenect_device_connected() now 0");
	CHECK(stops_depth == 1 && stops_video == 1 && stops_audio == 0, "its streams were stopped (camera only)");
	CHECK(r == 0, "process_events does not fail while a hotplug callback is set");
	pump(ctx, 0);
	CHECK(stops_depth == 1, "streams are stopped only once");
	freenect_close_device(d);

	printf("4. polling (no libusb hotplug)\n");
	freenect_set_hotplug_callback(ctx, NULL, NULL);
	CHECK(deregs == 1, "clearing the callback deregisters from libusb");
	native = 0; arrived = left = 0; scans = 0;
	freenect_set_hotplug_callback(ctx, cb, NULL);
	pump(ctx, 60000000L);
	CHECK(last_timeout_us == 1000000L, "timeout capped at 1 s");
	CHECK(arrived == 1 && strcmp(last_serial, "SERIAL-B") == 0, "first scan reports Kinect B");
	pump(ctx, 200000L);
	CHECK(last_timeout_us == 200000L, "shorter timeouts are kept");
	plug(1, 8, "SERIAL-A");
	clock_ms += 400; pump(ctx, 0);
	CHECK(arrived == 1, "no scan before the interval has passed");
	clock_ms += 700; pump(ctx, 0);
	CHECK(arrived == 2 && strcmp(last_serial, "SERIAL-A") == 0, "scan after 1 s finds the replugged Kinect");

	printf("5. the callback closes the lost device and reopens it on arrival\n");
	open_on_arrival = close_on_left = 1;
	unplug(8); clock_ms += 1000; pump(ctx, 0);
	plug(1, 9, "SERIAL-A"); clock_ms += 1000; pump(ctx, 0); /* arrival: opens A */
	CHECK(app_dev && freenect_device_connected(app_dev), "callback opened Kinect A");
	unplug(9); clock_ms += 1000; r = pump(ctx, 0);
	CHECK(app_dev == NULL && ctx->first == NULL, "callback closed it after it left");
	plug(1, 10, "SERIAL-A"); clock_ms += 1000; pump(ctx, 0);
	CHECK(app_dev && freenect_device_connected(app_dev), "reopened after it came back");

	printf("6. the audio controller reboots while the camera stays\n");
	CHECK(strcmp(freenect_get_device_serial(app_dev), "SERIAL-A") == 0, "freenect_get_device_serial() of the open device");
	app_dev->usb_audio.dev = (libusb_device_handle *)(void *)&recover_calls; /* any non-NULL */
	stops_depth = stops_video = 0;
	app_dev->usb_audio.device_dead = 1; /* what a failed transfer does */
	recover_result = 0; recover_calls = 0;
	r = pump(ctx, 0);
	CHECK(recover_calls == 1 && freenect_device_connected(app_dev), "reopened: still connected");
	CHECK(stops_depth == 0 && stops_video == 0 && r == 0, "camera streams not touched");
	recover_result = -1;
	for (r = 0; r < 5; r++) {
		app_dev->usb_audio.device_dead = 1;
		pump(ctx, 0);
	}
	CHECK(recover_calls == FN_AUDIO_RECOVER_MAX, "gives up after FN_AUDIO_RECOVER_MAX attempts");
	CHECK(!freenect_device_connected(app_dev) && stops_depth == 1, "then the device counts as disconnected");
	app_dev->usb_audio.dev = NULL;
	app_dev->usb_audio.device_dead = 0;
	app_dev->disconnect_handled = 0;

	printf("7. without a hotplug callback a lost device is an error, as before\n");
	freenect_set_hotplug_callback(ctx, NULL, NULL);
	app_dev->usb_cam.device_dead = 1; /* what a failed transfer does */
	r = pump(ctx, 0);
	CHECK(r < 0, "process_events returns < 0");
	freenect_close_device(app_dev);

	freenect_shutdown(ctx);
	printf("\n%s\n", pass ? "ALL PASS" : "SOME FAILED");
	return pass ? 0 : 1;
}
