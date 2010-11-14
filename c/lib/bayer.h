/*
 *  bayer.h
 *  xcode-kinect
 *
 *  Created by Juan Carlos del Valle on 11/13/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
	void Bayer2BGR( const uint8_t *bayer0, int bayer_step, uint8_t *dst0, int dst_step, unsigned int width, unsigned int height);
#ifdef __cplusplus
}
#endif