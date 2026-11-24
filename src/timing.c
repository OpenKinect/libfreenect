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
 * Depth/colour frame phase alignment (§10C.4, §20.1.6, §20.7.3).
 *
 * freenect_nudge_stream_timing() shifts the depth stream's frame phase by a
 * number of microseconds, as Microsoft's driver does. With
 * freenect_set_stream_timing_auto(dev, 1), libfreenect measures the phase
 * between colour and depth frames and nudges depth as Kinect10 does; this
 * runs in freenect_process_events(), outside libusb's event handling.
 */

#include <errno.h>
#include <string.h>

#include "freenect_internal.h"
#include "flags.h"
#include "stream_timing.h"

// Microsoft's driver stamps depth frames 3600 µs earlier than colour (§10A.4).
#define FN_DEPTH_TIME_OFFSET_US 3600

// The four timing floats of command 0x16 parameter 6 (§20.1.6). The reply
// starts with a status word.
FN_INTERNAL int fn_fetch_timing_params(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	uint16_t cmd[5];
	uint8_t reply[0x40];
	float f[4];
	int res;

	cmd[0] = fn_le16(0x06); // ParamID: timing
	cmd[1] = fn_le16(0);    // Format
	cmd[2] = fn_le16(1);    // Resolution, as in Microsoft's init script
	cmd[3] = fn_le16(30);   // FPS
	cmd[4] = fn_le16(0);    // Offset
	res = send_cmd(dev, 0x16, cmd, 10, reply, sizeof(reply));
	if (res < 2 + 16)
		return -1;
	{
		int i;
		for (i = 0; i < 4; i++) {
			uint32_t w;
			memcpy(&w, reply + 2 + 4 * i, 4);
			w = fn_le32(w);
			memcpy(&f[i], &w, 4);
		}
	}
	if (!(f[2] > 0.0f && f[2] < 1.0e6f) || !(f[3] > -1.0e9f && f[3] < 1.0e9f))
		return -1;
	dev->timing_k2 = (int32_t)(f[2] * 1000.0f);
	dev->timing_k3 = (int32_t)f[3];
	FN_DEBUG("Stream timing parameters: %f %f %f %f\n", f[0], f[1], f[2], f[3]);
	return dev->timing_k2 > 0 ? 0 : -1;
}

int freenect_nudge_stream_timing(freenect_device *dev, int depth_nudge_us)
{
	freenect_context *ctx = dev->parent;
	uint16_t cmd[3];
	uint8_t reply[16];
	int v = fn_timing_nudge_value(depth_nudge_us, dev->timing_k2, dev->timing_k3);
	int res;
	if (v < 0)
		return -ENOTSUP;
	cmd[0] = fn_le16((uint16_t)v);
	cmd[1] = fn_le16(1);
	cmd[2] = fn_le16(1);
	res = send_cmd(dev, 0x22, cmd, 6, reply, sizeof(reply));
	if (res < 0) {
		FN_WARNING("Could not nudge the stream timing: %d\n", res);
		return res;
	}
	FN_DEBUG("Stream timing: depth nudged by %d us (value %d)\n", depth_nudge_us, v);
	return 0;
}

int freenect_set_stream_timing_auto(freenect_device *dev, int on)
{
	if (on && dev->timing_k2 <= 0)
		return -ENOTSUP;
	dev->timing_auto = on != 0;
	fn_phase_reset(&dev->timing_phase);
	memset(&dev->timing_policy, 0, sizeof(dev->timing_policy));
	dev->timing_seen_depth = dev->depth.delivered;
	dev->timing_seen_video = dev->video.delivered;
	return 0;
}

// Called by freenect_process_events(): feeds the frames delivered since the
// last call to the phase estimate, oldest first, and nudges when it says so.
FN_INTERNAL void fn_timing_process(freenect_device *dev)
{
	freenect_frame_mode vmode;
	int have_d, have_v, rate;
	int32_t nudge = 0;
	int due = 0;

	if (!dev->timing_auto || !dev->depth.running || !dev->video.running || dev->usb_cam.device_dead)
		return;
	have_d = dev->depth.delivered != dev->timing_seen_depth;
	have_v = dev->video.delivered != dev->timing_seen_video;
	if (!have_d && !have_v)
		return;
	dev->timing_seen_depth = dev->depth.delivered;
	dev->timing_seen_video = dev->video.delivered;

	// As Kinect10: only for colour at 30, 15 or 12 frames per second, and
	// not for hi-res colour.
	vmode = freenect_get_current_video_mode(dev);
	rate = vmode.resolution == FREENECT_RESOLUTION_HIGH ? 0 : vmode.framerate;

	{
		int64_t d = (int64_t)dev->depth.mid_us - FN_DEPTH_TIME_OFFSET_US;
		int64_t c = (int64_t)dev->video.mid_us;
		if (have_d && have_v && c < d) {
			due |= fn_phase_update(&dev->timing_phase, 0, c, rate, &nudge);
			due |= fn_phase_update(&dev->timing_phase, 1, d, rate, &nudge);
		} else {
			if (have_d)
				due |= fn_phase_update(&dev->timing_phase, 1, d, rate, &nudge);
			if (have_v)
				due |= fn_phase_update(&dev->timing_phase, 0, c, rate, &nudge);
		}
	}
	if (due) {
		freenect_context *ctx = dev->parent;
		int r = fn_timing_accept(&dev->timing_policy, nudge, fn_now_us());
		if (r > 0)
			freenect_nudge_stream_timing(dev, nudge);
		else if (r < 0)
			FN_INFO("Stream timing: nudges do not change the measured phase (%d us); automatic alignment stopped\n",
			        dev->timing_policy.last_err_us);
		else
			FN_SPEW("Stream timing: phase estimate suggests %d us; not acted on\n", nudge);
	}
}
