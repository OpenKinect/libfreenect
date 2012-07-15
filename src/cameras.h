/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010-2011 individual OpenKinect contributors. See the CONTRIB
 * file for details.
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

#ifndef CAMERAS_H
#define CAMERAS_H

#include "libfreenect.h"

// Just a couple function declarations.

// These are called by core.c to do camera-specific initialization that needs
// camera-specific protocol support.
int freenect_camera_init(freenect_device *dev);
int freenect_camera_teardown(freenect_device *dev);

/* memcpy in intersection of two intervalls.
 * Length of interval: 			min(E,R) - max(S,L)
 * Index of *src and *dst:	S.
 * Assumptions:							S<E, L<R
 *
 */

static inline void memcpy_intersection(uint8_t *dst, uint8_t *src, int S, int E, int L, int R){
	int m = S>L?S:L;//lower bound index
	int M = E<R?E:R;//upper bound index
	if( M-m> 0 ){
		//uint8_t *dst2 = dst + m;			//dst+S is associated with index S
		//uint8_t *src2 = src + (m - S);//*src is associated index S.
		memcpy(dst+(m-S), src+(m-S), M-m);
		//memset(dst+(m-S), 0, M-m);
	}
};

#endif

