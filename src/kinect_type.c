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

#include "freenect_internal.h"

static int is_k4w_pid(uint16_t pid)
{
	return pid == PID_K4W_CAMERA || pid == PID_K4W_AUDIO || pid == PID_K4W_AUDIO_ALT_1;
}

/*
 * Decide the Kinect v1 model the way Microsoft's runtime does:
 *
 *   hub 0409:005A (NEC)       -> Xbox 360 model 1414
 *   hub 045E:02C2, and the Kinect function on the hub's lowest port is
 *     02BF, 02BE or 02C3      -> Kinect for Windows
 *     any other Kinect PID    -> Xbox 360 model 1473
 *   hub 045E:02C2 without a Kinect child -> Kinect for Windows
 *
 * first_child_pid is 0 if the hub has no Kinect child (or was not inspected).
 *
 * When the hub is not visible (have_hub == 0, e.g. behind some virtual
 * machine USB passthrough) or is neither of the two, fall back to what
 * libfreenect did before: a 02BF camera is Kinect for Windows, a camera
 * whose bcdDevice is not 0x010B (267) is a 1473, anything else a 1414.
 */
fn_kinect_type fnusb_kinect_type(int have_hub, uint16_t hub_vid, uint16_t hub_pid, uint16_t first_child_pid,
                                 uint16_t camera_pid, uint16_t camera_bcd)
{
	if (have_hub && hub_vid == VID_NEC && hub_pid == PID_NEC_HUB)
		return FN_KINECT_XBOX_1414;
	if (have_hub && hub_vid == VID_MICROSOFT && hub_pid == PID_KINECT_HUB) {
		if (first_child_pid == 0)
			return FN_KINECT_WINDOWS;
		return is_k4w_pid(first_child_pid) ? FN_KINECT_WINDOWS : FN_KINECT_XBOX_1473;
	}
	/* Fallback: no recognisable hub. */
	if (camera_pid == PID_K4W_CAMERA)
		return FN_KINECT_WINDOWS;
	if (camera_pid == PID_NUI_CAMERA)
		return camera_bcd != 267 ? FN_KINECT_XBOX_1473 : FN_KINECT_XBOX_1414;
	return FN_KINECT_UNKNOWN;
}

const char *fnusb_kinect_type_name(fn_kinect_type type)
{
	switch (type) {
	case FN_KINECT_XBOX_1414: return "Xbox 360 Kinect (model 1414)";
	case FN_KINECT_XBOX_1473: return "Xbox 360 Kinect (model 1473)";
	case FN_KINECT_WINDOWS: return "Kinect for Windows (model 1517)";
	default: return "unknown Kinect v1";
	}
}
