/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <unistd.h>

#include "freenect_internal.h"
#include "registration.h"
#include "cameras.h"
#include "loader.h"

static int env_log_level(freenect_loglevel *level);


FREENECTAPI int freenect_init(freenect_context **ctx, freenect_usb_context *usb_ctx)
{
	int res;

	*ctx = (freenect_context*)malloc(sizeof(freenect_context));
	if (*ctx == NULL)
		return -1;

	memset(*ctx, 0, sizeof(freenect_context));

	(*ctx)->log_level = LL_NOTICE;
	(*ctx)->log_from_env = env_log_level(&(*ctx)->log_level);
	(*ctx)->log_start_ms = fn_now_ms();
	(*ctx)->enabled_subdevices = (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA);
	res = fnusb_init(&(*ctx)->usb, usb_ctx);
	if (res < 0) {
		free(*ctx);
		*ctx = NULL;
		return res;
	}
	fnusb_check_version(*ctx);
	return res;
}

FREENECTAPI int freenect_shutdown(freenect_context *ctx)
{
	fnusb_hotplug_deregister(ctx);
	ctx->hotplug.cb = NULL;

	while (ctx->first) {
		FN_NOTICE("Device %p open during shutdown, closing...\n", ctx->first);
		freenect_close_device(ctx->first);
	}

	fnusb_shutdown(&ctx->usb);
	free(ctx);
	return 0;
}

FREENECTAPI int freenect_set_hotplug_callback(freenect_context *ctx, freenect_hotplug_cb cb, void *user)
{
	if (!cb) {
		fnusb_hotplug_deregister(ctx);
		memset(&ctx->hotplug, 0, sizeof(ctx->hotplug));
		return 0;
	}
	if (!ctx->hotplug.cb) {
		// Start from nothing known, so that the first scan reports every
		// Kinect that is already connected.
		memset(&ctx->hotplug, 0, sizeof(ctx->hotplug));
		ctx->hotplug.dirty = 1;
		fnusb_hotplug_register(ctx);
	}
	ctx->hotplug.cb = cb;
	ctx->hotplug.user = user;
	return 0;
}

FREENECTAPI const char *freenect_get_device_serial(freenect_device *dev)
{
	return dev->serial[0] ? dev->serial : NULL;
}

FREENECTAPI int freenect_device_connected(freenect_device *dev)
{
	return !(dev->usb_cam.device_dead || dev->usb_motor.device_dead || dev->usb_audio.device_dead);
}

// Rescan when libusb reported a Kinect arriving or leaving, or (without
// libusb hotplug) once every FN_HOTPLUG_POLL_MS. Open devices whose camera
// left are marked as disconnected.
static int hotplug_scan(freenect_context *ctx, fn_hotplug_change *changes, int max_changes)
{
	fn_hotplug_entry now[FN_HOTPLUG_MAX];
	uint64_t t = fn_now_ms();
	int n, nch, i;
	freenect_device *dev;

	if (!ctx->hotplug.cb)
		return 0;
	// With libusb hotplug, scan when it reports a change, and once a second
	// while a new camera's serial cannot be read yet.
	if (!ctx->hotplug.dirty && ((ctx->hotplug.native && !ctx->hotplug.n_pending) ||
	                            t - ctx->hotplug.last_scan_ms < FN_HOTPLUG_POLL_MS))
		return 0;
	ctx->hotplug.dirty = 0;
	ctx->hotplug.last_scan_ms = t;

	n = fnusb_scan_cameras(ctx, now, FN_HOTPLUG_MAX);
	if (n < 0) {
		FN_WARNING("Hotplug: cannot list USB devices (%d)\n", n);
		return 0;
	}
	n = fn_hotplug_hold_unready(&ctx->hotplug, now, n, t);
	nch = fn_hotplug_update(&ctx->hotplug, now, n, changes, max_changes);
	for (i = 0; i < nch; i++) {
		if (changes[i].event != FREENECT_HOTPLUG_LEFT)
			continue;
		for (dev = ctx->first; dev; dev = dev->next) {
			if (dev->cam_bus == changes[i].dev.bus && dev->cam_addr == changes[i].dev.addr) {
				dev->usb_cam.device_dead = 1;
				dev->usb_motor.device_dead = 1;
				dev->usb_audio.device_dead = 1;
			}
		}
	}
	return nch;
}

FREENECTAPI int freenect_process_events(freenect_context *ctx)
{
	struct timeval timeout;
	timeout.tv_sec = 60;
	timeout.tv_usec = 0;
	return freenect_process_events_timeout(ctx, &timeout);
}

FREENECTAPI int freenect_process_events_timeout(freenect_context *ctx, struct timeval *timeout)
{
	fn_hotplug_change changes[2 * FN_HOTPLUG_MAX];
	struct timeval capped;
	freenect_device *dev;
	int nch, i, res;

	// Without libusb hotplug, or while a new camera's serial cannot be read
	// yet, come back at least once per scan interval.
	if (ctx->hotplug.cb && (!ctx->hotplug.native || ctx->hotplug.n_pending) && timeout &&
	    (timeout->tv_sec > 1 || (timeout->tv_sec == 1 && timeout->tv_usec > 0))) {
		capped.tv_sec = 1;
		capped.tv_usec = 0;
		timeout = &capped;
	}
	res = fnusb_process_events_timeout(&ctx->usb, timeout);
	nch = hotplug_scan(ctx, changes, 2 * FN_HOTPLUG_MAX);

	// Commands that go to the camera here, outside libusb's event handling:
	// Kinect for Windows colour settings once video frames arrive, and the
	// automatic depth/colour phase alignment.
	for (dev = ctx->first; dev; dev = dev->next) {
		if (dev->color_pending)
			fn_color_apply_pending(dev);
		if (dev->timing_auto)
			fn_timing_process(dev);
	}

	// An audio controller that rebooted into its bootloader while the rest of
	// its Kinect is still there gets its firmware again (a few times at most).
	for (dev = ctx->first; dev; dev = dev->next) {
		if (dev->usb_audio.dev && dev->usb_audio.device_dead && !dev->usb_cam.device_dead &&
		    !dev->usb_motor.device_dead && dev->audio_recover_attempts < FN_AUDIO_RECOVER_MAX) {
			int r;
			dev->audio_recover_attempts++;
			if (dev->audio.running)
				freenect_stop_audio(dev);
			FN_WARNING("The Kinect's audio controller went away; reopening it (and loading its firmware if it restarted)\n");
			r = fnusb_recover_audio(dev);
			if (r == 0)
				FN_NOTICE("The Kinect's audio controller is back\n");
			else {
				dev->usb_audio.device_dead = 1;
				FN_WARNING("Could not reopen the Kinect's audio controller (%d)\n", r);
			}
		}
	}

	// Stop the streams of devices that were unplugged, once. Without a hotplug
	// callback, report the loss as an error (as libfreenect always did); with
	// one, the application learns about it from the callback.
	dev = ctx->first;
	while(dev) {
		if (!freenect_device_connected(dev)) {
			if (!ctx->hotplug.cb)
				res = -1;
			if (!dev->disconnect_handled) {
				dev->disconnect_handled = 1;
				FN_ERROR("Kinect disconnected, stopping its streams\n");
				if (dev->usb_cam.dev) {
					freenect_stop_video(dev);
					freenect_stop_depth(dev);
				}
				if (dev->usb_audio.dev)
					freenect_stop_audio(dev);
			}
		}
		dev = dev->next;
	}

	// Last, as the callback may open and close devices.
	for (i = 0; i < nch; i++) {
		FN_INFO("Hotplug: Kinect %s %s (bus %u, address %u)\n",
		        changes[i].dev.serial[0] ? changes[i].dev.serial : "(serial unknown)",
		        changes[i].event == FREENECT_HOTPLUG_ARRIVED ? "arrived" : "left",
		        changes[i].dev.bus, changes[i].dev.addr);
		if (ctx->hotplug.cb)
			ctx->hotplug.cb(ctx, changes[i].event, changes[i].dev.serial, ctx->hotplug.user);
	}
	return res;
}

FREENECTAPI int freenect_num_devices(freenect_context *ctx)
{
	return fnusb_num_devices(ctx);
}

FREENECTAPI int freenect_list_device_attributes(freenect_context *ctx, struct freenect_device_attributes **attribute_list)
{
	return fnusb_list_device_attributes(ctx, attribute_list);
}

FREENECTAPI void freenect_free_device_attributes(struct freenect_device_attributes *attribute_list)
{
	// Iterate over list, freeing contents of each item as we go.
	struct freenect_device_attributes* to_free;
	while(attribute_list != NULL) {
		to_free = attribute_list;
		if (attribute_list->camera_serial != NULL) {
			free((char*)attribute_list->camera_serial);
			attribute_list->camera_serial = NULL;
		}
		attribute_list = attribute_list->next;
		free(to_free);
	}
	return;
}

FREENECTAPI int freenect_supported_subdevices(void)
{
	return FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA | FREENECT_DEVICE_AUDIO;
}

FREENECTAPI void freenect_select_subdevices(freenect_context *ctx, freenect_device_flags subdevs)
{
	ctx->enabled_subdevices = (freenect_device_flags)(subdevs & (FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA | FREENECT_DEVICE_AUDIO));
}

FREENECTAPI freenect_device_flags freenect_enabled_subdevices(freenect_context *ctx)
{
	return ctx->enabled_subdevices;
}

FREENECTAPI int freenect_open_device(freenect_context *ctx, freenect_device **dev, int index)
{
	int res;
	freenect_device *pdev = (freenect_device*)malloc(sizeof(freenect_device));
	if (!pdev)
		return -1;

	memset(pdev, 0, sizeof(*pdev));

	pdev->parent = ctx;
	fn_color_init(pdev);

	res = fnusb_open_subdevices(pdev, index);
	if (res < 0) {
		free(pdev);
		return res;
	}

	if (!ctx->first) {
		ctx->first = pdev;
	} else {
		freenect_device *prev = ctx->first;
		while (prev->next)
			prev = prev->next;
		prev->next = pdev;
	}

	*dev = pdev;

	// LIBFREENECT_LED=off: switch the LED off as soon as it can be reached. The
	// 1473/K4W audio firmware starts with it blinking.
	if (fn_led_forced_off() && (pdev->usb_motor.dev || pdev->motor_control_with_audio_enabled))
		freenect_set_led(pdev, LED_OFF);

	// Do device-specific initialization
	if (pdev->usb_cam.dev) {
		if (freenect_camera_init(pdev) < 0) {
			return -1;
		}
	}

	return 0;
}

FREENECTAPI int freenect_open_device_by_camera_serial(freenect_context *ctx, freenect_device **dev, const char* camera_serial)
{
	// This is implemented by listing the devices and seeing which index (if
	// any) has a camera with a matching serial number, and then punting to
	// freenect_open_device with that index.
	struct freenect_device_attributes* attrlist;
	struct freenect_device_attributes* item;
	int count = fnusb_list_device_attributes(ctx, &attrlist);
	if (count < 0) {
		FN_ERROR("freenect_open_device_by_camera_serial: Couldn't enumerate serial numbers\n");
		return count;
	}
	int index = 0;
	for(item = attrlist ; item != NULL; item = item->next , index++) {
		if (strlen(item->camera_serial) == strlen(camera_serial) && strcmp(item->camera_serial, camera_serial) == 0) {
			freenect_free_device_attributes(attrlist);
			return freenect_open_device(ctx, dev, index);
		}
	}
	freenect_free_device_attributes(attrlist);
	FN_ERROR("freenect_open_device_by_camera_serial: Couldn't find a device with serial %s\n", camera_serial);
	return -1;
}

FREENECTAPI int freenect_close_device(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;

	FN_INFO("Closing the Kinect\n");
	if (dev->usb_cam.dev) {
		freenect_camera_teardown(dev);
		FN_INFO("Closing: camera streams stopped\n");
	}

	// Put the LED back into the Kinect's idle state (blinking green, as at
	// power-on; off with LIBFREENECT_LED=off), so that it doesn't keep showing
	// what the program last chose once nothing uses the Kinect.
	if (dev->led_changed && freenect_device_connected(dev) &&
	    (dev->usb_motor.dev || dev->motor_control_with_audio_enabled)) {
		int r = freenect_set_led(dev, LED_BLINK_GREEN);
		if (r < 0)
			FN_WARNING("Closing: could not put the LED back (%d)\n", r);
		else
			FN_INFO("Closing: LED back to its idle state\n");
	} else if (dev->led_changed) {
		FN_INFO("Closing: LED left as it is (Kinect disconnected, or no motor/LED access)\n");
	}

	res = fnusb_close_subdevices(dev);
	FN_INFO("Closing: USB handles released\n");
	if (res < 0) {
		FN_ERROR("fnusb_close_subdevices failed: %d\n", res);
		return res;
	}

	freenect_device *last = NULL;
	freenect_device *cur = ctx->first;

	while (cur && cur != dev) {
		last = cur;
		cur = cur->next;
	}

	if (!cur) {
		FN_ERROR("device %p not found in linked list for this context!\n", dev);
		return -1;
	}

	if (last)
		last->next = cur->next;
	else
		ctx->first = cur->next;

	free(dev);
	return 0;
}

FREENECTAPI void freenect_set_user(freenect_device *dev, void *user)
{
	dev->user_data = user;
}

FREENECTAPI void *freenect_get_user(freenect_device *dev)
{
	return dev->user_data;
}

// LIBFREENECT_LOG=fatal|error|warning|notice|info|debug|spew|flood sets the
// log level whatever the program asks for, and adds a time stamp (seconds
// since freenect_init()) to each line, for diagnosing a user's run.
static int env_log_level(freenect_loglevel *level)
{
	static const char *const names[] = { "fatal", "error", "warning", "notice", "info", "debug", "spew", "flood" };
	const char *v = getenv("LIBFREENECT_LOG");
	int i;
	if (!v)
		return 0;
	for (i = 0; i < 8; i++) {
		if (strcmp(v, names[i]) == 0) {
			*level = (freenect_loglevel)i;
			return 1;
		}
	}
	return 0;
}

FREENECTAPI void freenect_set_log_level(freenect_context *ctx, freenect_loglevel level)
{
	if (ctx->log_from_env)
		return; // LIBFREENECT_LOG wins
	ctx->log_level = level;
}

FREENECTAPI void freenect_set_log_callback(freenect_context *ctx, freenect_log_cb cb)
{
	ctx->log_cb = cb;
}

FN_INTERNAL void fn_log(freenect_context *ctx, freenect_loglevel level, const char *fmt, ...)
{
	va_list ap;

	if (level > ctx->log_level)
		return;

	if (ctx->log_cb) {
		char msgbuf[1024];

		va_start(ap, fmt);
		vsnprintf(msgbuf, 1024, fmt, ap);
		msgbuf[1023] = 0;
		va_end(ap);

		ctx->log_cb(ctx, level, msgbuf);
	} else {
		if (ctx->log_from_env) {
			uint64_t t = fn_now_ms() - ctx->log_start_ms;
			fprintf(stderr, "[%4u.%03u] ", (unsigned)(t / 1000), (unsigned)(t % 1000));
		}
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}


FREENECTAPI void freenect_set_fw_address_nui(freenect_context * ctx, unsigned char * fw_ptr, unsigned int num_bytes)
{
    ctx->fn_fw_nui_ptr = fw_ptr;
    ctx->fn_fw_nui_size = num_bytes;
}

FREENECTAPI void freenect_set_fw_address_k4w(freenect_context * ctx, unsigned char * fw_ptr, unsigned int num_bytes)
{
    ctx->fn_fw_k4w_ptr = fw_ptr;
    ctx->fn_fw_k4w_size = num_bytes;
}

