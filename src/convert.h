/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2021 individual OpenKinect contributors. See the CONTRIB file
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

#include <stdint.h>

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

/**
 * Convert a packed array of n elements with vw useful bits into array of
 * zero-padded 16bit elements.
 *
 * @param src The source packed array, of size (n * vw / 8) bytes
 * @param dest The destination unpacked array, of size (n * 2) bytes
 * @param vw The virtual width of elements, that is the number of useful bits for each of them
 * @param n The number of elements (in particular, of the destination array), NOT a length in bytes
 */
static void convert_packed_to_16bit(uint8_t *src, uint16_t *dest, int vw, int n)
{
	if (vw == 11) {
		convert_packed11_to_16bit(src, dest, n);
		return;
	}

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

#define CLAMP(x) if (x < 0) {x = 0;} if (x > 255) {x = 255;}
static void convert_uyvy_to_rgb(uint8_t *raw_buf, uint8_t *proc_buf, int16_t width, int16_t height)
{
	int x, y;
	for(y = 0; y < height; ++y) {
		for(x = 0; x < width; x+=2) {
			int i = (width * y + x);
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

static void convert_bayer_to_rgb(uint8_t *raw_buf, uint8_t *proc_buf, int16_t width, int16_t height)
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
	nextLine = curLine + width;
	for (y = 0; y < height; ++y) {

		if ((y > 0) && (y < height-1))
			prevLine = curLine - width; // normal case
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
		for (x = 0; x < width-1; ++x) {
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
