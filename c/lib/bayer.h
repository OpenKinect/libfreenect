/*
 *  bayer.h
 *  xcode-kinect
 *
 *  Created by Juan Carlos del Valle on 11/13/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include <stdint.h>

#define DC1394_COLOR_FILTER_RGGB 512
#define DC1394_COLOR_FILTER_GBRG 513
#define DC1394_COLOR_FILTER_GRBG 514
#define DC1394_COLOR_FILTER_BGGR 515

#define DC1394_COLOR_FILTER_MIN DC1394_COLOR_FILTER_RGGB
#define DC1394_COLOR_FILTER_MAX DC1394_COLOR_FILTER_BGGR
#define DC1394_COLOR_FILTER_NUM (DC1394_COLOR_FILTER_MAX - DC1394_COLOR_FILTER_MIN + 1)

#define DC1394_INVALID_COLOR_FILTER -26

#define CLIP(in, out)\
in = in < 0 ? 0 : in;\
in = in > 255 ? 255 : in;\
out=in;

#ifdef __cplusplus
extern "C" {
#endif
	int dc1394_bayer_HQLinear(const uint8_t *bayer, uint8_t *rgb, int sx, int sy, int tile);
#ifdef __cplusplus
}
#endif