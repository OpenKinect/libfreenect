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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "freenect_internal.h"
#include "registration.h"
#include "cameras.h"
#include "flags.h"
#include "depth_decimate.h"

static freenect_frame_mode video_mode_for_device(freenect_device *dev, freenect_resolution res, freenect_video_format fmt);

#define MAKE_RESERVED(res, fmt) (uint32_t)(((res & 0xff) << 8) | (((fmt & 0xff))))
#define RESERVED_TO_RESOLUTION(reserved) (freenect_resolution)((reserved >> 8) & 0xff)
#define RESERVED_TO_FORMAT(reserved) ((reserved) & 0xff)

#define video_mode_count 12
static freenect_frame_mode supported_video_modes[video_mode_count] = {
	// reserved, resolution, format, bytes, width, height, data_bits_per_pixel, padding_bits_per_pixel, framerate, is_valid
	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_RGB), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_RGB}, 1280*1024*3, 1280, 1024, 24, 0, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_RGB}, 640*480*3, 640,  480, 24, 0, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_BAYER), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_BAYER}, 1280*1024, 1280, 1024, 8, 0, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_BAYER), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_BAYER}, 640*480, 640, 480, 8, 0, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_IR_8BIT), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_IR_8BIT}, 1280*1024, 1280, 1024, 8, 0, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_8BIT), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_IR_8BIT}, 640*488, 640, 488, 8, 0, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_IR_10BIT), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_IR_10BIT}, 1280*1024*2, 1280, 1024, 10, 6, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_10BIT), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_IR_10BIT}, 640*488*2, 640, 488, 10, 6, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_HIGH,   FREENECT_VIDEO_IR_10BIT_PACKED), FREENECT_RESOLUTION_HIGH, {FREENECT_VIDEO_IR_10BIT_PACKED}, 1280*1024*10/8, 1280, 1024, 10, 0, 10, 1 },
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_IR_10BIT_PACKED), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_IR_10BIT_PACKED}, 640*488*10/8, 640, 488, 10, 0, 30, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_YUV_RGB), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_YUV_RGB}, 640*480*3, 640, 480, 24, 0, 15, 1 },

	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_YUV_RAW), FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_YUV_RAW}, 640*480*2, 640, 480, 16, 0, 15, 1 },
};

#define depth_mode_count 14
static freenect_frame_mode supported_depth_modes[depth_mode_count] = {
	// reserved, resolution, format, bytes, width, height, data_bits_per_pixel, padding_bits_per_pixel, framerate, is_valid
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_11BIT}, 640*480*2, 640, 480, 11, 5, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_10BIT}, 640*480*2, 640, 480, 10, 6, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT_PACKED), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_11BIT_PACKED}, 640*480*11/8, 640, 480, 11, 0, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT_PACKED), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_10BIT_PACKED}, 640*480*10/8, 640, 480, 10, 0, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_REGISTERED), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_REGISTERED}, 640*480*2, 640, 480, 16, 0, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM), FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_MM}, 640*480*2, 640, 480, 16, 0, 30, 1},
	// 320x240 and 80x60: taken from the 640x480 frame in software (§20.2.4)
	{MAKE_RESERVED(FREENECT_RESOLUTION_LOW, FREENECT_DEPTH_11BIT), FREENECT_RESOLUTION_LOW, {FREENECT_DEPTH_11BIT}, 320*240*2, 320, 240, 11, 5, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_LOW, FREENECT_DEPTH_10BIT), FREENECT_RESOLUTION_LOW, {FREENECT_DEPTH_10BIT}, 320*240*2, 320, 240, 10, 6, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_LOW, FREENECT_DEPTH_REGISTERED), FREENECT_RESOLUTION_LOW, {FREENECT_DEPTH_REGISTERED}, 320*240*2, 320, 240, 16, 0, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_LOW, FREENECT_DEPTH_MM), FREENECT_RESOLUTION_LOW, {FREENECT_DEPTH_MM}, 320*240*2, 320, 240, 16, 0, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_VERY_LOW, FREENECT_DEPTH_11BIT), FREENECT_RESOLUTION_VERY_LOW, {FREENECT_DEPTH_11BIT}, 80*60*2, 80, 60, 11, 5, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_VERY_LOW, FREENECT_DEPTH_10BIT), FREENECT_RESOLUTION_VERY_LOW, {FREENECT_DEPTH_10BIT}, 80*60*2, 80, 60, 10, 6, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_VERY_LOW, FREENECT_DEPTH_REGISTERED), FREENECT_RESOLUTION_VERY_LOW, {FREENECT_DEPTH_REGISTERED}, 80*60*2, 80, 60, 16, 0, 30, 1},
	{MAKE_RESERVED(FREENECT_RESOLUTION_VERY_LOW, FREENECT_DEPTH_MM), FREENECT_RESOLUTION_VERY_LOW, {FREENECT_DEPTH_MM}, 80*60*2, 80, 60, 16, 0, 30, 1},
};

/**
 * Convert a packed array of n elements with vw useful bits into array of
 * zero-padded 16bit elements.
 *
 * @param src The source packed array, of size (n * vw / 8) bytes
 * @param dest The destination unpacked array, of size (n * 2) bytes
 * @param vw The virtual width of elements, that is the number of useful bits for each of them
 * @param n The number of elements (in particular, of the destination array), NOT a length in bytes
 */
static inline void convert_packed_to_16bit(uint8_t *src, uint16_t *dest, int vw, int n)
{
	unsigned int mask = (1 << vw) - 1;
	uint32_t buffer = 0;
	int bitsIn = 0;
	while (n--) {
		while (bitsIn < vw) {
			buffer = (buffer << 8) | *(src++);
			bitsIn += 8;
		}
		bitsIn -= vw;
		*(dest++) = (buffer >> bitsIn) & mask;
	}
}

/**
 * Convert a packed array of n elements with vw useful bits into array of
 * 8bit elements, dropping LSB.
 *
 * @param src The source packed array, of size (n * vw / 8) bytes
 * @param dest The destination unpacked array, of size (n * 2) bytes
 * @param vw The virtual width of elements, that is the number of useful bits for each of them
 * @param n The number of elements (in particular, of the destination array), NOT a length in bytes
 *
 * @pre vw is expected to be >= 8.
 */
static inline void convert_packed_to_8bit(uint8_t *src, uint8_t *dest, int vw, int n)
{
	uint32_t buffer = 0;
	int bitsIn = 0;
	while (n--) {
		while (bitsIn < vw) {
			buffer = (buffer << 8) | *(src++);
			bitsIn += 8;
		}
		bitsIn -= vw;
		*(dest++) = buffer >> (bitsIn + vw - 8);
	}
}

// Loop-unrolled version of the 11-to-16 bit unpacker.  n must be a multiple of 8.
static void convert_packed11_to_16bit(uint8_t *raw, uint16_t *frame, int n)
{
	uint16_t baseMask = (1 << 11) - 1;
	while(n >= 8)
	{
		uint8_t r0  = *(raw+0);
		uint8_t r1  = *(raw+1);
		uint8_t r2  = *(raw+2);
		uint8_t r3  = *(raw+3);
		uint8_t r4  = *(raw+4);
		uint8_t r5  = *(raw+5);
		uint8_t r6  = *(raw+6);
		uint8_t r7  = *(raw+7);
		uint8_t r8  = *(raw+8);
		uint8_t r9  = *(raw+9);
		uint8_t r10 = *(raw+10);

		frame[0] =  (r0<<3)  | (r1>>5);
		frame[1] = ((r1<<6)  | (r2>>2) )           & baseMask;
		frame[2] = ((r2<<9)  | (r3<<1) | (r4>>7) ) & baseMask;
		frame[3] = ((r4<<4)  | (r5>>4) )           & baseMask;
		frame[4] = ((r5<<7)  | (r6>>1) )           & baseMask;
		frame[5] = ((r6<<10) | (r7<<2) | (r8>>6) ) & baseMask;
		frame[6] = ((r8<<5)  | (r9>>3) )           & baseMask;
		frame[7] = ((r9<<8)  | (r10)   )           & baseMask;

		n -= 8;
		raw += 11;
		frame += 8;
	}
}

static void depth_process(freenect_device *dev, uint8_t *pkt, int len)
{
	freenect_context *ctx = dev->parent;

	if (len == 0)
		return;

	if (!dev->depth.running)
		return;

	int got_frame_size = stream_process(ctx, &dev->depth, pkt, len,dev->depth_chunk_cb,dev->user_data);

	if (!got_frame_size)
		return;

	FN_SPEW("Got depth frame of size %d/%d, %d/%d packets arrived, TS %08x\n", got_frame_size,
	        dev->depth.frame_size, dev->depth.valid_pkts, dev->depth.pkts_per_frame, dev->depth.timestamp);

	// 320x240 and 80x60 are converted at 640x480 into depth_full first.
	uint16_t *full = dev->depth_full ? dev->depth_full : (uint16_t*)dev->depth.proc_buf;
	switch (dev->depth_format) {
		case FREENECT_DEPTH_11BIT:
			convert_packed11_to_16bit(dev->depth.raw_buf, full, 640*480);
			break;
		case FREENECT_DEPTH_REGISTERED:
			freenect_apply_registration(dev, dev->depth.raw_buf, full, false);
			break;
		case FREENECT_DEPTH_MM:
			freenect_apply_depth_to_mm(dev, dev->depth.raw_buf, full);
			break;
		case FREENECT_DEPTH_10BIT:
			convert_packed_to_16bit(dev->depth.raw_buf, full, 10, 640*480);
			break;
		case FREENECT_DEPTH_10BIT_PACKED:
		case FREENECT_DEPTH_11BIT_PACKED:
			break;
		default:
			FN_ERROR("depth_process() was called, but an invalid depth_format is set\n");
			break;
	}
	if (dev->depth_full)
		fn_decimate_depth(dev->depth_full, (uint16_t*)dev->depth.proc_buf,
		                  dev->depth_resolution == FREENECT_RESOLUTION_VERY_LOW ? 8 : 2, dev->depth_mirrored);
	if (dev->depth_cb)
		dev->depth_cb(dev, dev->depth.proc_buf, dev->depth.timestamp);
}

#define CLAMP(x) if (x < 0) {x = 0;} if (x > 255) {x = 255;}
static void convert_uyvy_to_rgb(uint8_t *raw_buf, uint8_t *proc_buf, freenect_frame_mode frame_mode)
{
	int x, y;
	for(y = 0; y < frame_mode.height; ++y) {
		for(x = 0; x < frame_mode.width; x+=2) {
			int i = (frame_mode.width * y + x);
			int u  = raw_buf[2*i];
			int y1 = raw_buf[2*i+1];
			int v  = raw_buf[2*i+2];
			int y2 = raw_buf[2*i+3];
			int r1 = (y1-16)*1164/1000 + (v-128)*1596/1000;
			int g1 = (y1-16)*1164/1000 - (v-128)*813/1000 - (u-128)*391/1000;
			int b1 = (y1-16)*1164/1000 + (u-128)*2018/1000;
			int r2 = (y2-16)*1164/1000 + (v-128)*1596/1000;
			int g2 = (y2-16)*1164/1000 - (v-128)*813/1000 - (u-128)*391/1000;
			int b2 = (y2-16)*1164/1000 + (u-128)*2018/1000;
			CLAMP(r1)
			CLAMP(g1)
			CLAMP(b1)
			CLAMP(r2)
			CLAMP(g2)
			CLAMP(b2)
			proc_buf[3*i]  =r1;
			proc_buf[3*i+1]=g1;
			proc_buf[3*i+2]=b1;
			proc_buf[3*i+3]=r2;
			proc_buf[3*i+4]=g2;
			proc_buf[3*i+5]=b2;
		}
	}
}
#undef CLAMP

static void convert_bayer_to_rgb(uint8_t *raw_buf, uint8_t *proc_buf, freenect_frame_mode frame_mode)
{
	int x,y;
	/* Pixel arrangement:
	 * G R G R G R G R
	 * B G B G B G B G
	 * G R G R G R G R
	 * B G B G B G B G
	 * G R G R G R G R
	 * B G B G B G B G
	 *
	 * To convert a Bayer-pattern into RGB you have to handle four pattern
	 * configurations:
	 * 1)         2)         3)         4)
	 *      B1      B1 G1 B2   R1 G1 R2      R1       <- previous line
	 *   R1 G1 R2   G2 R1 G3   G2 B1 G3   B1 G1 B2    <- current line
	 *      B2      B3 G4 B4   R3 G4 R4      R2       <- next line
	 *   ^  ^  ^
	 *   |  |  next pixel
	 *   |  current pixel
	 *   previous pixel
	 *
	 * The RGB values (r,g,b) for each configuration are calculated as
	 * follows:
	 *
	 * 1) r = (R1 + R2) / 2
	 *    g =  G1
	 *    b = (B1 + B2) / 2
	 *
	 * 2) r =  R1
	 *    g = (G1 + G2 + G3 + G4) / 4
	 *    b = (B1 + B2 + B3 + B4) / 4
	 *
	 * 3) r = (R1 + R2 + R3 + R4) / 4
	 *    g = (G1 + G2 + G3 + G4) / 4
	 *    b =  B1
	 *
	 * 4) r = (R1 + R2) / 2
	 *    g =  G1
	 *    b = (B1 + B2) / 2
	 *
	 * To efficiently calculate these values, two 32bit integers are used
	 * as "shift-buffers". One integer to store the 3 horizontal bayer pixel
	 * values (previous, current, next) of the current line. The other
	 * integer to store the vertical average value of the bayer pixels
	 * (previous, current, next) of the previous and next line.
	 *
	 * The boundary conditions for the first and last line and the first
	 * and last column are solved via mirroring the second and second last
	 * line and the second and second last column.
	 *
	 * To reduce slow memory access, the values of a rgb pixel are packet
	 * into a 32bit variable and transfered together.
	 */

	uint8_t *dst = proc_buf; // pointer to destination

	uint8_t *prevLine;        // pointer to previous, current and next line
	uint8_t *curLine;         // of the source bayer pattern
	uint8_t *nextLine;

	// storing horizontal values in hVals:
	// previous << 16, current << 8, next
	uint32_t hVals;
	// storing vertical averages in vSums:
	// previous << 16, current << 8, next
	uint32_t vSums;

	// init curLine and nextLine pointers
	curLine  = raw_buf;
	nextLine = curLine + frame_mode.width;
	for (y = 0; y < frame_mode.height; ++y) {

		if ((y > 0) && (y < frame_mode.height-1))
			prevLine = curLine - frame_mode.width; // normal case
		else if (y == 0)
			prevLine = nextLine;      // top boundary case
		else
			nextLine = prevLine;      // bottom boundary case

		// init horizontal shift-buffer with current value
		hVals  = (*(curLine++) << 8);
		// handle left column boundary case
		hVals |= (*curLine << 16);
		// init vertical average shift-buffer with current values average
		vSums = ((*(prevLine++) + *(nextLine++)) << 7) & 0xFF00;
		// handle left column boundary case
		vSums |= ((*prevLine + *nextLine) << 15) & 0xFF0000;

		// store if line is odd or not
		uint8_t yOdd = y & 1;
		// the right column boundary case is not handled inside this loop
		// thus the "639"
		for (x = 0; x < frame_mode.width-1; ++x) {
			// place next value in shift buffers
			hVals |= *(curLine++);
			vSums |= (*(prevLine++) + *(nextLine++)) >> 1;

			// calculate the horizontal sum as this sum is needed in
			// any configuration
			uint8_t hSum = ((uint8_t)(hVals >> 16) + (uint8_t)(hVals)) >> 1;

			if (yOdd == 0) {
				if ((x & 1) == 0) {
					// Configuration 1
					*(dst++) = hSum;		// r
					*(dst++) = hVals >> 8;	// g
					*(dst++) = vSums >> 8;	// b
				} else {
					// Configuration 2
					*(dst++) = hVals >> 8;
					*(dst++) = (hSum + (uint8_t)(vSums >> 8)) >> 1;
					*(dst++) = ((uint8_t)(vSums >> 16) + (uint8_t)(vSums)) >> 1;
				}
			} else {
				if ((x & 1) == 0) {
					// Configuration 3
					*(dst++) = ((uint8_t)(vSums >> 16) + (uint8_t)(vSums)) >> 1;
					*(dst++) = (hSum + (uint8_t)(vSums >> 8)) >> 1;
					*(dst++) = hVals >> 8;
				} else {
					// Configuration 4
					*(dst++) = vSums >> 8;
					*(dst++) = hVals >> 8;
					*(dst++) = hSum;
				}
			}

			// shift the shift-buffers
			hVals <<= 8;
			vSums <<= 8;
		} // end of for x loop
		// right column boundary case, mirroring second last column
		hVals |= (uint8_t)(hVals >> 16);
		vSums |= (uint8_t)(vSums >> 16);

		// the horizontal sum simplifies to the second last column value
		uint8_t hSum = (uint8_t)(hVals);

		if (yOdd == 0) {
			if ((x & 1) == 0) {
				*(dst++) = hSum;
				*(dst++) = hVals >> 8;
				*(dst++) = vSums >> 8;
			} else {
				*(dst++) = hVals >> 8;
				*(dst++) = (hSum + (uint8_t)(vSums >> 8)) >> 1;
				*(dst++) = vSums;
			}
		} else {
			if ((x & 1) == 0) {
				*(dst++) = vSums;
				*(dst++) = (hSum + (uint8_t)(vSums >> 8)) >> 1;
				*(dst++) = hVals >> 8;
			} else {
				*(dst++) = vSums >> 8;
				*(dst++) = hVals >> 8;
				*(dst++) = hSum;
			}
		}

	} // end of for y loop
}

static void video_process(freenect_device *dev, uint8_t *pkt, int len)
{
	freenect_context *ctx = dev->parent;

	if (len == 0)
		return;

	if (!dev->video.running)
		return;

	int got_frame_size = stream_process(ctx, &dev->video, pkt, len,dev->video_chunk_cb,dev->user_data);

	if (!got_frame_size)
		return;

	FN_SPEW("Got video frame of size %d/%d, %d/%d packets arrived, TS %08x\n", got_frame_size,
	        dev->video.frame_size, dev->video.valid_pkts, dev->video.pkts_per_frame, dev->video.timestamp);

	freenect_frame_mode frame_mode = freenect_get_current_video_mode(dev);
	switch (dev->video_format) {
		case FREENECT_VIDEO_RGB:
			convert_bayer_to_rgb(dev->video.raw_buf, (uint8_t*)dev->video.proc_buf, frame_mode);
			break;
		case FREENECT_VIDEO_BAYER:
			break;
		case FREENECT_VIDEO_IR_10BIT:
			convert_packed_to_16bit(dev->video.raw_buf, (uint16_t*)dev->video.proc_buf, 10, frame_mode.width * frame_mode.height);
			break;
		case FREENECT_VIDEO_IR_10BIT_PACKED:
			break;
		case FREENECT_VIDEO_IR_8BIT:
			convert_packed_to_8bit(dev->video.raw_buf, (uint8_t*)dev->video.proc_buf, 10, frame_mode.width * frame_mode.height);
			break;
		case FREENECT_VIDEO_YUV_RGB:
			convert_uyvy_to_rgb(dev->video.raw_buf, (uint8_t*)dev->video.proc_buf, frame_mode);
			break;
		case FREENECT_VIDEO_YUV_RAW:
			break;
		default:
			FN_ERROR("video_process() was called, but an invalid video_format is set\n");
			break;
	}

	if (dev->video_cb)
		dev->video_cb(dev, dev->video.proc_buf, dev->video.timestamp);
}

static int freenect_fetch_reg_info(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	char reply[0x200];
	uint16_t cmd[5];
	// Read once at open, for 640x480 at 30 fps, as Microsoft's init script
	// does ([0x40, 0, 1, 30, 0], §20.1.1): the mapping for every colour
	// resolution uses these values (§20.5.2), and asking with the hi-res
	// colour mode fails.
	cmd[0] = fn_le16(0x40); // ParamID - in this scenario, XN_HOST_PROTOCOL_ALGORITHM_REGISTRATION
	cmd[1] = fn_le16(0); // Format
	cmd[2] = fn_le16(1); // Resolution
	cmd[3] = fn_le16(30); // FPS
	cmd[4] = fn_le16(0); // Offset

	int res;
	res = send_cmd(dev, 0x16, cmd, 10, reply, 118); // OPCODE_ALGORITHM_PARAMS
	if(res != 118) {
		FN_ERROR("freenect_fetch_reg_info: send_cmd read %d bytes (expected 118)\n", res);
		return -1;
	}
	memcpy(&dev->registration.reg_info, reply + 2, sizeof(dev->registration.reg_info));
	dev->registration.reg_info.ax            = fn_le32s(dev->registration.reg_info.ax);
	dev->registration.reg_info.bx            = fn_le32s(dev->registration.reg_info.bx);
	dev->registration.reg_info.cx            = fn_le32s(dev->registration.reg_info.cx);
	dev->registration.reg_info.dx            = fn_le32s(dev->registration.reg_info.dx);
	dev->registration.reg_info.ay            = fn_le32s(dev->registration.reg_info.ay);
	dev->registration.reg_info.by            = fn_le32s(dev->registration.reg_info.by);
	dev->registration.reg_info.cy            = fn_le32s(dev->registration.reg_info.cy);
	dev->registration.reg_info.dy            = fn_le32s(dev->registration.reg_info.dy);
	dev->registration.reg_info.dx_start      = fn_le32s(dev->registration.reg_info.dx_start);
	dev->registration.reg_info.dy_start      = fn_le32s(dev->registration.reg_info.dy_start);
	dev->registration.reg_info.dx_beta_start = fn_le32s(dev->registration.reg_info.dx_beta_start);
	dev->registration.reg_info.dy_beta_start = fn_le32s(dev->registration.reg_info.dy_beta_start);
	dev->registration.reg_info.dx_beta_inc   = fn_le32s(dev->registration.reg_info.dx_beta_inc);
	dev->registration.reg_info.dy_beta_inc   = fn_le32s(dev->registration.reg_info.dy_beta_inc);
	dev->registration.reg_info.dxdx_start    = fn_le32s(dev->registration.reg_info.dxdx_start);
	dev->registration.reg_info.dxdy_start    = fn_le32s(dev->registration.reg_info.dxdy_start);
	dev->registration.reg_info.dydx_start    = fn_le32s(dev->registration.reg_info.dydx_start);
	dev->registration.reg_info.dydy_start    = fn_le32s(dev->registration.reg_info.dydy_start);
	dev->registration.reg_info.dxdxdx_start  = fn_le32s(dev->registration.reg_info.dxdxdx_start);
	dev->registration.reg_info.dydxdx_start  = fn_le32s(dev->registration.reg_info.dydxdx_start);
	dev->registration.reg_info.dxdxdy_start  = fn_le32s(dev->registration.reg_info.dxdxdy_start);
	dev->registration.reg_info.dydxdy_start  = fn_le32s(dev->registration.reg_info.dydxdy_start);
	dev->registration.reg_info.dydydx_start  = fn_le32s(dev->registration.reg_info.dydydx_start);
	dev->registration.reg_info.dydydy_start  = fn_le32s(dev->registration.reg_info.dydydy_start);
	FN_SPEW("ax:                %d\n", dev->registration.reg_info.ax);
	FN_SPEW("bx:                %d\n", dev->registration.reg_info.bx);
	FN_SPEW("cx:                %d\n", dev->registration.reg_info.cx);
	FN_SPEW("dx:                %d\n", dev->registration.reg_info.dx);
	FN_SPEW("ay:                %d\n", dev->registration.reg_info.ay);
	FN_SPEW("by:                %d\n", dev->registration.reg_info.by);
	FN_SPEW("cy:                %d\n", dev->registration.reg_info.cy);
	FN_SPEW("dy:                %d\n", dev->registration.reg_info.dy);
	FN_SPEW("dx_start:          %d\n", dev->registration.reg_info.dx_start);
	FN_SPEW("dy_start:          %d\n", dev->registration.reg_info.dy_start);
	FN_SPEW("dx_beta_start:     %d\n", dev->registration.reg_info.dx_beta_start);
	FN_SPEW("dy_beta_start:     %d\n", dev->registration.reg_info.dy_beta_start);
	FN_SPEW("dx_beta_inc:       %d\n", dev->registration.reg_info.dx_beta_inc);
	FN_SPEW("dy_beta_inc:       %d\n", dev->registration.reg_info.dy_beta_inc);
	FN_SPEW("dxdx_start:        %d\n", dev->registration.reg_info.dxdx_start);
	FN_SPEW("dxdy_start:        %d\n", dev->registration.reg_info.dxdy_start);
	FN_SPEW("dydx_start:        %d\n", dev->registration.reg_info.dydx_start);
	FN_SPEW("dydy_start:        %d\n", dev->registration.reg_info.dydy_start);
	FN_SPEW("dxdxdx_start:      %d\n", dev->registration.reg_info.dxdxdx_start);
	FN_SPEW("dydxdx_start:      %d\n", dev->registration.reg_info.dydxdx_start);
	FN_SPEW("dxdxdy_start:      %d\n", dev->registration.reg_info.dxdxdy_start);
	FN_SPEW("dydxdy_start:      %d\n", dev->registration.reg_info.dydxdy_start);
	FN_SPEW("dydydx_start:      %d\n", dev->registration.reg_info.dydydx_start);
	FN_SPEW("dydydy_start:      %d\n", dev->registration.reg_info.dydydy_start);
	/*
	// NOTE: Not assigned above
	FN_SPEW("dx_center:         %d\n", dev_reg_info->dx_center);
	FN_SPEW("rollout_blank:     %d\n", dev_reg_info->rollout_blank);
	FN_SPEW("rollout_size:      %d\n", dev_reg_info->rollout_size);
	FN_SPEW("back_comp1:            %d\n", dev_reg_info->back_comp1);
	FN_SPEW("back_comp2:            %d\n", dev_reg_info->back_comp2);
	*/
	return 0;
}

static int freenect_fetch_reg_pad_info(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	char reply[0x200];
	uint16_t cmd[5];
	freenect_frame_mode mode = freenect_get_current_video_mode(dev);
	cmd[0] = fn_le16(0x41); // ParamID
	cmd[1] = fn_le16(0); // Format
	cmd[2] = fn_le16((uint16_t)mode.resolution); // Resolution
	cmd[3] = fn_le16((uint16_t)mode.framerate); // FPS
	cmd[4] = fn_le16(0); // Offset
	int res;
	res = send_cmd(dev, 0x16, cmd, 10, reply, 8); // OPCODE_ALGORITHM_PARAMS
	if(res != 8) {
		FN_ERROR("freenect_fetch_reg_pad_info: send_cmd read %d bytes (expected 8)\n", res);
		return -1;
	}
	memcpy(&dev->registration.reg_pad_info, reply+2, sizeof(dev->registration.reg_pad_info));
	dev->registration.reg_pad_info.start_lines    = fn_le16s(dev->registration.reg_pad_info.start_lines);
	dev->registration.reg_pad_info.end_lines      = fn_le16s(dev->registration.reg_pad_info.end_lines);
	dev->registration.reg_pad_info.cropping_lines = fn_le16s(dev->registration.reg_pad_info.cropping_lines);
	FN_SPEW("start_lines:    %u\n",dev->registration.reg_pad_info.start_lines);
	FN_SPEW("end_lines:      %u\n",dev->registration.reg_pad_info.end_lines);
	FN_SPEW("cropping_lines: %u\n",dev->registration.reg_pad_info.cropping_lines);
	return 0;
}

static int freenect_fetch_reg_const_shift(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	char reply[0x200];
	uint16_t cmd[5];
	freenect_frame_mode mode = freenect_get_current_video_mode(dev);
	cmd[0] = fn_le16(0x00); // ParamID
	cmd[1] = fn_le16(0); // Format
	cmd[2] = fn_le16((uint16_t)mode.resolution); // Resolution
	cmd[3] = fn_le16((uint16_t)mode.framerate); // FPS
	cmd[4] = fn_le16(0); // Offset
	int res;
	res = send_cmd(dev, 0x16, cmd, 10, reply, 4); // OPCODE_ALGORITHM_PARAMS
	if(res != 4) {
		FN_ERROR("freenect_fetch_reg_const_shift: send_cmd read %d bytes (expected 8)\n", res);
		return -1;
	}
	uint16_t shift;
	memcpy(&shift, reply+2, sizeof(shift));
	shift = fn_le16(shift);
	dev->registration.const_shift = (double)shift;
	FN_SPEW("const_shift: %f\n",dev->registration.const_shift);
	return 0;
}

static int freenect_fetch_zero_plane_info(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;

	char reply[0x200];
	uint16_t cmd[5] = {0}; // Offset is the only field in this command, and it's 0

	int res;
	res = send_cmd(dev, 0x04, cmd, 10, reply, ctx->zero_plane_res); //OPCODE_GET_FIXED_PARAMS = 4,
	if (res != ctx->zero_plane_res) {
		FN_ERROR("freenect_fetch_zero_plane_info: send_cmd read %d bytes (expected %d)\n", res,ctx->zero_plane_res);
		return -1;
	}

	memcpy(&(dev->registration.zero_plane_info), reply + 94, sizeof(dev->registration.zero_plane_info));
	union {
		uint32_t ui;
		float f;
	} conversion_union;
	conversion_union.f = dev->registration.zero_plane_info.dcmos_emitter_dist;
	conversion_union.ui = fn_le32(conversion_union.ui);
	dev->registration.zero_plane_info.dcmos_emitter_dist = conversion_union.f;

	conversion_union.f = dev->registration.zero_plane_info.dcmos_rcmos_dist;
	conversion_union.ui = fn_le32(conversion_union.ui);
	dev->registration.zero_plane_info.dcmos_rcmos_dist = conversion_union.f;

	conversion_union.f = dev->registration.zero_plane_info.reference_distance;
	conversion_union.ui = fn_le32(conversion_union.ui);
	dev->registration.zero_plane_info.reference_distance = conversion_union.f;

	conversion_union.f = dev->registration.zero_plane_info.reference_pixel_size;
	conversion_union.ui = fn_le32(conversion_union.ui);
	dev->registration.zero_plane_info.reference_pixel_size = conversion_union.f;

	// WTF is all this data?  it's way bigger than sizeof(XnFixedParams)...
	FN_SPEW("dcmos_emitter_distance: %f\n", dev->registration.zero_plane_info.dcmos_emitter_dist);
	FN_SPEW("dcmos_rcmos_distance:   %f\n", dev->registration.zero_plane_info.dcmos_rcmos_dist);
	FN_SPEW("reference_distance:     %f\n", dev->registration.zero_plane_info.reference_distance);
	FN_SPEW("reference_pixel_size:   %f\n", dev->registration.zero_plane_info.reference_pixel_size);

	// FIXME: OpenNI seems to use a hardcoded value of 2.4 instead of 2.3 as reported by Kinect
	dev->registration.zero_plane_info.dcmos_rcmos_dist = 2.4f;

	return 0;
}

int freenect_start_depth(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;

	if (dev->depth.running)
		return -1;

	dev->depth.pkt_size = DEPTH_PKTDSIZE;
	dev->depth.flag = 0x70;
	dev->depth.variable_length = 0;

	// The camera always sends 640x480; 320x240 and 80x60 are taken from it
	// after conversion (§20.2.4), so they need a 640x480 work buffer.
	freenect_frame_mode out_mode = freenect_find_depth_mode(dev->depth_resolution, dev->depth_format);
	if (!out_mode.is_valid) {
		FN_ERROR("freenect_start_depth(): invalid depth format/resolution combination\n");
		return -1;
	}
	free(dev->depth_full);
	dev->depth_full = NULL;
	if (dev->depth_resolution != FREENECT_RESOLUTION_MEDIUM) {
		dev->depth_full = (uint16_t*)malloc(640*480*sizeof(uint16_t));
		if (!dev->depth_full)
			return -1;
	}

	switch (dev->depth_format) {
		case FREENECT_DEPTH_REGISTERED:
		case FREENECT_DEPTH_MM:
			if (freenect_init_registration(dev) < 0) {
				FN_ERROR("freenect_start_depth(): out of memory for the registration tables\n");
				return -1;
			}
			// fall through - these are computed from the 11-bit stream
		case FREENECT_DEPTH_11BIT:
			stream_init(ctx, &dev->depth, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT_PACKED).bytes, out_mode.bytes);
			break;
		case FREENECT_DEPTH_10BIT:
			stream_init(ctx, &dev->depth, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_10BIT_PACKED).bytes, out_mode.bytes);
			break;
		case FREENECT_DEPTH_11BIT_PACKED:
		case FREENECT_DEPTH_10BIT_PACKED:
			stream_init(ctx, &dev->depth, 0, freenect_find_depth_mode(dev->depth_resolution, dev->depth_format).bytes);
			break;
		default:
			FN_ERROR("freenect_start_depth() called with invalid depth format %d\n", dev->depth_format);
			return -1;
	}
	dev->depth.skip_frames = 0;

	const unsigned char depth_endpoint = 0x82;
	int packet_size = fnusb_get_max_iso_packet_size(&dev->usb_cam, depth_endpoint, DEPTH_PKTBUF);

	FN_INFO("[Stream 70] Negotiated packet size %d\n", packet_size);

	int res = fnusb_start_iso(&dev->usb_cam, &dev->depth_isoc, depth_process, depth_endpoint, NUM_XFERS, PKTS_PER_XFER, packet_size);
	if (res < 0)
		return res;

	write_register(dev, 0x105, 0x00); // Disable auto-cycle of projector
	write_register(dev, 0x06, 0x00); // reset depth stream
	switch (dev->depth_format) {
		case FREENECT_DEPTH_11BIT:
		case FREENECT_DEPTH_11BIT_PACKED:
		case FREENECT_DEPTH_REGISTERED:
		case FREENECT_DEPTH_MM:
			write_register(dev, 0x12, 0x03);
			break;
		case FREENECT_DEPTH_10BIT:
		case FREENECT_DEPTH_10BIT_PACKED:
			write_register(dev, 0x12, 0x02);
			break;
		case FREENECT_DEPTH_DUMMY: // Returned already, hush gcc
			break;
	}
	write_register(dev, 0x13, 0x01);
	write_register(dev, 0x14, 0x1e);
	write_register(dev, 0x06, 0x02); // start depth stream
	write_register(dev, 0x17, 0x00); // disable depth hflip
	dev->depth_mirrored = 0;

	// IR projector: on, as Microsoft's runtime does after every depth start,
	// unless freenect_set_ir_emitter(dev, 0) asked to keep it off.
	fn_set_ir_emitter(dev, !dev->ir_emitter_forced_off);

	dev->depth.running = 1;
	return 0;
}

int freenect_start_video(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;

	if (dev->video.running)
		return -1;

	dev->video.pkt_size = VIDEO_PKTDSIZE;
	dev->video.flag = 0x80;
	dev->video.variable_length = 0;

	uint16_t mode_reg, mode_value;
	uint16_t res_reg, res_value;
	uint16_t fps_reg, fps_value;
	uint16_t hflip_reg;

	switch(dev->video_format) {
		case FREENECT_VIDEO_RGB:
		case FREENECT_VIDEO_BAYER:
			if(dev->video_resolution == FREENECT_RESOLUTION_HIGH) {
				mode_value = 0x00; // Bayer
				res_value = 0x02; // 1280x1024
				fps_value = 0x0f; // "15" Hz
			} else if (dev->video_resolution == FREENECT_RESOLUTION_MEDIUM) {
				mode_value = 0x00; // Bayer
				res_value = 0x01; // 640x480
				fps_value = 0x1e; // 30 Hz
			} else {
				FN_ERROR("freenect_start_video(): called with invalid format/resolution combination\n");
				return -1;
			}
			mode_reg = 0x0c;
			res_reg = 0x0d;
			fps_reg = 0x0e;
			hflip_reg = 0x47;
			break;
		case FREENECT_VIDEO_IR_8BIT:
		case FREENECT_VIDEO_IR_10BIT:
		case FREENECT_VIDEO_IR_10BIT_PACKED:
			if(dev->video_resolution == FREENECT_RESOLUTION_HIGH) {
				if(dev->depth.running) {
					FN_ERROR("freenect_start_video(): cannot stream high-resolution IR at same time as depth stream\n");
					return -1;
				}

				// Due to some ridiculous condition in the firmware, we have to start and stop the
				// depth stream before the camera will hand us 1280x1024 IR.  This is a stupid
				// workaround, but we've yet to find a better solution.
				write_register(dev, 0x13, 0x01); // set depth camera resolution (640x480)
				write_register(dev, 0x14, 0x1e); // set depth camera FPS (30)
				write_register(dev, 0x06, 0x02); // start depth camera
				write_register(dev, 0x06, 0x00); // stop depth camera

				mode_value = 0x00; // Luminance, 10-bit packed
				res_value = 0x02; // 1280x1024
				fps_value = 0x0f; // "15" Hz
			} else if (dev->video_resolution == FREENECT_RESOLUTION_MEDIUM) {
				mode_value = 0x00; // Luminance, 10-bit packed
				res_value = 0x01; // 640x480
				fps_value = 0x1e; // 30 Hz
			} else {
				FN_ERROR("freenect_start_video(): called with invalid format/resolution combination\n");
				return -1;
			}
			mode_reg = 0x19;
			res_reg = 0x1a;
			fps_reg = 0x1b;
			hflip_reg = 0x48;
			break;
		case FREENECT_VIDEO_YUV_RGB:
		case FREENECT_VIDEO_YUV_RAW:
			if(dev->video_resolution == FREENECT_RESOLUTION_MEDIUM) {
				mode_value = 0x05; // UYUV mode
				res_value = 0x01; // 640x480
				fps_value = 0x0f; // 15Hz
			} else {
				FN_ERROR("freenect_start_video(): called with invalid format/resolution combination\n");
				return -1;
			}
			mode_reg = 0x0c;
			res_reg = 0x0d;
			fps_reg = 0x0e;
			hflip_reg = 0x47;
			break;
		default:
			FN_ERROR("freenect_start_video(): called with invalid video format %d\n", dev->video_format);
			return -1;
	}

	freenect_frame_mode frame_mode = freenect_get_current_video_mode(dev);
	switch (dev->video_format) {
		case FREENECT_VIDEO_RGB:
			stream_init(ctx, &dev->video, video_mode_for_device(dev, dev->video_resolution, FREENECT_VIDEO_BAYER).bytes, frame_mode.bytes);
			break;
		case FREENECT_VIDEO_BAYER:
			stream_init(ctx, &dev->video, 0, frame_mode.bytes);
			break;
		case FREENECT_VIDEO_IR_8BIT:
			stream_init(ctx, &dev->video, freenect_find_video_mode(dev->video_resolution, FREENECT_VIDEO_IR_10BIT_PACKED).bytes, frame_mode.bytes);
			break;
		case FREENECT_VIDEO_IR_10BIT:
			stream_init(ctx, &dev->video, freenect_find_video_mode(dev->video_resolution, FREENECT_VIDEO_IR_10BIT_PACKED).bytes, frame_mode.bytes);
			break;
		case FREENECT_VIDEO_IR_10BIT_PACKED:
			stream_init(ctx, &dev->video, 0, frame_mode.bytes);
			break;
		case FREENECT_VIDEO_YUV_RGB:
			stream_init(ctx, &dev->video, freenect_find_video_mode(dev->video_resolution, FREENECT_VIDEO_YUV_RAW).bytes, frame_mode.bytes);
			break;
		case FREENECT_VIDEO_YUV_RAW:
			stream_init(ctx, &dev->video, 0, frame_mode.bytes);
			break;
		case FREENECT_VIDEO_DUMMY: // Silence compiler
			break;
	}
	// As Kinect10 does (§10D.1.3): skip the first two complete frames after a
	// video start, while the sensor's exposure and colour settle.
	dev->video.skip_frames = 2;

	const unsigned char video_endpoint = 0x81;
	int packet_size = fnusb_get_max_iso_packet_size(&dev->usb_cam, video_endpoint, VIDEO_PKTBUF);

	FN_INFO("[Stream 80] Negotiated packet size %d\n", packet_size);

	int res = fnusb_start_iso(&dev->usb_cam, &dev->video_isoc, video_process, video_endpoint, NUM_XFERS, PKTS_PER_XFER, packet_size);
	if (res < 0)
		return res;

	write_register(dev, mode_reg, mode_value);
	write_register(dev, res_reg, res_value);
	write_register(dev, fps_reg, fps_value);

	switch (dev->video_format) {
		case FREENECT_VIDEO_RGB:
		case FREENECT_VIDEO_BAYER:
		case FREENECT_VIDEO_YUV_RGB:
		case FREENECT_VIDEO_YUV_RAW:
			write_register(dev, 0x05, 0x01); // start video stream
			break;
		case FREENECT_VIDEO_IR_8BIT:
		case FREENECT_VIDEO_IR_10BIT:
		case FREENECT_VIDEO_IR_10BIT_PACKED:
			write_register(dev, 0x105, 0x00); // Disable auto-cycle of projector
			write_register(dev, 0x05, 0x03); // start video stream
			break;
		case FREENECT_VIDEO_DUMMY: // Silence compiler
			break;
	}
	write_register(dev, hflip_reg, 0x00); // disable Hflip

	dev->video.running = 1;
	fn_color_on_video_start(dev); // Kinect for Windows colour settings, sent once frames arrive
	return 0;
}

int freenect_stop_depth(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;

	if (!dev->depth.running)
		return -1;

	dev->depth.running = 0;
	// A Kinect that was unplugged can't take commands; they would only fail
	// after their timeouts.
	if (!dev->usb_cam.device_dead) {
		write_register(dev, 0x06, 0x00); // stop depth stream
		fn_set_ir_emitter(dev, 0);       // and switch the IR projector off (#385)
	}

	res = fnusb_stop_iso(&dev->usb_cam, &dev->depth_isoc);
	if (res < 0) {
		FN_ERROR("Failed to stop depth isochronous stream: %d\n", res);
		return res;
	}

	freenect_destroy_registration(&(dev->registration));
	stream_freebufs(ctx, &dev->depth);
	free(dev->depth_full);
	dev->depth_full = NULL;
	return 0;
}

int freenect_stop_video(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;

	if (!dev->video.running)
		return -1;

	dev->video.running = 0;
	if (!dev->usb_cam.device_dead)
		write_register(dev, 0x05, 0x00); // stop video stream

	res = fnusb_stop_iso(&dev->usb_cam, &dev->video_isoc);
	if (res < 0) {
		FN_ERROR("Failed to stop RGB isochronous stream: %d\n", res);
		return res;
	}

	stream_freebufs(ctx, &dev->video);
	return 0;
}

void freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb)
{
	dev->depth_cb = cb;
}

void freenect_set_video_callback(freenect_device *dev, freenect_video_cb cb)
{
	dev->video_cb = cb;
}


void freenect_set_depth_chunk_callback(freenect_device *dev, freenect_chunk_cb cb)
{
	dev->depth_chunk_cb = cb;
}

void freenect_set_video_chunk_callback(freenect_device *dev, freenect_chunk_cb cb)
{
	dev->video_chunk_cb = cb;
}

int freenect_get_video_mode_count()
{
	return video_mode_count;
}

freenect_frame_mode freenect_get_video_mode(int mode_num)
{
	if (mode_num >= 0 && mode_num < video_mode_count)
		return supported_video_modes[mode_num];
	freenect_frame_mode retval;
	retval.is_valid = 0;
	return retval;
}

// The frame mode as this device delivers it. The hi-res colour sensor mode
// is 1280x1024 only on the Xbox 1414; the 1473 and Kinect for Windows send
// 1280x960 (§5.8, §10A.5). The mode table keeps 1280x1024, so buffers sized
// from freenect_find_video_mode() are large enough on every model.
static freenect_frame_mode video_mode_for_device(freenect_device *dev, freenect_resolution res, freenect_video_format fmt)
{
	freenect_frame_mode mode = freenect_find_video_mode(res, fmt);
	if (mode.is_valid && res == FREENECT_RESOLUTION_HIGH &&
	    (fmt == FREENECT_VIDEO_RGB || fmt == FREENECT_VIDEO_BAYER) &&
	    (dev->device_type == FN_KINECT_XBOX_1473 || dev->device_type == FN_KINECT_WINDOWS)) {
		mode.height = 960;
		mode.bytes = mode.width * mode.height * (fmt == FREENECT_VIDEO_RGB ? 3 : 1);
	}
	return mode;
}

freenect_frame_mode freenect_get_current_video_mode(freenect_device *dev)
{
	return video_mode_for_device(dev, dev->video_resolution, dev->video_format);
}

freenect_frame_mode freenect_find_video_mode(freenect_resolution res, freenect_video_format fmt)
{
	uint32_t unique_id = MAKE_RESERVED(res, fmt);
	int i;
	for(i = 0 ; i < video_mode_count; i++) {
		if (supported_video_modes[i].reserved == unique_id)
			return supported_video_modes[i];
	}
	freenect_frame_mode retval;
	retval.is_valid = 0;
	return retval;
}

int freenect_set_video_mode(freenect_device* dev, const freenect_frame_mode mode)
{
	freenect_context *ctx = dev->parent;
	if (dev->video.running) {
		FN_ERROR("Tried to set video mode while stream is active\n");
		return -1;
	}
	// Verify that the mode passed in is actually in the supported mode list
	int found = 0;
	int i;
	for(i = 0 ; i < video_mode_count; i++) {
		if (supported_video_modes[i].reserved == mode.reserved) {
			found = 1;
			break;
		}
	}
	if (!found) {
		FN_ERROR("freenect_set_video_mode: freenect_frame_mode provided is invalid\n");
		return -1;
	}

	freenect_resolution res = RESERVED_TO_RESOLUTION(mode.reserved);
	freenect_video_format fmt = (freenect_video_format)RESERVED_TO_FORMAT(mode.reserved);
	dev->video_format = fmt;
	dev->video_resolution = res;
	return 0;
}

int freenect_get_depth_mode_count()
{
	return depth_mode_count;
}

freenect_frame_mode freenect_get_depth_mode(int mode_num)
{
	if (mode_num >= 0 && mode_num < depth_mode_count)
		return supported_depth_modes[mode_num];
	freenect_frame_mode retval;
	retval.is_valid = 0;
	return retval;
}

freenect_frame_mode freenect_get_current_depth_mode(freenect_device *dev)
{
	return freenect_find_depth_mode(dev->depth_resolution, dev->depth_format);
}

freenect_frame_mode freenect_find_depth_mode(freenect_resolution res, freenect_depth_format fmt)
{
	uint32_t unique_id = MAKE_RESERVED(res, fmt);
	int i;
	for(i = 0 ; i < depth_mode_count; i++) {
		if (supported_depth_modes[i].reserved == unique_id)
			return supported_depth_modes[i];
	}
	freenect_frame_mode retval;
	retval.is_valid = 0;
	return retval;
}

int freenect_set_depth_mode(freenect_device* dev, const freenect_frame_mode mode)
{
	freenect_context *ctx = dev->parent;
	if (dev->depth.running) {
		FN_ERROR("Tried to set depth mode while stream is active\n");
		return -1;
	}
	// Verify that the mode passed in is actually in the supported mode list
	int found = 0;
	int i;
	for(i = 0 ; i < depth_mode_count; i++) {
		if (supported_depth_modes[i].reserved == mode.reserved) {
			found = 1;
			break;
		}
	}
	if (!found) {
		FN_ERROR("freenect_set_depth_mode: freenect_frame_mode provided is invalid\n");
		return -1;
	}
	freenect_resolution res = RESERVED_TO_RESOLUTION(mode.reserved);
	freenect_depth_format fmt = (freenect_depth_format)RESERVED_TO_FORMAT(mode.reserved);
	dev->depth_format = fmt;
	dev->depth_resolution = res;
	return 0;
}
int freenect_set_depth_buffer(freenect_device *dev, void *buf)
{
	return stream_setbuf(dev->parent, &dev->depth, buf);
}

int freenect_set_video_buffer(freenect_device *dev, void *buf)
{
	return stream_setbuf(dev->parent, &dev->video, buf);
}

FN_INTERNAL int freenect_camera_init(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;
	res = freenect_fetch_reg_pad_info(dev);
	if (res < 0) {
		FN_ERROR("freenect_camera_init(): Failed to fetch registration pad info for device\n");
		return res;
	}
	res = freenect_fetch_zero_plane_info(dev);
	if (res < 0) {
		FN_ERROR("freenect_camera_init(): Failed to fetch zero plane info for device\n");
		return res;
	}
	res = freenect_set_video_mode(dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
	res = freenect_set_depth_mode(dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));
	res = freenect_fetch_reg_const_shift(dev);
	if (res < 0) {
		FN_ERROR("freenect_camera_init(): Failed to fetch const shift for device\n");
		return res;
	}
	// As before, a camera that does not answer still opens; depth to colour
	// mapping is then off.
	if (freenect_fetch_reg_info(dev) < 0)
		FN_WARNING("freenect_camera_init(): no registration info; depth/colour mapping will be wrong\n");
	// Only for freenect_nudge_stream_timing(); a camera without them streams
	// as before.
	if (fn_fetch_timing_params(dev) < 0)
		FN_INFO("freenect_camera_init(): no stream timing parameters; stream timing alignment unavailable\n");
	// A program that was killed while depth ran leaves the IR projector on
	// until the Kinect loses power; switch it off until depth starts.
	if (fn_set_ir_emitter(dev, 0) < 0)
		FN_INFO("freenect_camera_init(): could not switch the IR projector off\n");
	return 0;
}

FN_INTERNAL int freenect_camera_teardown(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res = 0, r;
	// Stop both streams: this used to return after stopping depth, leaving a
	// running video stream behind.
	if (dev->depth.running) {
		r = freenect_stop_depth(dev);
		if (r < 0) {
			FN_ERROR("freenect_camera_teardown(): Failed to stop depth camera\n");
			res = r;
		}
	}
	if (dev->video.running) {
		r = freenect_stop_video(dev);
		if (r < 0) {
			FN_ERROR("freenect_camera_teardown(): Failed to stop video camera\n");
			res = r;
		}
	}
	freenect_destroy_registration(&(dev->registration));
	free(dev->depth_full); // left by a start that failed
	dev->depth_full = NULL;
	return res;
}
