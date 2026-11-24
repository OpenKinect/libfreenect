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

#include "libfreenect.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Coordinate mapper: converts between depth pixels, colour pixels and camera
 * space (metres), with the same results as the Kinect for Windows SDK's
 * INuiCoordinateMapper. It needs only the Kinect's own calibration, read
 * over USB when the device is opened, and the SDK's nominal depth camera
 * intrinsics; no Microsoft data.
 *
 * Images:
 * - Depth: FREENECT_DEPTH_MM values (0 = no data) at 640x480
 *   (FREENECT_RESOLUTION_MEDIUM), 320x240 (LOW) or 80x60 (VERY_LOW).
 * - Colour: 640x480 (MEDIUM) or 1280x960 (HIGH). The hi-res colour frame of
 *   the Xbox 360 Kinect (model 1414) is 1280x1024; HIGH coordinates are in
 *   its top 1280x960.
 *
 * Coordinates are those of the images as libfreenect delivers them, not
 * mirrored. After freenect_mapper_set_mirrored(m, 1) every depth and colour
 * x is that of a horizontally mirrored image (x' = width - 1 - x), as the
 * SDK delivers its images; the numbers are then exactly the SDK's.
 *
 * Camera space is the SDK's skeleton space: metres, centred on the depth
 * camera, x to the left of the Kinect as seen from behind it (to the right
 * in a mirrored image), y up, z away from the Kinect.
 *
 * A mapper is not thread safe; use one per thread.
 */
typedef struct freenect_coordinate_mapper freenect_coordinate_mapper;

/// A depth pixel and its depth in mm (0: none). x and y are
/// FREENECT_MAPPER_INVALID where there is no depth pixel.
typedef struct {
	int32_t x;
	int32_t y;
	uint16_t depth_mm;
} freenect_depth_point;

/// A colour pixel; FREENECT_MAPPER_INVALID in both where there is none. It
/// can lie outside the colour image.
typedef struct {
	int32_t x;
	int32_t y;
} freenect_color_point;

/// A point in camera space, in metres.
typedef struct {
	float x;
	float y;
	float z;
} freenect_camera_point;

#define FREENECT_MAPPER_INVALID INT32_MIN

/// Size of the calibration that freenect_mapper_get_calibration() writes.
#define FREENECT_CALIBRATION_SIZE 152

/// A mapper for the calibration of an open device (read when it was opened).
/// NULL if out of memory or dev is NULL.
FREENECTAPI freenect_coordinate_mapper* freenect_create_coordinate_mapper(freenect_device* dev);

/// A mapper for a calibration saved with freenect_mapper_get_calibration(),
/// for example with a recording. NULL if it is not one, or out of memory.
FREENECTAPI freenect_coordinate_mapper* freenect_create_coordinate_mapper_from_calibration(const void* data, size_t size);

FREENECTAPI void freenect_destroy_coordinate_mapper(freenect_coordinate_mapper* m);

/// Writes the mapper's calibration (FREENECT_CALIBRATION_SIZE bytes, a
/// portable little-endian layout) to buf. Returns the size, or -1 if size is
/// too small.
FREENECTAPI int freenect_mapper_get_calibration(freenect_coordinate_mapper* m, void* buf, size_t size);

/// 1: depth and colour x coordinates are those of mirrored images (as the
/// SDK's); 0 (default): as libfreenect delivers them.
FREENECTAPI void freenect_mapper_set_mirrored(freenect_coordinate_mapper* m, int mirrored);

/*
 * All mapping functions return 0, or -1 for a resolution they do not
 * support or a NULL pointer.
 */

/// The colour pixel that sees depth pixel p (MapDepthPointToColorPoint).
/// FREENECT_MAPPER_INVALID for depth 0.
FREENECTAPI int freenect_map_depth_point_to_color(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_resolution color_res,
	freenect_depth_point p, freenect_color_point* out);

/// The colour pixel of every pixel of a depth frame (MapDepthFrameToColorFrame);
/// out has one entry per depth pixel, FREENECT_MAPPER_INVALID where there
/// is no depth.
FREENECTAPI int freenect_map_depth_frame_to_color(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_resolution color_res,
	const uint16_t* depth_mm, freenect_color_point* out);

/// The depth pixel seen by every pixel of a colour frame
/// (MapColorFrameToDepthFrame): each depth pixel is projected into the colour
/// image, and single gaps of one or two pixels in a row are filled in. out
/// has one entry per colour pixel; FREENECT_MAPPER_INVALID and depth 0 where
/// no depth pixel lands.
FREENECTAPI int freenect_map_color_frame_to_depth(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_resolution color_res,
	const uint16_t* depth_mm, freenect_depth_point* out);

/// Camera space point of a depth pixel (MapDepthPointToSkeletonPoint); all
/// 0 for depth 0.
FREENECTAPI int freenect_map_depth_point_to_camera(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_depth_point p, freenect_camera_point* out);

/// Camera space points of a whole depth frame, one per pixel.
FREENECTAPI int freenect_map_depth_frame_to_camera(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, const uint16_t* depth_mm, freenect_camera_point* out);

/// The depth pixel a camera space point projects to, and its depth in mm
/// (MapSkeletonPointToDepthPoint). FREENECT_MAPPER_INVALID and depth 0 for a
/// point at the origin or behind the camera.
FREENECTAPI int freenect_map_camera_point_to_depth(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_camera_point p, freenect_depth_point* out);

/// The colour pixel that sees a camera space point (MapSkeletonPointToColorPoint).
FREENECTAPI int freenect_map_camera_point_to_color(freenect_coordinate_mapper* m,
	freenect_resolution color_res, freenect_camera_point p, freenect_color_point* out);

/// The camera space point seen by every pixel of a colour frame
/// (MapColorFrameToSkeletonFrame); all 0 where no depth pixel lands.
FREENECTAPI int freenect_map_color_frame_to_camera(freenect_coordinate_mapper* m,
	freenect_resolution depth_res, freenect_resolution color_res,
	const uint16_t* depth_mm, freenect_camera_point* out);

#ifdef __cplusplus
}
#endif
