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
#pragma once

#include "nui_internal.h"

/* One hand from the 640x480 depth (libfreenect orientation, mm), given its
 * hand and wrist joints and the body's midline (the shoulder centre), all
 * in skeleton space. fingertips[] come out as thumb, index, middle, ring,
 * little, with finger_found[] saying which are there. */
void fn_nui_hand_analyze(const uint16_t* depth, v3 hand, v3 wrist, v3 midline, freenect_nui_hand* out);

/* The two hands together (hand joints within 16 cm): their pixels are one
 * region, analysed as one hand from the joints' midpoints. */
void fn_nui_clasp_analyze(const uint16_t* depth, v3 hand_l, v3 hand_r, v3 wrist_l, v3 wrist_r, v3 midline, freenect_nui_clasp* out);
