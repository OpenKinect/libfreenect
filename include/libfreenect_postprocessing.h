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

#ifndef LIBFREENECT_POSTPROCESSING_H
#define LIBFREENECT_POSTPROCESSING_H

#include <stdint.h>

#include "libfreenect.h"
#include "libfreenect_postprocessing.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FREENECT_MAP_NORMAL  0  /* undistortion map from xyz to uv space */
#define FREENECT_MAP_INVERSE 1  /* inverse */

/*
 * undistortion parameters
 * x'' = x' (1+k1 r**2 + k2 r**4 + k3 r**6) + 2 p1 x'y' + p2(r**2 + 2x'**2)
 * y'' = y' (1+k1 r**2 + k2 r**4 + k3 r**6) + p1 (r**2 + 2 y'**2) + 2p2x'y'
 * where r**2 = x'**2 + y'**2
 */
typedef struct {
	float k1;
	float k2;
	float k3;
	float k4;
	float k5;
	float p1;
	float p2;
} freenect_param_undistort;

/*
 * intrinsic camera parameters
 * fx = focal distance x-axis
 * fy = focal distance y-axis
 * cx = center offset x-axis
 * cy = center offset y-axis
 */
typedef struct {
	float fx;
	float fy;
	float cx;
	float cy;
} freenect_param_intrinsic;

/*
 * extrinsic parameters for depth camera
 * r11 to r33: rotation matrix elements
 * t1 to t3: translation vector elements
 */
typedef struct {
	float r11;
	float r12;
	float r13;
	float r21;
	float r22;
	float r23;
	float r31;
	float r32;
	float r33;
	float t1;
	float t2;
	float t3;
} freenect_param_extrinsic;

typedef struct {
	freenect_param_undistort undistort;
	freenect_param_intrinsic intrinsic;
	freenect_param_extrinsic extrinsic;
} freenect_params;

/// If Win32, export all functions for DLL usage
#ifndef _WIN32
  #define FREENECTAPI /**< DLLExport information for windows, set to nothing on other platforms */
#else
  /**< DLLExport information for windows, set to nothing on other platforms */
  #ifdef __cplusplus
    #define FREENECTAPI extern "C" __declspec(dllexport)
  #else
    // this is required when building from a Win32 port of gcc without being
    // forced to compile all of the library files (.c) with g++...
    #define FREENECTAPI __declspec(dllexport)
  #endif
#endif

/* barrel and tangential undistortion */
FREENECTAPI void freenect_undistort_video(freenect_device *dev, void* _bufin, void* _bufout);
FREENECTAPI void freenect_undistort_depth(freenect_device *dev, void* _bufin, void* _bufout);

/* initialisation of internal matrices */
//void freenect_init_projection_matrix(freenect_device* dev, freenect_param_intrinsic* intr, freenect_param_extrinsic* extr);
FREENECTAPI void freenect_init_undistort_map_video(freenect_device* dev, freenect_param_undistort* dist_coeffs, freenect_param_intrinsic* intr, int inverse);
FREENECTAPI void freenect_init_undistort_map_depth(freenect_device* dev, freenect_param_undistort* dist_coeffs, freenect_param_intrinsic* intr, int inverse);

/* linear transformations */
//void freenect_xyz2uv(float* in, int* out, freenect_projection_matrix* pm);
//void freenect_uv2xyz(int* in, float* out, freenect_projection_matrix* pm);

#ifdef __cplusplus
}
#endif

#endif //

