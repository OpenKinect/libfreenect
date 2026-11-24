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
 * Kinect for Windows colour camera settings (§10C).
 *
 * The camera takes these registers only while video streams, so values are
 * kept in the device and written by fn_color_apply_pending(), called from
 * freenect_process_events() once a video frame has arrived (Kinect10 too
 * applies its settings on the first frame after a start, §10D.1.3). They are
 * not written from the USB callback, which runs inside libusb's event
 * handling, where synchronous transfers can't be made.
 */

#include <errno.h>
#include <string.h>

#include "freenect_internal.h"
#include "color_settings.h"
#include "flags.h"

// GM command 0x96: colour-sensor registers (§10C.1)
static int color_write(freenect_device *dev, int width, uint16_t reg, uint16_t val)
{
	uint16_t cmd[5];
	uint8_t reply[16];
	unsigned int len;
	if (width == 16) {
		cmd[0] = fn_le16(1);
		cmd[1] = fn_le16(reg);
		cmd[2] = fn_le16(val);
		len = 6;
	} else {
		// indirect: address and data pseudo-registers
		cmd[0] = fn_le16(2);
		cmd[1] = fn_le16(0xFF80);
		cmd[2] = fn_le16(reg);
		cmd[3] = fn_le16(0xFF81);
		cmd[4] = fn_le16(val & 0xff);
		len = 10;
	}
	return send_cmd(dev, 0x96, cmd, len, reply, sizeof(reply));
}

// Returns the value, or < 0. The reply's byte 2 bit 0 says it is valid, and
// the value is the 16-bit word at byte 6 (reply+0x0a and +0x0e with the
// 8-byte header, §5).
static int color_read(freenect_device *dev, int width, uint16_t reg)
{
	uint16_t cmd[3];
	uint8_t reply[16];
	int res;
	cmd[0] = fn_le16(1);
	cmd[1] = fn_le16(width == 16 ? 0xFF82 : 0xFF83);
	cmd[2] = fn_le16(reg);
	memset(reply, 0, sizeof(reply));
	res = send_cmd(dev, 0x96, cmd, 6, reply, sizeof(reply));
	if (res < 8)
		return res < 0 ? res : -EIO;
	if (!(reply[2] & 1))
		return -EIO;
	res = reply[6] | (reply[7] << 8);
	return width == 8 ? (res & 0xff) : res;
}

// Kinect10's WriteReg: verified with retries >= 0, repeated without reading
// back otherwise.
static int color_write_retry(freenect_device *dev, const fn_color_write *w)
{
	freenect_context *ctx = dev->parent;
	int tries = (w->retries < 0 ? -w->retries : w->retries) + 2;
	int t, res = -EIO;
	uint16_t want = w->width == 8 ? (w->val & 0xff) : w->val;
	for (t = 0; t < tries; t++) {
		res = color_write(dev, w->width, w->reg, w->val);
		if (w->retries < 0 || res < 0)
			continue;
		if (color_read(dev, w->width, w->reg) == want)
			return 0;
		res = -EIO;
	}
	if (w->retries < 0 && res >= 0)
		return 0;
	FN_WARNING("Colour setting: register %04x <= %04x did not take\n", w->reg, w->val);
	return res < 0 ? res : -EIO;
}

static int write_all(freenect_device *dev, const fn_color_write *w, int n)
{
	int i, res = 0;
	for (i = 0; i < n; i++) {
		int r = color_write_retry(dev, &w[i]);
		if (r < 0)
			res = r;
	}
	return res;
}

static int write_awb_table(freenect_device *dev, const uint16_t *vals)
{
	int i, res = 0;
	for (i = 0; i < FN_COLOR_AWB_TABLE_LEN; i++) {
		fn_color_write w = { 16, fn_color_awb_regs[i], vals[i], 0 };
		if (color_write_retry(dev, &w) < 0)
			res = -EIO;
	}
	{
		fn_color_write c = { 16, FN_COLOR_REG_COMMIT, 0x28, -1 };
		color_write_retry(dev, &c);
	}
	return res;
}

// Writes one setting now, with the side effects Kinect10 gives it (§10C.2).
// A setting whose auto exposure / auto white balance precondition isn't met
// is skipped; switching that mode re-applies it.
static int write_setting(freenect_device *dev, freenect_color_setting s)
{
	const fn_color_info *info = fn_color_setting_info(s);
	double v = dev->color_value[s];
	int ae = dev->color_value[FREENECT_COLOR_AUTO_EXPOSURE] != 0;
	int awb = dev->color_value[FREENECT_COLOR_AUTO_WHITE_BALANCE] != 0;
	fn_color_write w[2];
	int n, res;

	if ((info->needs_ae > 0 && !ae) || (info->needs_ae < 0 && ae) || (info->needs_awb < 0 && awb))
		return 0;
	n = fn_color_encode(s, v, w);
	if (n < 0)
		return n;

	switch (s) {
		case FREENECT_COLOR_AUTO_EXPOSURE:
			res = write_all(dev, w, n);
			if (ae) {
				write_setting(dev, FREENECT_COLOR_BRIGHTNESS);
				write_setting(dev, FREENECT_COLOR_POWER_LINE_FREQUENCY);
				write_setting(dev, FREENECT_COLOR_BACKLIGHT_COMPENSATION);
			} else {
				write_setting(dev, FREENECT_COLOR_FRAME_INTERVAL); // also re-applies the exposure time
				write_setting(dev, FREENECT_COLOR_GAIN);
			}
			return res;
		case FREENECT_COLOR_FRAME_INTERVAL:
			res = write_all(dev, w, n);
			write_setting(dev, FREENECT_COLOR_EXPOSURE_TIME);
			return res;
		case FREENECT_COLOR_POWER_LINE_FREQUENCY: {
			// bit 1 of 0xCC20 enables the filter; 0xCC03 selects 50 or 60 Hz
			int r = color_read(dev, 8, FN_COLOR_REG_PL_ENABLE);
			fn_color_write en = { 8, FN_COLOR_REG_PL_ENABLE, 0, 0x10 };
			if (r < 0)
				return r;
			en.val = (uint16_t)(w[0].val ? (r | 2) : (r & ~2));
			res = color_write_retry(dev, &en);
			if (res < 0)
				return res;
			return write_all(dev, w, n);
		}
		case FREENECT_COLOR_AUTO_WHITE_BALANCE:
			if (awb) {
				res = write_all(dev, w, n);
				write_awb_table(dev, fn_color_awb_on_vals);
			} else {
				write_awb_table(dev, fn_color_awb_off_vals);
				res = write_all(dev, w, n);
				write_setting(dev, FREENECT_COLOR_WHITE_BALANCE);
			}
			return res;
		default:
			return write_all(dev, w, n);
	}
}

FN_INTERNAL void fn_color_init(freenect_device *dev)
{
	int s;
	for (s = 0; s < FREENECT_COLOR_SETTING_COUNT; s++)
		dev->color_value[s] = fn_color_setting_info((freenect_color_setting)s)->def;
	dev->color_set_mask = 0;
	dev->color_pending = 0;
}

// Called by freenect_start_video(): after every start the settings are sent
// again, once frames arrive.
FN_INTERNAL void fn_color_on_video_start(freenect_device *dev)
{
	if (dev->color_set_mask)
		dev->color_pending = dev->color_set_mask;
}

// Called by freenect_process_events(), outside libusb's event handling.
FN_INTERNAL void fn_color_apply_pending(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	uint32_t pending = dev->color_pending;
	int s;
	if (!pending || !dev->video.running || dev->video.valid_frames < 1 || dev->usb_cam.device_dead)
		return;
	dev->color_pending = 0;
	// The modes first, so that the settings depending on them see their state.
	if (pending & (1u << FREENECT_COLOR_AUTO_EXPOSURE))
		write_setting(dev, FREENECT_COLOR_AUTO_EXPOSURE);
	if (pending & (1u << FREENECT_COLOR_AUTO_WHITE_BALANCE))
		write_setting(dev, FREENECT_COLOR_AUTO_WHITE_BALANCE);
	for (s = 0; s < FREENECT_COLOR_SETTING_COUNT; s++)
		if (s != FREENECT_COLOR_AUTO_EXPOSURE && s != FREENECT_COLOR_AUTO_WHITE_BALANCE && (pending & (1u << s)))
			write_setting(dev, (freenect_color_setting)s);
	FN_INFO("Colour camera settings applied\n");
}

int freenect_set_color_setting(freenect_device *dev, freenect_color_setting setting, double value)
{
	fn_color_write w[2];
	if (dev->device_type != FN_KINECT_WINDOWS)
		return -ENOTSUP; // as the SDK: only the Kinect for Windows has them
	if (fn_color_encode(setting, value, w) < 0)
		return -EINVAL;
	dev->color_value[setting] = value;
	dev->color_set_mask |= 1u << setting;
	dev->color_pending |= 1u << setting;
	return 0;
}

int freenect_get_color_setting(freenect_device *dev, freenect_color_setting setting, double *value)
{
	if (dev->device_type != FN_KINECT_WINDOWS)
		return -ENOTSUP;
	if (!fn_color_setting_info(setting) || !value)
		return -EINVAL;
	*value = dev->color_value[setting];
	return 0;
}
