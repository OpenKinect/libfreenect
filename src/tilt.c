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
#include <math.h>

#include "freenect_internal.h"

// Tilt limits as in Microsoft's Kinect SDK (NUI_CAMERA_ELEVATION_MAXIMUM/MINIMUM).
// The motor can travel a little further (the 1473/K4W firmware clamps to +-30),
// but the SDK never asks for more than +-27.
#define MAX_TILT_ANGLE 27
#define MIN_TILT_ANGLE (-27)

// Motor pacing, also as in Microsoft's runtime (Kinect10, see the reverse-
// engineering reference, sections 10E.2.1 and 10E.2.2):
#define MOTOR_CTRL_SPACING_MS 10   // 1414: between control transfers
#define TILT_MIN_INTERVAL_MS 1000  // between moves
#define TILT_WINDOW_MS 20000       // at most TILT_WINDOW_MOVES moves per window
#define TILT_WINDOW_MOVES 15
#define LEVEL_MIN_G 0.1            // below this the accelerometer is not trusted
#define LEVEL_MIN_COS 0.309        // cos 72 deg: more tipped than this, do not move

#define GRAVITY 9.80665

// Structs for 1473 and K4W communication (bulk commands on the audio controller)
typedef struct {
	uint32_t magic;
	uint32_t tag;
	uint32_t arg1;
	uint32_t cmd;
	uint32_t arg2;
}fn_alt_motor_command;

typedef struct {
	uint32_t magic;
	uint32_t tag;
	uint32_t status;
}fn_alt_motor_reply;

static int tag_seq = 0;

// On a rejected command the firmware stalls its bulk pipes instead of
// replying; clear both so the next command can go through.
static void alt_clear_halts(libusb_device_handle* dev)
{
	libusb_clear_halt(dev, 0x01);
	libusb_clear_halt(dev, 0x81);
}

static int get_reply(libusb_device_handle* dev, freenect_context *ctx){
	unsigned char buffer[512];
	memset(buffer, 0, 512);
	int transferred = 0;
	int res = 0;
	res = libusb_bulk_transfer(dev, 0x81, buffer, 512, &transferred, 100);
	if (res != 0) {
		FN_ERROR("get_reply(): libusb_bulk_transfer failed: %s (transferred = %d)\n", libusb_error_name(res), transferred);
		alt_clear_halts(dev);
	} else if (transferred != 12) {
		FN_ERROR("get_reply(): weird - got %d bytes (expected 12)\n", transferred);
		res = -1;
	} else {
		fn_alt_motor_reply reply;
		memcpy(&reply, buffer, sizeof(reply));
		if (fn_le32(reply.magic) != 0x0a6fe000) {
			FN_ERROR("Bad magic: %08X (expected 0A6FE000)\n", fn_le32(reply.magic));
			res = -1;
		}
		if (fn_le32(reply.status) != 0) {
			FN_ERROR("reply status %u: the command failed\n", fn_le32(reply.status));
			res = -1;
		}
	}
	return res;
}

// Build number of the running UAC firmware: cmd 0x09 returns a 0x200-byte
// block with the build at +0x44 (709 for v1.8, 390 for SDK beta 2), then the
// usual status. Returns the build, or < 0 if the firmware doesn't answer it.
FN_INTERNAL int fnusb_audio_fw_build(libusb_device_handle *audio, freenect_context *ctx)
{
	fn_alt_motor_command cmd;
	unsigned char buffer[512];
	int transferred = 0, res;
	uint32_t build;

	cmd.magic = fn_le32(0x06022009);
	cmd.tag = fn_le32(tag_seq++);
	cmd.arg1 = fn_le32(0x200); // expected reply length
	cmd.cmd = fn_le32(0x09);
	cmd.arg2 = 0;
	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, &cmd, 16);
	res = libusb_bulk_transfer(audio, 0x01, buffer, 16, &transferred, 250);
	if (res != 0) {
		alt_clear_halts(audio);
		return res;
	}
	res = libusb_bulk_transfer(audio, 0x81, buffer, sizeof(buffer), &transferred, 250);
	if (res != 0) {
		alt_clear_halts(audio);
		return res;
	}
	if (transferred < 0x48) {
		FN_INFO("fnusb_audio_fw_build(): %d bytes of build info, expected 512\n", transferred);
		get_reply(audio, ctx);
		return -1;
	}
	memcpy(&build, buffer + 0x44, 4);
	if (get_reply(audio, ctx) < 0)
		return -1;
	return (int)fn_le32(build);
}

freenect_raw_tilt_state* freenect_get_tilt_state(freenect_device *dev)
{
	return &dev->raw_state;
}

// Read the 0x68-byte motor/accelerometer state (command 0x8032) of a 1473 or
// Kinect for Windows. Layout, from the audio firmware's reply builder and
// Microsoft's runtime:
//   0x04  u32  flags; bit 0 = motor moving
//   0x1C  s16  tilt angle in whole degrees (8-sample average); -64 = invalid
//   0x20  u8   result of the last move: 0 reached, 1 stall, 3 timeout,
//              5 jolt abort, 7 stopped by host, 8 link handshake failed
//   0x30, 0x34, 0x38  s32  accelerometer x, y, z (819 counts per g)
#define ALT_STATE_LEN 0x68
int update_tilt_state_alt(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;

	if (dev->usb_audio.dev == NULL)
	{
		FN_WARNING("Motor control failed: audio device missing\n");
		return -1;
	}

	int transferred = 0;
	int res = 0;
	fn_alt_motor_command cmd;
	cmd.magic = fn_le32(0x06022009);
	cmd.tag = fn_le32(tag_seq++);
	cmd.arg1 = fn_le32(ALT_STATE_LEN); // expected reply length
	cmd.cmd = fn_le32(0x8032);

	unsigned char buffer[256];
	memset(buffer, 0, sizeof(buffer));
	memcpy(buffer, &cmd, 16);

	res = libusb_bulk_transfer(dev->usb_audio.dev, 0x01, buffer, 16, &transferred, 250);
	if (res != 0)
	{
		alt_clear_halts(dev->usb_audio.dev);
		return res;
	}

	res = libusb_bulk_transfer(dev->usb_audio.dev, 0x81, buffer, 256, &transferred, 250);
	if (res != 0)
	{
		alt_clear_halts(dev->usb_audio.dev);
		return res;
	}
	if (transferred != ALT_STATE_LEN)
	{
		FN_ERROR("update_tilt_state_alt(): got %d bytes of motor state, expected %d\n", transferred, ALT_STATE_LEN);
		get_reply(dev->usb_audio.dev, ctx);
		return -1;
	}

	uint32_t flags;
	int16_t tilt;
	int32_t ax, ay, az;
	memcpy(&flags, buffer + 0x04, 4);
	memcpy(&tilt, buffer + 0x1C, 2);
	memcpy(&ax, buffer + 0x30, 4);
	memcpy(&ay, buffer + 0x34, 4);
	memcpy(&az, buffer + 0x38, 4);
	flags = fn_le32(flags);
	tilt = (int16_t)fn_le16((uint16_t)tilt);
	ax = (int32_t)fn_le32((uint32_t)ax);
	ay = (int32_t)fn_le32((uint32_t)ay);
	az = (int32_t)fn_le32((uint32_t)az);
	uint8_t result = buffer[0x20];
	FN_SPEW("Motor state: flags %08x tilt %d result %u accel %d %d %d\n", flags, tilt, result, ax, ay, az);

	dev->raw_state.accelerometer_x = (int16_t)ax;
	dev->raw_state.accelerometer_y = (int16_t)ay;
	dev->raw_state.accelerometer_z = (int16_t)az;

	// The 1414 reports half-degrees and freenect_get_tilt_degs() divides by 2.
	if (tilt != -64)
		dev->raw_state.tilt_angle = (int8_t)(tilt * 2);

	if (flags & 1)
		dev->raw_state.tilt_status = TILT_STATUS_MOVING;
	else if (result == 1) // stalled: reached a mechanical limit
		dev->raw_state.tilt_status = TILT_STATUS_LIMIT;
	else
		dev->raw_state.tilt_status = TILT_STATUS_STOPPED;

	return get_reply(dev->usb_audio.dev, ctx);
}

// 1414: keep motor control transfers at least MOTOR_CTRL_SPACING_MS apart.
static int motor_control(freenect_device *dev, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue,
                         uint8_t *data, uint16_t wLength)
{
	uint64_t now = fn_now_ms();
	if (dev->motor_last_ctrl_ms != 0 && now - dev->motor_last_ctrl_ms < MOTOR_CTRL_SPACING_MS)
		fn_sleep_ms((unsigned)(MOTOR_CTRL_SPACING_MS - (now - dev->motor_last_ctrl_ms)));
	int ret = fnusb_control(&dev->usb_motor, bmRequestType, bRequest, wValue, 0x0, data, wLength);
	dev->motor_last_ctrl_ms = fn_now_ms();
	return ret;
}

int freenect_update_tilt_state(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;

	//if we have motor control via audio and fw is uploaded - call the alt function
	if( dev->motor_control_with_audio_enabled ){
		return update_tilt_state_alt(dev);
	}

	if(!(ctx->enabled_subdevices & FREENECT_DEVICE_MOTOR))
		return 0;

	uint8_t buf[10];
	uint16_t ux, uy, uz;
	int ret = motor_control(dev, 0xC0, 0x32, 0x0, buf, 10);
	if (ret != 10) {
		FN_ERROR("Error in accelerometer reading, libusb_control_transfer returned %d\n", ret);
		return ret < 0 ? ret : -1;
	}

	ux = ((uint16_t)buf[2] << 8) | buf[3];
	uy = ((uint16_t)buf[4] << 8) | buf[5];
	uz = ((uint16_t)buf[6] << 8) | buf[7];

	dev->raw_state.accelerometer_x = (int16_t)ux;
	dev->raw_state.accelerometer_y = (int16_t)uy;
	dev->raw_state.accelerometer_z = (int16_t)uz;
	dev->raw_state.tilt_angle = (int8_t)buf[8];
	dev->raw_state.tilt_status = (freenect_tilt_status_code)buf[9];

	return ret;
}

// Is the rate limit on? LIBFREENECT_TILT_RATE_LIMIT=0 turns it off.
static int tilt_rate_limit_enabled(void)
{
	const char *v = getenv("LIBFREENECT_TILT_RATE_LIMIT");
	return !(v && v[0] == '0');
}

// The checks Microsoft's runtime makes before every move. Returns 1 to go
// ahead, 0 to skip the move silently (sensor tipped over, as the SDK does),
// or a negative errno for a rate-limited request.
static int tilt_move_allowed(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;

	if (tilt_rate_limit_enabled()) {
		uint64_t now = fn_now_ms();
		if (dev->tilt_last_move_ms != 0 && now - dev->tilt_last_move_ms < TILT_MIN_INTERVAL_MS) {
			FN_WARNING("set_tilt(): moves must be at least %d ms apart; try again later\n", TILT_MIN_INTERVAL_MS);
			return -EAGAIN;
		}
		if (dev->tilt_window_start_ms == 0 || now - dev->tilt_window_start_ms >= TILT_WINDOW_MS) {
			dev->tilt_window_start_ms = now;
			dev->tilt_window_count = 0;
		}
		if (dev->tilt_window_count + 1 > TILT_WINDOW_MOVES) {
			FN_WARNING("set_tilt(): at most %d moves per %d s, to protect the motor; try again later\n",
			           TILT_WINDOW_MOVES, TILT_WINDOW_MS / 1000);
			return -EAGAIN;
		}
	}

	// Level check on fresh accelerometer data.
	if (freenect_update_tilt_state(dev) < 0)
		return 1; // cannot read it; the SDK would fail here, libfreenect moves as before
	{
		double x = dev->raw_state.accelerometer_x / (double)FREENECT_COUNTS_PER_G;
		double y = dev->raw_state.accelerometer_y / (double)FREENECT_COUNTS_PER_G;
		double z = dev->raw_state.accelerometer_z / (double)FREENECT_COUNTS_PER_G;
		double n = sqrt(x * x + y * y + z * z);
		if (n < LEVEL_MIN_G || y / n < LEVEL_MIN_COS) {
			FN_NOTICE("set_tilt(): the Kinect is tipped over (or its accelerometer reads ~0 g); not moving\n");
			return 0;
		}
	}
	return 1;
}

static void tilt_move_done(freenect_device *dev)
{
	dev->tilt_last_move_ms = fn_now_ms();
	dev->tilt_window_count++;
}

static int clamp_tilt(freenect_context *ctx, double angle)
{
	if (angle > MAX_TILT_ANGLE || angle < MIN_TILT_ANGLE) {
		FN_WARNING("set_tilt(): %.1f degrees is outside [%d, %d]; clamping\n", angle, MIN_TILT_ANGLE, MAX_TILT_ANGLE);
		angle = angle > MAX_TILT_ANGLE ? MAX_TILT_ANGLE : MIN_TILT_ANGLE;
	}
	return (int)lround(angle);
}

int freenect_set_tilt_degs_alt(freenect_device *dev, int tilt_degrees)
{
	freenect_context *ctx = dev->parent;

	if (dev->usb_audio.dev == NULL)
	{
		FN_WARNING("Motor control failed: audio device missing\n");
		return -1;
	}

	fn_alt_motor_command cmd;
	cmd.magic = fn_le32(0x06022009);
	cmd.tag = fn_le32(tag_seq++);
	cmd.arg1 = fn_le32(0);
	cmd.cmd = fn_le32(0x803b);
	cmd.arg2 = (uint32_t)(fn_le32((int32_t)tilt_degrees));
	int transferred = 0;
	int res = 0;
	unsigned char buffer[20];
	memcpy(buffer, &cmd, 20);

	res = libusb_bulk_transfer(dev->usb_audio.dev, 0x01, buffer, 20, &transferred, 250);
	if (res != 0) {
		FN_ERROR("freenect_set_tilt_alt(): libusb_bulk_transfer failed: %s (transferred = %d)\n", libusb_error_name(res), transferred);
		alt_clear_halts(dev->usb_audio.dev);
		return res;
	}

	return get_reply(dev->usb_audio.dev, ctx);
}

int freenect_set_tilt_degs(freenect_device *dev, double angle)
{
	freenect_context *ctx = dev->parent;
	int degrees, allowed, ret;

	if (!dev->motor_control_with_audio_enabled && !(ctx->enabled_subdevices & FREENECT_DEVICE_MOTOR))
		return 0;

	degrees = clamp_tilt(ctx, angle);
	allowed = tilt_move_allowed(dev);
	if (allowed <= 0)
		return allowed; // 0: skipped because the Kinect is tipped over; < 0: rate-limited

	//if we have motor control via audio and fw is uploaded - call the alt function
	if( dev->motor_control_with_audio_enabled ){
		ret = freenect_set_tilt_degs_alt(dev, degrees);
	} else {
		uint8_t empty[0x1];
		ret = motor_control(dev, 0x40, 0x31, (uint16_t)(int16_t)(degrees * 2), empty, 0x0);
	}
	if (ret >= 0)
		tilt_move_done(dev);
	return ret;
}

// LED values of the 1473/K4W audio firmware (command 0x10): the green LED is
// driven by PWM, the red one by an I2C expander; there is no yellow.
enum
{
	LED_ALT_OFF = 1,
	LED_ALT_BLINK_GREEN = 2,
	LED_ALT_SOLID_GREEN = 3,
	LED_ALT_SOLID_RED = 4,
};

static uint32_t led_alt_value(freenect_led_options state)
{
	switch (state) {
	case LED_OFF: return LED_ALT_OFF;
	case LED_GREEN: return LED_ALT_SOLID_GREEN;
	case LED_RED: return LED_ALT_SOLID_RED;
	case LED_YELLOW: return LED_ALT_SOLID_GREEN;   // no yellow on these models
	case LED_BLINK_GREEN: return LED_ALT_BLINK_GREEN;
	default: return LED_ALT_BLINK_GREEN;           // e.g. LED_BLINK_RED_YELLOW: blinking is the closest
	}
}

FN_INTERNAL int fnusb_set_led_alt(libusb_device_handle * dev, freenect_context * ctx, freenect_led_options state)
{
	fn_alt_motor_command cmd;
	cmd.magic = fn_le32(0x06022009);
	cmd.tag = fn_le32(tag_seq++);
	cmd.arg1 = fn_le32(0);
	cmd.cmd = fn_le32(0x10);
	cmd.arg2 = fn_le32(led_alt_value(state));

	unsigned char buffer[20];
	memcpy(buffer, &cmd, 20);

	int transferred = 0;
	int res = libusb_bulk_transfer(dev, 0x01, buffer, 20, &transferred, 100);
	if (res != 0) {
		// Firmware images without the LED command stall the pipe instead of replying.
		FN_WARNING("fnusb_set_led_alt(): libusb_bulk_transfer failed: %s (transferred = %d)\n", libusb_error_name(res), transferred);
		alt_clear_halts(dev);
		return res;
	}
	return get_reply(dev, ctx);
}

int freenect_set_led_alt(freenect_device *dev, freenect_led_options state)
{
	freenect_context *ctx = dev->parent;

	if (dev->usb_audio.dev == NULL)
	{
		FN_WARNING("Motor control failed: audio device missing\n");
		return -1;
	}
	if (dev->audio_fw_build > 0 && dev->audio_fw_build < FN_UAC_LED_MIN_BUILD) {
		// The command would be rejected and stall the pipes.
		if (!dev->led_unsupported_noted) {
			dev->led_unsupported_noted = 1;
			FN_NOTICE("The audio firmware (build %d) has no LED command; load Microsoft's UAC.BIN v1.8 "
			          "(build %d) to control the LED\n", dev->audio_fw_build, FN_UAC_LED_MIN_BUILD);
		}
		return -1;
	}

	return fnusb_set_led_alt(dev->usb_audio.dev, ctx, state);
}

// LIBFREENECT_LED=off keeps the LED off whatever applications ask for (for
// example at night), on every model.
FN_INTERNAL int fn_led_forced_off(void)
{
	const char *v = getenv("LIBFREENECT_LED");
	return v && strcmp(v, "off") == 0;
}

int freenect_set_led(freenect_device *dev, freenect_led_options option)
{
	freenect_context *ctx = dev->parent;

	if (option != LED_OFF && fn_led_forced_off()) {
		FN_INFO("LIBFREENECT_LED=off: keeping the LED off\n");
		option = LED_OFF;
	}
	dev->led_changed = 1; // freenect_close_device() puts it back

	//if we have motor control via audio and fw is uploaded - call the alt function
	if( dev->motor_control_with_audio_enabled ){
		return freenect_set_led_alt(dev, option);
	}

	if(!(ctx->enabled_subdevices & FREENECT_DEVICE_MOTOR))
		return 0;

	uint8_t empty[0x1];
	return motor_control(dev, 0x40, 0x06, (uint16_t)option, empty, 0x0);
}

double freenect_get_tilt_degs(freenect_raw_tilt_state *state)
{
	return ((double)state->tilt_angle) / 2.;
}

freenect_tilt_status_code freenect_get_tilt_status(freenect_raw_tilt_state *state)
{
	return state->tilt_status;
}

void freenect_get_mks_accel(freenect_raw_tilt_state *state, double* x, double* y, double* z)
{
	//the documentation for the accelerometer (http://www.kionix.com/Product%20Sheets/KXSD9%20Product%20Brief.pdf)
	//states there are 819 counts/g
	*x = (double)state->accelerometer_x/FREENECT_COUNTS_PER_G*GRAVITY;
	*y = (double)state->accelerometer_y/FREENECT_COUNTS_PER_G*GRAVITY;
	*z = (double)state->accelerometer_z/FREENECT_COUNTS_PER_G*GRAVITY;
}
