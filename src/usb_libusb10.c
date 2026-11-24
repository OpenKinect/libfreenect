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
#include <errno.h>
#include <unistd.h>
#ifndef _WIN32
#include <time.h> // clock_gettime
#endif
#include <libusb.h>
#include "freenect_internal.h"
#include "loader.h"

#ifdef _MSC_VER
	# define sleep(x) Sleep((x)*1000) 
#endif 


FN_INTERNAL int fnusb_num_devices(freenect_context *ctx)
{
	libusb_device **devs; // pointer to pointer of device, used to retrieve a list of devices
	ssize_t count = libusb_get_device_list (ctx->usb.ctx, &devs);
	if (count < 0)
		return (count >= INT_MIN) ? (int)count : -1;

	int number_found = 0, i = 0;
	struct libusb_device_descriptor desc;
	for (i = 0; i < count; ++i)
	{
		int r = libusb_get_device_descriptor (devs[i], &desc);
		if (r < 0)
		{
			FN_WARNING("Failed to query USB device descriptor.\n");
			continue;
		}

		if (desc.idVendor == VID_MICROSOFT)
		{
			if (desc.idProduct == PID_NUI_CAMERA || desc.idProduct == PID_K4W_CAMERA)
			{
				number_found++;
			}
			else if (desc.idProduct == PID_KV2_CAMERA)
			{
				FN_NOTICE("Skipping Kinect v2 device (needs https://github.com/OpenKinect/libfreenect2).\n");
			}
		}
	}

	libusb_free_device_list (devs, 1);
	return number_found;
}

FN_INTERNAL short fnusb_is_camera(struct libusb_device_descriptor desc)
{
	return desc.idVendor == VID_MICROSOFT
	    && (desc.idProduct == PID_NUI_CAMERA || desc.idProduct == PID_K4W_CAMERA);
}

FN_INTERNAL short fnusb_is_motor(struct libusb_device_descriptor desc)
{
	return desc.idVendor == VID_MICROSOFT
	    && desc.idProduct == PID_NUI_MOTOR;
}

FN_INTERNAL short fnusb_is_audio(struct libusb_device_descriptor desc)
{
	return desc.idVendor == VID_MICROSOFT
	    && (desc.idProduct == PID_NUI_AUDIO
	     || desc.idProduct == PID_K4W_AUDIO ||  desc.idProduct == PID_K4W_AUDIO_ALT_1 || desc.idProduct == PID_K4W_AUDIO_ALT_2);
}

FN_INTERNAL libusb_device * fnusb_find_sibling_device(freenect_context* ctx, libusb_device* camera,
                                                      libusb_device** deviceList, int count,
                                                      short (*predicate)(struct libusb_device_descriptor))
{
	if (count <= 0) return NULL;

	const int cameraBusNo = libusb_get_bus_number(camera);
	if (cameraBusNo < 0) return NULL;
	const libusb_device * cameraParent = libusb_get_parent(camera);

	int siblingCount_Total = 0;
	int siblingCount_SameBus = 0;
	libusb_device* siblingMatch_Single = NULL;
	libusb_device* siblingMatch_SameBus = NULL;

	int i = 0;
	for (i = 0; i < count; i++)
	{
		libusb_device* currentDevice = deviceList[i];

		struct libusb_device_descriptor desc;
		int res = libusb_get_device_descriptor(currentDevice, &desc);
		if (res < 0) {
			continue;
		}

		if (!(*predicate)(desc)) {
			continue;
		}

		siblingCount_Total++;
		siblingMatch_Single = (siblingCount_Total == 1) ? currentDevice : NULL;

		if (cameraBusNo != libusb_get_bus_number(currentDevice)){
			continue;
		}

		siblingCount_SameBus++;
		siblingMatch_SameBus = (siblingCount_SameBus == 1) ? currentDevice : NULL;

		if (cameraParent == NULL || cameraParent != libusb_get_parent(currentDevice)) {
			continue;
		}

		FN_DEBUG("Found sibling device [same parent]\n");
		return currentDevice;
	}

	if (siblingMatch_SameBus != NULL) {
		FN_DEBUG("Found sibling device [single on same bus]\n");
		return siblingMatch_SameBus;
	}
	if (siblingMatch_Single != NULL) {
		FN_DEBUG("Found sibling device [single on system]\n");
		return siblingMatch_Single;
	}

	return NULL;
}

static int fnusb_is_kinect_function(const struct libusb_device_descriptor *desc)
{
	if (desc->idVendor != VID_MICROSOFT)
		return 0;
	switch (desc->idProduct) {
	case PID_NUI_CAMERA: case PID_K4W_CAMERA: case PID_NUI_MOTOR: case PID_NUI_MOTOR_ALT:
	case PID_NUI_AUDIO: case PID_K4W_AUDIO: case PID_K4W_AUDIO_ALT_1: case PID_K4W_AUDIO_ALT_2:
		return 1;
	default:
		return 0;
	}
}

// Decide the Kinect model from its internal hub (see fnusb_kinect_type()): the
// hub's IDs and the Kinect function on the hub's lowest port.
static fn_kinect_type fnusb_detect_kinect_type(freenect_context *ctx, libusb_device *camera, libusb_device **devs,
                                               int count, const struct libusb_device_descriptor *camera_desc)
{
	libusb_device *hub = libusb_get_parent(camera);
	struct libusb_device_descriptor hub_desc;
	int have_hub = hub != NULL && libusb_get_device_descriptor(hub, &hub_desc) == 0;
	uint16_t first_pid = 0;
	int first_port = 256, i;
	fn_kinect_type type;

	if (!have_hub)
		memset(&hub_desc, 0, sizeof(hub_desc));
	for (i = 0; have_hub && i < count; i++) {
		struct libusb_device_descriptor desc;
		if (libusb_get_parent(devs[i]) != hub || libusb_get_device_descriptor(devs[i], &desc) != 0)
			continue;
		if (fnusb_is_kinect_function(&desc) && libusb_get_port_number(devs[i]) < first_port) {
			first_port = libusb_get_port_number(devs[i]);
			first_pid = desc.idProduct;
		}
	}
	type = fnusb_kinect_type(have_hub, hub_desc.idVendor, hub_desc.idProduct, first_pid,
	                         camera_desc->idProduct, camera_desc->bcdDevice);
	if (have_hub && ((hub_desc.idVendor == VID_NEC && hub_desc.idProduct == PID_NEC_HUB) ||
	                 (hub_desc.idVendor == VID_MICROSOFT && hub_desc.idProduct == PID_KINECT_HUB)))
		FN_NOTICE("Kinect model: %s, from hub %04x:%04x\n", fnusb_kinect_type_name(type), hub_desc.idVendor, hub_desc.idProduct);
	else
		FN_NOTICE("Kinect model: %s, guessed from the camera (bcdDevice %04x): the Kinect's hub is not visible\n",
		          fnusb_kinect_type_name(type), camera_desc->bcdDevice);
	return type;
}

// Read the serial that identifies the Kinect whose camera is `camera`:
// the camera's serial, or for the model 1473 and the Kinect for Windows
// (whose cameras report 0000000000000000) the audio controller's serial.
// cam_handle is an open handle to the camera, or NULL to open it here.
// Returns 0, or < 0 if the camera has no serial or cannot be opened.
static int fnusb_read_kinect_serial(freenect_context *ctx, libusb_device *camera, libusb_device_handle *cam_handle,
                                    libusb_device **devs, ssize_t count, char *serial, int size)
{
	struct libusb_device_descriptor desc;
	unsigned char buf[256]; // String descriptors are at most 256 bytes.
	libusb_device_handle *handle = cam_handle;
	int res = libusb_get_device_descriptor(camera, &desc);
	if (res < 0)
		return res;
	// Verify that a serial number exists to query.  If not, don't touch the device.
	if (desc.iSerialNumber == 0)
		return -1;
	if (!handle) {
		res = libusb_open(camera, &handle);
		if (res != 0)
			return res;
	}
	res = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, buf, sizeof(buf));
	if (!cam_handle)
		libusb_close(handle);
	if (res < 0)
		return res;

	// K4W and 1473 don't provide a camera serial; use audio serial instead.
	const char* const K4W_1473_SERIAL = "0000000000000000";
	if (strncmp((const char*)buf, K4W_1473_SERIAL, 16) == 0)
	{
		libusb_device* audio_device = fnusb_find_sibling_device(ctx, camera, devs, (int)count, &fnusb_is_audio);
		if (audio_device != NULL)
		{
			struct libusb_device_descriptor audio_desc;
			res = libusb_get_device_descriptor(audio_device, &audio_desc);
			if (res != 0)
			{
				FN_WARNING("Failed to get audio serial descriptors of K4W or 1473 device: %s\n", libusb_error_name(res));
			}
			else
			{
				libusb_device_handle * audio_handle = NULL;
				res = libusb_open(audio_device, &audio_handle);
				if (res != 0)
				{
					FN_WARNING("Failed to open audio device for serial of K4W or 1473 device: %s\n", libusb_error_name(res));
				}
				else
				{
					unsigned char audio_serial[256];
					res = libusb_get_string_descriptor_ascii(audio_handle, audio_desc.iSerialNumber, audio_serial, sizeof(audio_serial));
					libusb_close(audio_handle);
					if (res <= 0)
						FN_WARNING("Failed to get audio serial of K4W or 1473 device: %s\n", libusb_error_name(res));
					else
						memcpy(buf, audio_serial, sizeof(buf));
				}
			}
		}
	}

	snprintf(serial, (size_t)size, "%s", (const char *)buf);
	return 0;
}

FN_INTERNAL int fnusb_list_device_attributes(freenect_context *ctx, struct freenect_device_attributes** attribute_list)
{
	*attribute_list = NULL; // initialize some return value in case the user is careless.
	libusb_device **devs;   // pointer to pointer of device, used to retrieve a list of devices
	ssize_t count = libusb_get_device_list (ctx->usb.ctx, &devs);
	if (count < 0)
	{
		return (count >= INT_MIN) ? (int)count : -1;
	}

	struct freenect_device_attributes** next_attr = attribute_list;

	// Pass over the list.  For each camera seen, if we already have a camera
	// for the newest_camera device, allocate a new one and append it to the list,
	// incrementing num_cams.
	int num_cams = 0;
	int i;
	for (i = 0; i < count; i++)
	{
		libusb_device* camera_device = devs[i];
		char serial[256];

		struct libusb_device_descriptor desc;
		int res = libusb_get_device_descriptor (camera_device, &desc);
		if (res < 0)
		{
			continue;
		}

		if (desc.idVendor == VID_MICROSOFT && (desc.idProduct == PID_NUI_CAMERA || desc.idProduct == PID_K4W_CAMERA))
		{
			if (fnusb_read_kinect_serial(ctx, camera_device, NULL, devs, count, serial, sizeof(serial)) < 0)
				continue;

			// Add item to linked list.
			struct freenect_device_attributes* current_attr = (struct freenect_device_attributes*)malloc(sizeof(struct freenect_device_attributes));
			memset(current_attr, 0, sizeof(*current_attr));

			current_attr->camera_serial = strdup(serial);
			*next_attr = current_attr;
			next_attr = &(current_attr->next);
			num_cams++;
		}
	}

	libusb_free_device_list(devs, 1);
	return num_cams;
}

// Find the Kinect cameras now connected (at most max) for the hotplug scan.
// Serials are read only for cameras the previous scan did not know; for a
// camera this context has open, through its own handle (on Windows a second
// open of a WinUSB device fails).
FN_INTERNAL int fnusb_scan_cameras(freenect_context *ctx, fn_hotplug_entry *out, int max)
{
	libusb_device **devs;
	ssize_t count = libusb_get_device_list(ctx->usb.ctx, &devs);
	int i, n = 0;
	if (count < 0)
		return (count >= INT_MIN) ? (int)count : -1;
	for (i = 0; i < count && n < max; i++) {
		struct libusb_device_descriptor desc;
		const fn_hotplug_entry *known;
		fn_hotplug_entry *e = &out[n];
		if (libusb_get_device_descriptor(devs[i], &desc) < 0 || !fnusb_is_camera(desc))
			continue;
		e->bus = libusb_get_bus_number(devs[i]);
		e->addr = libusb_get_device_address(devs[i]);
		e->serial[0] = '\0';
		known = fn_hotplug_find(&ctx->hotplug, e->bus, e->addr);
		if (known) {
			memcpy(e->serial, known->serial, sizeof(e->serial));
		} else {
			libusb_device_handle *own = NULL;
			freenect_device *dev;
			for (dev = ctx->first; dev; dev = dev->next)
				if (dev->usb_cam.dev && dev->cam_bus == e->bus && dev->cam_addr == e->addr)
					own = dev->usb_cam.dev;
			if (fnusb_read_kinect_serial(ctx, devs[i], own, devs, count, e->serial, sizeof(e->serial)) < 0)
				e->serial[0] = '\0';
		}
		n++;
	}
	libusb_free_device_list(devs, 1);
	return n;
}

static int LIBUSB_CALL fnusb_hotplug_event(libusb_context *usb_ctx, libusb_device *device, libusb_hotplug_event event, void *user)
{
	freenect_context *ctx = (freenect_context *)user;
	struct libusb_device_descriptor desc;
	(void)usb_ctx; (void)event;
	// Only rescan for Kinect functions. Runs inside libusb's event handling,
	// where no synchronous I/O is allowed, so the scan happens afterwards.
	if (libusb_get_device_descriptor(device, &desc) == 0 && desc.idVendor == VID_MICROSOFT)
		ctx->hotplug.dirty = 1;
	return 0; // stay registered
}

// Use libusb's hotplug notifications if this platform has them; otherwise
// freenect_process_events_timeout() polls.
FN_INTERNAL void fnusb_hotplug_register(freenect_context *ctx)
{
	libusb_hotplug_callback_handle handle;
	const char *poll = getenv("LIBFREENECT_HOTPLUG_POLL");
	ctx->hotplug.native = 0;
	if (poll && strcmp(poll, "1") == 0) {
		FN_INFO("LIBFREENECT_HOTPLUG_POLL=1: scanning for Kinects once a second\n");
		return;
	}
	if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		FN_INFO("libusb has no hotplug support here; scanning for Kinects once a second\n");
		return;
	}
	if (libusb_hotplug_register_callback(ctx->usb.ctx,
	        (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
	        (libusb_hotplug_flag)0, VID_MICROSOFT, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
	        fnusb_hotplug_event, ctx, &handle) == LIBUSB_SUCCESS) {
		ctx->hotplug.native = 1;
		ctx->hotplug.native_handle = (int)handle;
	} else {
		FN_WARNING("libusb hotplug registration failed; scanning for Kinects once a second\n");
	}
}

FN_INTERNAL void fnusb_hotplug_deregister(freenect_context *ctx)
{
	if (ctx->hotplug.native)
		libusb_hotplug_deregister_callback(ctx->usb.ctx, (libusb_hotplug_callback_handle)ctx->hotplug.native_handle);
	ctx->hotplug.native = 0;
}

#ifdef _WIN32
// LIBFREENECT_USBDK=1 selects libusb's UsbDk backend instead of WinUSB/libusbK.
static int fnusb_usbdk_requested(void)
{
	char value[8];
	DWORD n = GetEnvironmentVariableA("LIBFREENECT_USBDK", value, sizeof(value));
	return n > 0 && n < sizeof(value) && value[0] == '1';
}
#endif

FN_INTERNAL int fnusb_init(fnusb_ctx *ctx, freenect_usb_context *usb_ctx)
{
	int res;
	ctx->using_usbdk = 0;
	if (!usb_ctx) {
		res = libusb_init(&ctx->ctx);
		if (res >= 0) {
			ctx->should_free_ctx = 1;
#if defined(_WIN32) && defined(LIBUSB_API_VERSION) && (LIBUSB_API_VERSION >= 0x01000106)
			// Must happen before any device is opened (libusb >= 1.0.22).
			if (fnusb_usbdk_requested() && libusb_set_option(ctx->ctx, LIBUSB_OPTION_USE_USBDK) == LIBUSB_SUCCESS)
				ctx->using_usbdk = 1;
#endif
			return 0;
		} else {
			ctx->should_free_ctx = 0;
			ctx->ctx = NULL;
			return res;
		}
	} else {
    // explicit cast required: in WIN32, freenect_usb_context* maps to void*
    ctx->ctx = (libusb_context*)usb_ctx;
		ctx->should_free_ctx = 0;
		return 0;
	}
}

FN_INTERNAL void fnusb_check_version(freenect_context *ctx)
{
	const struct libusb_version *v = libusb_get_version();
	FN_INFO("Using libusb %u.%u.%u%s\n", v->major, v->minor, v->micro, ctx->usb.using_usbdk ? " (UsbDk backend)" : "");
#ifdef _WIN32
	// WinUSB isochronous support appeared in libusb 1.0.23; 1.0.24 and 1.0.25
	// shipped a Windows isochronous regression that 1.0.26 fixed.
	if (v->major == 1 && v->minor == 0 && v->micro < 26)
		FN_WARNING("libusb %u.%u.%u is too old for Kinect streaming on Windows; use libusb 1.0.26 or newer\n",
		           v->major, v->minor, v->micro);
#endif
}

FN_INTERNAL int fnusb_shutdown(fnusb_ctx *ctx)
{
	//int res;
	if (ctx->should_free_ctx) {
		libusb_exit(ctx->ctx);
		ctx->ctx = NULL;
	}
	return 0;
}

FN_INTERNAL int fnusb_process_events(fnusb_ctx *ctx)
{
	return libusb_handle_events(ctx->ctx);
}

FN_INTERNAL int fnusb_process_events_timeout(fnusb_ctx *ctx, struct timeval* timeout)
{
	return libusb_handle_events_timeout(ctx->ctx, timeout);
}

FN_INTERNAL int fnusb_claim_camera(freenect_device* dev)
{
	freenect_context *ctx = dev->parent;

	int ret = 0;

	// On Linux the gspca_kinect kernel driver binds the camera (it serves it
	// as a V4L2 device). Let libusb detach it when we claim the interface and
	// re-attach it when we release it, so the V4L2 device comes back after
	// the application closes the Kinect. Other platforms have no kernel
	// driver to detach and return LIBUSB_ERROR_NOT_SUPPORTED, which is fine.
	ret = libusb_set_auto_detach_kernel_driver(dev->usb_cam.dev, 1);
	if (ret < 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
		FN_WARNING("Cannot enable kernel driver auto-detach for the camera: %s\n", libusb_error_name(ret));

	ret = libusb_claim_interface(dev->usb_cam.dev, 0);
	if (ret < 0)
	{
		FN_ERROR("Failed to claim camera interface: %s\n", libusb_error_name(ret));
		libusb_close(dev->usb_cam.dev);
		dev->usb_cam.dev = NULL;
		return ret;
	}

	// The K4W and 1473 cameras have two alternate settings, and alt 0 has no
	// isochronous bandwidth: select alt 1, as the Microsoft driver does when
	// the interface has two (§10A.2). Decided from the descriptor, not the PID.
	int num_alt = 1;
	struct libusb_config_descriptor *config = NULL;
	if (libusb_get_active_config_descriptor(libusb_get_device(dev->usb_cam.dev), &config) == 0 && config) {
		if (config->bNumInterfaces > 0)
			num_alt = config->interface[0].num_altsetting;
		libusb_free_config_descriptor(config);
	} else if (dev->usb_cam.PID == PID_K4W_CAMERA) {
		num_alt = 2; // descriptor unreadable: fall back to the PID
	}
	if (num_alt >= 2)
	{
		ret = libusb_set_interface_alt_setting(dev->usb_cam.dev, 0, 1);
		if (ret != 0)
		{
			FN_ERROR("Failed to set alternate setting 1 of the camera interface: %s\n", libusb_error_name(ret));
			libusb_close(dev->usb_cam.dev);
			dev->usb_cam.dev = NULL;
			return ret;
		}
	}

	return ret;
}

FN_INTERNAL int fnusb_keep_alive_led(freenect_context* ctx, libusb_device* audio)
{
	if (audio == NULL) return -1;

	int res = 0;
	libusb_device_handle * audioHandle = NULL;

	res = libusb_open(audio, &audioHandle);
	if (res < 0) {
		FN_ERROR("Failed to set the LED of K4W or 1473 device: %s\n", libusb_error_name(res));
		return res;
	}

	// The LED command only exists once the audio firmware runs; in its
	// bootloader the controller has a single interface. (This used to reset
	// the device first, to recover from a session that did not close it
	// cleanly. A reset re-enumerates the whole audio controller and caused
	// LIBUSB_ERROR_IO on some systems, #580 and #557; stalled pipes are now
	// cleared where the commands fail, and every command has a timeout.)
	struct libusb_config_descriptor *config = NULL;
	int interfaces = 0;
	if (libusb_get_active_config_descriptor(audio, &config) == 0) {
		interfaces = config->bNumInterfaces;
		libusb_free_config_descriptor(config);
	}
	if (interfaces < 2) {
		FN_INFO("Audio controller has no firmware yet; not setting the LED\n");
		libusb_close(audioHandle);
		return 0;
	}

	res = libusb_claim_interface(audioHandle, 0);
	if (res < 0)
	{
		FN_ERROR("Unable to claim interface %s\n", libusb_error_name(res));
	}
	else
	{
		int build = fnusb_audio_fw_build(audioHandle, ctx);
		if (build > 0 && build < FN_UAC_LED_MIN_BUILD)
			FN_INFO("Audio firmware build %d has no LED command; not setting the LED\n", build);
		else
			fnusb_set_led_alt(audioHandle, ctx, fn_led_forced_off() ? LED_OFF : LED_GREEN);
		libusb_release_interface(audioHandle, 0);
	}

	libusb_close(audioHandle);
	return res;
}

// Open the audio controller `audio` of dev, upload its firmware if it is
// still in its bootloader, and wait for it to come back running it. Used when
// the device is opened, and again if the controller reboots into its
// bootloader while the device is open (fnusb_recover_audio()).
static int fnusb_open_audio(freenect_device *dev, libusb_device *audio)
{
	freenect_context *ctx = dev->parent;
	int res;

	struct libusb_device_descriptor desc;
	res = libusb_get_device_descriptor(audio, &desc);
	if (res < 0) {
		FN_ERROR("Could not query device: %s\n", libusb_error_name(res));
		return res < 0 ? res : -1;
	}

	res = libusb_open(audio, &dev->usb_audio.dev);
	if (res < 0 || !dev->usb_audio.dev)
	{
		FN_ERROR("Could not open device: %s\n", libusb_error_name(res));
		dev->usb_audio.dev = NULL;
		return res < 0 ? res : -1;
	}

	res = libusb_claim_interface(dev->usb_audio.dev, 0);
	if (res < 0) {
		FN_ERROR("Could not claim interface: %s\n", libusb_error_name(res));
		libusb_close(dev->usb_audio.dev);
		dev->usb_audio.dev = NULL;
		return res < 0 ? res : -1;
	}

	dev->usb_audio.VID = desc.idVendor;
	dev->usb_audio.PID = desc.idProduct;

	// Using the device handle that we've claimed, see if this
	// device has already uploaded firmware (has 2 interfaces).
	// If not, save the serial number (by reading the appropriate
	// descriptor), upload the firmware, and then enter a loop
	// waiting for a device with the same serial number to
	// reappear.
	int num_interfaces = fnusb_num_interfaces(&dev->usb_audio);
	if (num_interfaces >= 2)
	{
		if (dev->device_does_motor_control_with_audio)
		{
			dev->motor_control_with_audio_enabled = 1;
		}
	}
	else
	{
		// Read the serial number from the string descriptor and save it.
		unsigned char string_desc[256]; // String descriptors are at most 256 bytes
		res = libusb_get_string_descriptor_ascii(dev->usb_audio.dev, desc.iSerialNumber, string_desc, 256);
		if (res < 0) {
			FN_ERROR("Failed to retrieve serial number for audio device in bootloader state\n");
			return res < 0 ? res : -1;
		}

		char* audio_serial = strdup((char*)string_desc);

		FN_SPEW("Uploading firmware to audio device in bootloader state.\n");

		if (desc.idProduct == PID_NUI_AUDIO && ctx->fn_fw_nui_ptr && ctx->fn_fw_nui_size > 0)
		{
			FN_SPEW("loading firmware from memory\n");
			res = upload_firmware_from_memory(&dev->usb_audio, ctx->fn_fw_nui_ptr, ctx->fn_fw_nui_size);
		}
		else if (desc.idProduct == PID_K4W_AUDIO && ctx->fn_fw_k4w_ptr && ctx->fn_fw_k4w_size > 0)
		{
			FN_SPEW("loading firmware from memory\n");
			res = upload_firmware_from_memory(&dev->usb_audio, ctx->fn_fw_k4w_ptr, ctx->fn_fw_k4w_size);
		}
		else
		{
			res = upload_default_firmware(&dev->usb_audio);
		}

		if (res < 0) {
			FN_ERROR("upload_firmware failed: %d\n", res);
			free(audio_serial);
			return res < 0 ? res : -1;
		}

		// Remember the physical port: the controller re-enumerates on the same
		// port, possibly with another PID (02BB/02C3 with Microsoft's firmware).
		libusb_device *old_audio = libusb_get_device(dev->usb_audio.dev);
		uint8_t old_bus = libusb_get_bus_number(old_audio);
		uint8_t old_ports[8];
		int old_nports = libusb_get_port_numbers(old_audio, old_ports, sizeof(old_ports));

		libusb_close(dev->usb_audio.dev);
		dev->usb_audio.dev = NULL;

		// Wait for the device to reappear.
		int loops = 0;
		for (loops = 0; loops < 10; loops++)
		{
			FN_SPEW("Try %d: Looking for new audio device matching serial %s\n", loops, audio_serial);
			libusb_device **new_dev_list;
			int dev_index;
			ssize_t num_new_devs = libusb_get_device_list(ctx->usb.ctx, &new_dev_list);

			for (dev_index = 0; dev_index < num_new_devs; ++dev_index)
			{
				struct libusb_device_descriptor new_dev_desc;
				int r;
				r = libusb_get_device_descriptor (new_dev_list[dev_index], &new_dev_desc);
				if (r < 0)
					continue;
				// If this dev is a Kinect audio device, open device, read serial, and compare.
				if (fnusb_is_audio(new_dev_desc))
				{
					FN_SPEW("Matched VID/PID!\n");
					libusb_device_handle* new_dev_handle;
					// Open device
					r = libusb_open(new_dev_list[dev_index], &new_dev_handle);
					if (r < 0)
						continue;
					// Read serial
					r = libusb_get_string_descriptor_ascii(new_dev_handle, new_dev_desc.iSerialNumber, string_desc, 256);
					if (r < 0)
					{
						FN_SPEW("Lost new audio device while fetching serial number.\n");
						libusb_close(new_dev_handle);
						continue;
					}
					// Same device: same serial, or (if the firmware reports another
					// serial) the same bus and port path.
					int same_serial = (r == (int)strlen(audio_serial) && strcmp((char*)string_desc, audio_serial) == 0);
					uint8_t new_ports[8];
					int new_nports = libusb_get_port_numbers(new_dev_list[dev_index], new_ports, sizeof(new_ports));
					int same_port = old_nports > 0 && new_nports == old_nports &&
					                libusb_get_bus_number(new_dev_list[dev_index]) == old_bus &&
					                memcmp(new_ports, old_ports, (size_t)old_nports) == 0;
					if (!same_serial && same_port)
						FN_NOTICE("Audio controller came back on the same port with serial %s (was %s)\n", (char*)string_desc, audio_serial);
					if (same_serial || same_port)
					{
						// We found it!
						r = libusb_claim_interface(new_dev_handle, 0);
						if (r != 0)
						{
							// Ouch, found the device but couldn't claim the interface.
							FN_SPEW("Device with serial %s reappeared but couldn't claim interface 0\n", audio_serial);
							libusb_close(new_dev_handle);
							continue;
						}
						// Save the device handle.
						dev->usb_audio.dev = new_dev_handle;
						dev->usb_audio.PID = new_dev_desc.idProduct;
						// The old handle's last transfers may have failed with
						// NO_DEVICE as the controller rebooted; the new one is fine.
						dev->usb_audio.device_dead = 0;

						// Verify that we've actually found a device running the right firmware.
						num_interfaces = fnusb_num_interfaces(&dev->usb_audio);
						FN_NOTICE("Audio controller re-enumerated as %04x:%04x with %d interfaces%s\n",
						          new_dev_desc.idVendor, new_dev_desc.idProduct, num_interfaces,
						          fnusb_has_audio_class(&dev->usb_audio) ? " (standard USB Audio: Microsoft's UAC firmware)" : "");

						if (num_interfaces >= 2)
						{
							if (dev->device_does_motor_control_with_audio)
							{
								dev->motor_control_with_audio_enabled = 1;
							}
						}
						else
						{
							FN_SPEW("Opened audio with matching serial but too few interfaces.\n");
							dev->usb_audio.dev = NULL;
							libusb_close(new_dev_handle);
							continue;
						}

						break;
					}
					else
					{
						FN_SPEW("Got serial %s, expected serial %s\n", (char*)string_desc, audio_serial);
					}
				}
			}

			libusb_free_device_list(new_dev_list, 1);
			// If we found the right device, break out of this loop.
			if (dev->usb_audio.dev)
				break;
			// Sleep for a second to give the device more time to reenumerate.
			sleep(1);
		}

		free(audio_serial);
	}

	if (!dev->usb_audio.dev)
		return -1;
	dev->usb_audio.device_dead = 0;
	// Which UAC firmware runs: build 709 (v1.8) has the LED command, build 390
	// (SDK beta 2, which some loaders install) does not.
	dev->audio_fw_build = 0;
	if (fnusb_has_audio_class(&dev->usb_audio)) {
		dev->audio_fw_build = fnusb_audio_fw_build(dev->usb_audio.dev, ctx);
		if (dev->audio_fw_build > 0)
			FN_NOTICE("Audio firmware build %d%s\n", dev->audio_fw_build,
			          dev->audio_fw_build < FN_UAC_LED_MIN_BUILD ? " (SDK beta 2: no LED control; Microsoft's UAC.BIN v1.8 has it)" : "");
	}
	return 0;
}

FN_INTERNAL int fnusb_open_subdevices(freenect_device *dev, int index)
{
	freenect_context *ctx = dev->parent;

	dev->device_does_motor_control_with_audio = 0;
	dev->motor_control_with_audio_enabled = 0;
	dev->device_type = FN_KINECT_UNKNOWN;

	dev->usb_cam.parent = dev;
	dev->usb_cam.dev = NULL;
	dev->usb_motor.parent = dev;
	dev->usb_motor.dev = NULL;
	dev->usb_audio.parent = dev;
	dev->usb_audio.dev = NULL;
	dev->usb_cam.device_dead = 0;
	dev->usb_motor.device_dead = 0;
	dev->usb_audio.device_dead = 0;
	dev->disconnect_handled = 0;

	libusb_device **devs; // pointer to pointer of device, used to retrieve a list of devices
	ssize_t count = libusb_get_device_list (dev->parent->usb.ctx, &devs); //get the list of devices
	if (count < 0)
		return -1;

	libusb_device* camera = NULL;

	int res = 0;
	int i = 0, nr_cam = 0;
	for (i = 0; i < count; i++)
	{
		struct libusb_device_descriptor desc;
		res = libusb_get_device_descriptor (devs[i], &desc);
		if (res < 0)
			continue;

		if (!fnusb_is_camera(desc)) {
			continue;
		}

		if (nr_cam != index) {
			nr_cam++;
			continue;
		}

		camera = devs[i]; // found the requested camera
		dev->cam_bus = libusb_get_bus_number(camera);
		dev->cam_addr = libusb_get_device_address(camera);
		dev->serial[0] = '\0';
		dev->device_type = fnusb_detect_kinect_type(ctx, camera, devs, (int)count, &desc);
		if (ctx->enabled_subdevices & FREENECT_DEVICE_CAMERA)
		{
			res = libusb_open(camera, &dev->usb_cam.dev);
			if (res < 0 || !dev->usb_cam.dev)
			{
				FN_ERROR("Could not open camera: %s\n", libusb_error_name(res));
				dev->usb_cam.dev = NULL;
				goto failure;
			}

			if (dev->device_type == FN_KINECT_XBOX_1473 || dev->device_type == FN_KINECT_WINDOWS)
			{
				freenect_device_flags requested_devices = ctx->enabled_subdevices;

				// Not the 1414 kinect so remove the motor flag, this should preserve the audio flag if set
				ctx->enabled_subdevices = (freenect_device_flags)(ctx->enabled_subdevices & ~FREENECT_DEVICE_MOTOR);

				ctx->zero_plane_res = 334;
				dev->device_does_motor_control_with_audio = 1;

				// set the LED for non 1414 devices to keep the camera alive for some systems which get freezes
				libusb_device * audio = fnusb_find_sibling_device(ctx, camera, devs, count, &fnusb_is_audio);
				fnusb_keep_alive_led(ctx, audio);

				// for newer devices we need to enable the audio device for motor control
				// we only do this though if motor has been requested.
				if ((requested_devices & FREENECT_DEVICE_MOTOR) && (requested_devices & FREENECT_DEVICE_AUDIO) == 0)
				{
					ctx->enabled_subdevices = (freenect_device_flags)(ctx->enabled_subdevices | FREENECT_DEVICE_AUDIO);
				}
			}
			else
			{
				// The good old kinect that tilts and tweets
				ctx->zero_plane_res = 322;
			}

			dev->usb_cam.VID = desc.idVendor;
			dev->usb_cam.PID = desc.idProduct;

			res = fnusb_claim_camera(dev);
			if (res < 0) {
				goto failure;
			}
		}

		if (fnusb_read_kinect_serial(ctx, camera, dev->usb_cam.dev, devs, count, dev->serial, sizeof(dev->serial)) < 0)
			dev->serial[0] = '\0';
		break;
	}
	
	if (res < 0 || camera == NULL) {
		res = -1;
		goto failure;
	}

	// FIND MOTOR
	if (ctx->enabled_subdevices & FREENECT_DEVICE_MOTOR)
	{
		libusb_device* motor = fnusb_find_sibling_device(ctx, camera, devs, count, &fnusb_is_motor);
		if (motor == NULL) {
			FN_ERROR("Could not find device sibling\n");
			res = -1;
			goto failure;
		}

		struct libusb_device_descriptor desc;
		res = libusb_get_device_descriptor(motor, &desc);
		if (res < 0) {
			FN_ERROR("Could not query device: %s\n", libusb_error_name(res));
			goto failure;
		}

		res = libusb_open(motor, &dev->usb_motor.dev);
		if (res < 0 || !dev->usb_motor.dev)
		{
			FN_ERROR("Could not open device: %s\n", libusb_error_name(res));
			dev->usb_motor.dev = NULL;
			goto failure;
		}

		res = libusb_claim_interface(dev->usb_motor.dev, 0);
		if (res < 0) {
			FN_ERROR("Could not claim interface: %s\n", libusb_error_name(res));
			libusb_close(dev->usb_motor.dev);
			dev->usb_motor.dev = NULL;
			goto failure;
		}

		dev->usb_motor.VID = desc.idVendor;
		dev->usb_motor.PID = desc.idProduct;
	}
    
	// FIND AUDIO
	if (ctx->enabled_subdevices & FREENECT_DEVICE_AUDIO)
	{
		libusb_device* audio = fnusb_find_sibling_device(ctx, camera, devs, count, &fnusb_is_audio);
		if (audio == NULL) {
			FN_ERROR("Could not find device sibling\n");
			res = -1;
			goto failure;
		}

		res = fnusb_open_audio(dev, audio);
		if (res < 0)
			goto failure;
	}

	if ((dev->usb_cam.dev || !(ctx->enabled_subdevices & FREENECT_DEVICE_CAMERA))
   && (dev->usb_motor.dev || !(ctx->enabled_subdevices & FREENECT_DEVICE_MOTOR))
   && (dev->usb_audio.dev || !(ctx->enabled_subdevices & FREENECT_DEVICE_AUDIO)))
	{
		// Each requested subdevice is open.
		goto finally;
	}

failure:
	fnusb_close_subdevices(dev);

finally:
	libusb_free_device_list(devs, 1); // free the list, unref the devices in it
	return res;
}

// The audio controller of an open device rebooted (its transfers failed with
// LIBUSB_ERROR_NO_DEVICE) while the rest of the Kinect is still there: open it
// again, uploading its firmware once more. Returns 0 when it is back, and
// -ENODEV quickly if the camera is gone too (the Kinect was unplugged).
FN_INTERNAL int fnusb_recover_audio(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	libusb_device **devs, *camera, *audio = NULL;
	ssize_t count;
	int i, tries, res;

	if (dev->usb_audio.dev) {
		libusb_release_interface(dev->usb_audio.dev, 0);
		libusb_close(dev->usb_audio.dev);
		dev->usb_audio.dev = NULL;
	}
	dev->motor_control_with_audio_enabled = 0;

	// It may still be re-enumerating: look for it for up to 3 s.
	for (tries = 0; tries < 6 && !audio; tries++) {
		if (tries > 0)
			fn_sleep_ms(500);
		count = libusb_get_device_list(ctx->usb.ctx, &devs);
		if (count < 0)
			return (int)count;
		camera = NULL;
		for (i = 0; i < count; i++)
			if (libusb_get_bus_number(devs[i]) == dev->cam_bus && libusb_get_device_address(devs[i]) == dev->cam_addr)
				camera = devs[i];
		if (!camera) {
			libusb_free_device_list(devs, 1);
			return -ENODEV;
		}
		audio = fnusb_find_sibling_device(ctx, camera, devs, (int)count, &fnusb_is_audio);
		if (audio)
			libusb_ref_device(audio);
		libusb_free_device_list(devs, 1);
	}
	if (!audio)
		return -ENODEV;

	res = fnusb_open_audio(dev, audio);
	libusb_unref_device(audio);
	if (res < 0 && dev->usb_audio.dev) {
		libusb_release_interface(dev->usb_audio.dev, 0);
		libusb_close(dev->usb_audio.dev);
		dev->usb_audio.dev = NULL;
	}
	return res;
}

FN_INTERNAL int fnusb_close_subdevices(freenect_device *dev)
{
	if (dev->usb_cam.dev) {
		libusb_release_interface(dev->usb_cam.dev, 0); // re-attaches gspca_kinect (auto-detach)
		libusb_close(dev->usb_cam.dev);
		dev->usb_cam.dev = NULL;
	}
	if (dev->usb_motor.dev) {
		libusb_release_interface(dev->usb_motor.dev, 0);
		libusb_close(dev->usb_motor.dev);
		dev->usb_motor.dev = NULL;
	}
	if (dev->usb_audio.dev) {
		libusb_release_interface(dev->usb_audio.dev, 0);
		libusb_close(dev->usb_audio.dev);
		dev->usb_audio.dev = NULL;
	}
	return 0;
}

// Count a transfer's packets for the stream health check, and explain once
// when (almost) nothing arrives or much is lost.
static void iso_diag_count(freenect_context *ctx, fnusb_isoc_stream *strm, int pkts, int empty, int failed)
{
	char vm[128];
	const char *what = strm->parent == &strm->parent->parent->usb_audio ? "audio"
	                 : strm->endpoint == 0x81 ? "video" : "depth";
	fn_iso_verdict v = fn_iso_diag_add(&strm->diag, fn_now_ms(), pkts, empty, failed);
	if (v == FN_ISO_OK)
		return;
	if (v == FN_ISO_NO_DATA) {
		FN_WARNING("The Kinect's %s stream delivers no data: %u of %u isochronous packets in the last %d s were empty or failed.\n",
		           what, strm->diag.last_empty, strm->diag.last_pkts, FN_ISO_WINDOW_MS / 1000);
	} else {
		FN_NOTICE("The Kinect's %s stream is losing data: %u of %u isochronous packets in the last %d s failed.\n",
		          what, strm->diag.last_failed, strm->diag.last_pkts, FN_ISO_WINDOW_MS / 1000);
	}
	if (fn_detect_vm(vm, sizeof(vm)))
		FN_WARNING("This looks like a virtual machine (%s). USB passthrough often can't carry isochronous transfers "
		           "(usbip in WSL2, emulated EHCI/xHCI controllers). Use the Kinect on the host, or pass a whole USB "
		           "controller through to the VM (PCI passthrough). See \"Troubleshooting\" in libfreenect's README.\n", vm);
	else
		FN_WARNING("Plug the Kinect into a port directly on the computer (no hub, no extension), with its power supply "
		           "connected; if that doesn't help, try a port on another USB controller. See \"Troubleshooting\" in "
		           "libfreenect's README.\n");
}

// A transfer has finished for good (cancelled, failed, or not resubmitted).
// Counted once per transfer, so fnusb_stop_iso() knows which ones it still
// has to wait for.
static void iso_xfer_done(fnusb_isoc_stream *strm, struct libusb_transfer *xfer)
{
	int i;
	if (strm->xfers && strm->xfer_done) {
		for (i = 0; i < strm->num_xfers; i++) {
			if (strm->xfers[i] == xfer) {
				if (strm->xfer_done[i])
					return;
				strm->xfer_done[i] = 1;
				break;
			}
		}
	}
	strm->dead_xfers++;
}

static void LIBUSB_CALL iso_callback(struct libusb_transfer *xfer)
{
	int i;
	fnusb_isoc_stream *strm = (fnusb_isoc_stream*)xfer->user_data;
	freenect_context *ctx = strm->parent->parent->parent;

	if (strm->dead) {
		iso_xfer_done(strm, xfer);
		FN_SPEW("EP %02x transfer complete, %d left\n", xfer->endpoint, strm->num_xfers - strm->dead_xfers);
		return;
	}

	switch(xfer->status) {
		case LIBUSB_TRANSFER_COMPLETED: // Normal operation.
		{
			uint8_t *buf = (uint8_t*)xfer->buffer;
			int empty = 0, failed = 0;
			for (i=0; i<strm->pkts; i++) {
				// A completed transfer can still contain packets that failed or
				// were never filled (missed microframes, especially with WinUSB).
				// Their bytes are not data: pass them on as empty packets, so the
				// stream parser counts them as lost via the sequence numbers
				// instead of misparsing them and resyncing.
				const struct libusb_iso_packet_descriptor *desc = &xfer->iso_packet_desc[i];
				int len = (desc->status == LIBUSB_TRANSFER_COMPLETED) ? (int)desc->actual_length : 0;
				if (len == 0 && desc->status != LIBUSB_TRANSFER_COMPLETED)
					FN_SPEW("EP %02x: skipping iso packet %d with status %d\n", xfer->endpoint, i, desc->status);
				if (len == 0)
					empty++;
				if (desc->status != LIBUSB_TRANSFER_COMPLETED)
					failed++;
				strm->cb(strm->parent->parent, buf, len);
				buf += strm->len;
			}
			iso_diag_count(ctx, strm, strm->pkts, empty, failed);
			strm->error_logged = 0;
			int res;
			res = libusb_submit_transfer(xfer);
			if (res != 0) {
				if (!strm->resubmit_logged) {
					strm->resubmit_logged = 1;
					FN_ERROR("iso_callback(): failed to resubmit transfer on EP %02x after successful completion: %s\n",
					         xfer->endpoint, libusb_error_name(res));
				} else {
					FN_DEBUG("iso_callback(): failed to resubmit transfer on EP %02x: %s\n",
					         xfer->endpoint, libusb_error_name(res));
				}
				iso_xfer_done(strm, xfer);
				if (res == LIBUSB_ERROR_NO_DEVICE) {
					strm->parent->device_dead = 1;
				}
			}
			break;
		}
		case LIBUSB_TRANSFER_NO_DEVICE:
		{
			// We lost the device we were talking to.  This is a large problem,
			// and one that we should eventually come up with a way to
			// properly propagate up to the caller.
			if(!strm->parent->device_dead) {
				FN_ERROR("USB device disappeared, cancelling stream %02x :(\n", xfer->endpoint);
			}
			iso_xfer_done(strm, xfer);
			strm->parent->device_dead = 1;
			break;
		}
		case LIBUSB_TRANSFER_CANCELLED:
		{
			if(strm->dead) {
				FN_SPEW("EP %02x transfer cancelled\n", xfer->endpoint);
			} else {
				// This seems to be a libusb bug on OSX - instead of completing
				// the transfer with LIBUSB_TRANSFER_NO_DEVICE, the transfers
				// simply come back cancelled by the OS.  We can detect this,
				// though - the stream should be marked dead if we're
				// intentionally cancelling transfers.
				if(!strm->parent->device_dead) {
					FN_ERROR("Got cancelled transfer, but we didn't request it - device disconnected?\n");
				}
				strm->parent->device_dead = 1;
			}
			iso_xfer_done(strm, xfer);
			break;
		}
		default:
		{
			// On other errors, resubmit the transfer - in particular, libusb
			// on OSX tends to hit random errors a lot.  If we don't resubmit
			// the transfers, eventually all of them die and then we don't get
			// any more data from the Kinect.
			// Logged once until a transfer completes again: an unplugged
			// Kinect makes every outstanding transfer fail (on Windows with
			// this status rather than NO_DEVICE).
			if (!strm->error_logged) {
				strm->error_logged = 1;
				FN_WARNING("Isochronous transfer error on EP %02x: %d\n", xfer->endpoint, xfer->status);
			}
			iso_diag_count(ctx, strm, strm->pkts, strm->pkts, strm->pkts);
			int res;
			res = libusb_submit_transfer(xfer);
			if (res != 0) {
				if (!strm->resubmit_logged) {
					strm->resubmit_logged = 1;
					FN_ERROR("Isochronous transfer resubmission failed after an error on EP %02x: %s\n",
					         xfer->endpoint, libusb_error_name(res));
				} else {
					FN_DEBUG("Isochronous transfer resubmission failed after an error on EP %02x: %s\n",
					         xfer->endpoint, libusb_error_name(res));
				}
				iso_xfer_done(strm, xfer);
				if (res == LIBUSB_ERROR_NO_DEVICE) {
					strm->parent->device_dead = 1;
				}
			}
			break;
		}
	}
}

FN_INTERNAL int fnusb_get_max_iso_packet_size(fnusb_dev *dev, unsigned char endpoint, int default_size)
{
	freenect_context *ctx = dev->parent->parent;

	int size = libusb_get_max_iso_packet_size(libusb_get_device(dev->dev), endpoint);
	if (size <= 0)
	{
		FN_WARNING("libusb_get_max_iso_packet_size() returned %d; using default %d\n", size, default_size);
		size = default_size;
	}
	return size;
}

FN_INTERNAL int fnusb_start_iso(fnusb_dev *dev, fnusb_isoc_stream *strm, fnusb_iso_cb cb, unsigned char endpoint, int xfers, int pkts, int len)
{
	freenect_context *ctx = dev->parent->parent;

	strm->parent = dev;
	strm->cb = cb;
	strm->num_xfers = xfers;
	strm->pkts = pkts;
	strm->len = len;
	strm->buffer = (uint8_t*)malloc((size_t)xfers * pkts * len);
	strm->xfers = (struct libusb_transfer**)calloc(xfers, sizeof(struct libusb_transfer*));
	strm->xfer_done = (uint8_t*)calloc(xfers, 1);
	strm->dead = 0;
	strm->dead_xfers = 0;
	strm->endpoint = endpoint;
	memset(&strm->diag, 0, sizeof(strm->diag));
	strm->error_logged = 0;
	strm->resubmit_logged = 0;
	if (!strm->buffer || !strm->xfers || !strm->xfer_done)
	{
		FN_ERROR("Failed to allocate isochronous buffers for endpoint %02x\n", endpoint);
		free(strm->buffer);
		free(strm->xfers);
		free(strm->xfer_done);
		strm->buffer = NULL;
		strm->xfers = NULL;
		strm->xfer_done = NULL;
		return LIBUSB_ERROR_NO_MEM;
	}

	int i;
	int submitted = 0;
	int first_error = LIBUSB_SUCCESS;
	uint8_t *bufp = strm->buffer;

	for (i = 0; i < xfers; i++)
	{
		FN_SPEW("Creating endpoint %02x transfer #%d\n", endpoint, i);

		strm->xfers[i] = libusb_alloc_transfer(pkts);
		if (strm->xfers[i] == NULL)
		{
			FN_WARNING("Failed to allocate transfer\n");
			strm->xfer_done[i] = 1; strm->dead_xfers++;
			if (first_error == LIBUSB_SUCCESS)
				first_error = LIBUSB_ERROR_NO_MEM;
		}
		else
		{
			libusb_fill_iso_transfer(strm->xfers[i], dev->dev, endpoint, bufp, pkts * len, pkts, iso_callback, strm, 0);
			libusb_set_iso_packet_lengths(strm->xfers[i], len);

			int ret = libusb_submit_transfer(strm->xfers[i]);
			if (ret < 0)
			{
				FN_WARNING("Failed to submit isochronous transfer %d: %s\n", i, libusb_error_name(ret));
				strm->xfer_done[i] = 1; strm->dead_xfers++;
				if (first_error == LIBUSB_SUCCESS)
					first_error = ret;
			}
			else
			{
				submitted++;
			}
		}

		bufp += pkts*len;
	}

	if (submitted == 0)
	{
		// Nothing is in flight, so the stream can be torn down right here.
		FN_ERROR("Could not start isochronous stream on endpoint %02x: %s\n", endpoint, libusb_error_name(first_error));
		if (first_error == LIBUSB_ERROR_NOT_SUPPORTED)
		{
#ifdef _WIN32
			FN_ERROR("The driver bound to this Kinect function cannot do isochronous transfers. "
			         "Bind it to WinUSB (or libusbK); libusb0/libusb-win32 cannot stream. "
			         "WinUSB isochronous transfers also need Windows 8.1 or newer and libusb 1.0.26 or newer.\n");
#else
			FN_ERROR("The USB stack does not support isochronous transfers for this device.\n");
#endif
		}
		else if (first_error == LIBUSB_ERROR_ACCESS)
		{
#ifdef _WIN32
			FN_ERROR("Access denied: another program or driver is using this Kinect function.\n");
#else
			FN_ERROR("Access denied: install the udev rules (platform/linux/udev) or run with sufficient permissions.\n");
#endif
		}
		for (i = 0; i < xfers; i++)
			libusb_free_transfer(strm->xfers[i]);
		free(strm->buffer);
		free(strm->xfers);
		free(strm->xfer_done);
		strm->buffer = NULL;
		strm->xfers = NULL;
		strm->xfer_done = NULL;
		strm->dead = 1;
		return first_error;
	}
	if (submitted < xfers)
		FN_WARNING("Only %d of %d isochronous transfers started on endpoint %02x; frames may be dropped\n", submitted, xfers, endpoint);

	return 0;
}

FN_INTERNAL int fnusb_stop_iso(fnusb_dev *dev, fnusb_isoc_stream *strm)
{
	freenect_context *ctx = dev->parent->parent;
	int i;

	FN_FLOOD("fnusb_stop_iso() called\n");

	strm->dead = 1;

	if (!strm->xfers)
		return 0; // never started, or fnusb_start_iso() already cleaned up

	// Wait for the cancellations to complete, but not forever, so this function
	// itself cannot hang. Note the limit: if the OS never completes a
	// transfer (seen with WinUSB on the stalled audio stream), the process
	// still cannot exit until it does, and Windows shutdown can hang. That
	// case is prevented in freenect_start_audio(), not here.
	//
	// Cancel again every 200 ms whatever hasn't finished. libusb's WinUSB
	// backend (1.0.30) puts its own callback on isochronous transfers that
	// resubmits any transfer that did not complete, to restart the stream
	// without ContinueStream; a cancelled transfer counts as not completed, so
	// it is quietly resubmitted and our callback never sees it. The
	// resubmitted transfer has our callback again, so a second cancel ends it.
	// A transfer whose cancellation is still pending only answers
	// LIBUSB_ERROR_NOT_FOUND, which does no harm.
	// An unplugged Kinect's transfers that libusb's WinUSB backend lost while
	// resubmitting never come back, so don't wait as long for those.
	int waited_ms = 0;
	int wait_max_ms = dev->device_dead ? 1000 : 3000;
	while (strm->dead_xfers < strm->num_xfers && waited_ms < wait_max_ms) {
		struct timeval tv = { 0, 100000 };
		if (waited_ms % 200 == 0)
			for (i = 0; i < strm->num_xfers; i++)
				if (strm->xfers[i] && !strm->xfer_done[i])
					libusb_cancel_transfer(strm->xfers[i]);
		FN_FLOOD("fnusb_stop_iso() dead = %d\tnum = %d\n", strm->dead_xfers, strm->num_xfers);
		libusb_handle_events_timeout_completed(ctx->usb.ctx, &tv, NULL);
		waited_ms += 100;
	}
	if (strm->dead_xfers < strm->num_xfers) {
		// libusb may still own these transfers and write into the buffer, so
		// leak them rather than free memory that is still in use. Expected for
		// a Kinect that was unplugged; a warning otherwise.
		if (dev->device_dead)
			FN_INFO("fnusb_stop_iso(): %d of %d transfers of the unplugged Kinect's EP %02x never came back; leaking them\n",
			        strm->num_xfers - strm->dead_xfers, strm->num_xfers, strm->endpoint);
		else
			FN_WARNING("fnusb_stop_iso(): %d of %d transfers on this stream did not finish cancelling; leaking them\n",
			           strm->num_xfers - strm->dead_xfers, strm->num_xfers);
		strm->xfers = NULL;
		strm->buffer = NULL;
		strm->xfer_done = NULL;
		return 0;
	}
	FN_DEBUG("fnusb_stop_iso(): EP %02x stopped after %d ms\n", strm->endpoint, waited_ms);

	for (i=0; i<strm->num_xfers; i++)
		libusb_free_transfer(strm->xfers[i]);
	FN_FLOOD("fnusb_stop_iso() freed all transfers\n");

	free(strm->buffer);
	free(strm->xfers);
	free(strm->xfer_done);
	strm->buffer = NULL;
	strm->xfers = NULL;
	strm->xfer_done = NULL;

	FN_FLOOD("fnusb_stop_iso() freed buffers and stream\n");
	FN_FLOOD("fnusb_stop_iso() done\n");
	return 0;
}

FN_INTERNAL int fnusb_control(fnusb_dev *dev, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t wLength)
{
	// A finite timeout, as for bulk transfers: 0 would wait forever if the
	// device stops answering.
	int res = libusb_control_transfer(dev->dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, 5000);
	if (res == LIBUSB_ERROR_NO_DEVICE)
		dev->device_dead = 1; // unplugged
	return res;
}

FN_INTERNAL int fnusb_bulk(fnusb_dev *dev, uint8_t endpoint, uint8_t *data, int len, int *transferred) {
	int res;
	*transferred = 0;
	// A finite timeout: 0 would wait forever if the audio controller stops answering.
	res = libusb_bulk_transfer(dev->dev, endpoint, data, len, transferred, 5000);
	if (res == LIBUSB_ERROR_NO_DEVICE)
		dev->device_dead = 1; // unplugged (or, during a firmware upload, rebooted)
	return res;
}

FN_INTERNAL int fnusb_clear_halt(fnusb_dev *dev, uint8_t endpoint) {
	return libusb_clear_halt(dev->dev, endpoint);
}

FN_INTERNAL uint64_t fn_now_ms(void) {
#ifdef _WIN32
	return GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
#endif
}

// Monotonic microseconds, for frame time stamps (stream timing alignment).
FN_INTERNAL uint64_t fn_now_us(void) {
#ifdef _WIN32
	LARGE_INTEGER f, c;
	QueryPerformanceFrequency(&f);
	QueryPerformanceCounter(&c);
	return (uint64_t)(c.QuadPart / f.QuadPart) * 1000000 + (uint64_t)(c.QuadPart % f.QuadPart) * 1000000 / (uint64_t)f.QuadPart;
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)(ts.tv_nsec / 1000);
#endif
}

FN_INTERNAL void fn_sleep_ms(unsigned ms) {
#ifdef _WIN32
	Sleep(ms);
#else
	usleep((useconds_t)ms * 1000);
#endif
}

FN_INTERNAL int fnusb_num_interfaces(fnusb_dev *dev) {
	int retval = 0;
	int res;
	libusb_device* d = libusb_get_device(dev->dev);
	struct libusb_config_descriptor* config;
	res = libusb_get_active_config_descriptor(d, &config);
	if (res < 0) // Something went wrong
		return res;
	retval = config->bNumInterfaces;
	libusb_free_config_descriptor(config);
	return retval;
}

// 1 if the device has a standard USB Audio Class interface. That is the case
// when the audio controller runs Microsoft's UAC firmware: its microphones are
// then an ordinary USB audio device owned by the operating system's audio
// stack, and libfreenect's own audio protocol does not apply.
FN_INTERNAL int fnusb_has_audio_class(fnusb_dev *dev) {
	struct libusb_config_descriptor* config;
	int i, j, found = 0;
	if (!dev->dev || libusb_get_active_config_descriptor(libusb_get_device(dev->dev), &config) < 0)
		return 0;
	for (i = 0; i < config->bNumInterfaces && !found; i++)
		for (j = 0; j < config->interface[i].num_altsetting && !found; j++)
			if (config->interface[i].altsetting[j].bInterfaceClass == LIBUSB_CLASS_AUDIO)
				found = 1;
	libusb_free_config_descriptor(config);
	return found;
}
