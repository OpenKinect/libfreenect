/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * Ryan Gordon <rygordon4@gmail.com>
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

#ifndef LIBFREENECT__LV_H
#define LIBFREENECT_LV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libfreenect.h"
#include "../c_sync/libfreenect_sync.h"

#ifndef _WIN32
#define EXPORT
#else
#define EXPORT extern "C" __declspec(dllexport)
#endif

EXPORT short *freenect_lv_get_depth_frame(int index, freenect_depth_format fmt);
EXPORT char *freenect_lv_get_image_frame(int index, freenect_video_format fmt);
EXPORT int freenect_lv_set_tilt_degs(int angle, int index);
EXPORT freenect_raw_tilt_state *freenect_lv_get_tilt_state(int index);
EXPORT void freenect_lv_get_tilt_state_mks(double *dx, double *dy, double *dz, int index);
EXPORT void freenect_lv_set_led(freenect_led_options led, int index);
EXPORT void freenect_lv_stop();

#ifdef __cplusplus
}
#endif

#endif //