/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2011 individual OpenKinect contributors. See the CONTRIB file
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

#include <libfreenect.h>
#include <freenect_internal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>


#define REG_X_VAL_SCALE 256 // "fixed-point" precision for double -> int32_t conversion

#define S2D_PEL_CONST 10
#define S2D_CONST_OFFSET 0.375

#define DEPTH_SENSOR_X_RES 1280
#define DEPTH_MIRROR_X 1

#define DEPTH_MAX_METRIC_VALUE 10000
#define DEPTH_MAX_RAW_VALUE 2048
#define DEPTH_NO_RAW_VALUE 2047
#define DEPTH_NO_MM_VALUE 0

#define DEPTH_X_OFFSET 1
#define DEPTH_Y_OFFSET 1
#define DEPTH_X_RES 640
#define DEPTH_Y_RES 480

// #define DENSE_REGISTRATION


/// fill the table of horizontal shift values for metric depth -> RGB conversion
void freenect_init_depth_to_rgb(int32_t* depth_to_rgb, freenect_zero_plane_info* zpi) {

	uint32_t i,xScale = DEPTH_SENSOR_X_RES / DEPTH_X_RES;
	
	double pelSize = 1.0 / (zpi->reference_pixel_size * xScale * S2D_PEL_CONST);
	double pelDCC = zpi->dcmos_rcmos_dist * pelSize * S2D_PEL_CONST;
	double pelDSR = zpi->reference_distance * pelSize * S2D_PEL_CONST;
	
	memset(depth_to_rgb, DEPTH_NO_MM_VALUE, DEPTH_MAX_METRIC_VALUE * sizeof(int32_t));
	
	for (i = 0; i < DEPTH_MAX_METRIC_VALUE; i++) {
		double curDepth = i * pelSize;
		depth_to_rgb[i] = ((pelDCC * (curDepth - pelDSR) / curDepth) + S2D_CONST_OFFSET) * REG_X_VAL_SCALE;
	}
}

inline void unpack_8_pixels(uint8_t *raw, uint16_t *frame)
{
	uint16_t baseMask = 0x7FF;

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
}


int freenect_apply_registration(freenect_registration* reg, uint8_t* input_packed, uint16_t* output_mm) {

	size_t i, *wipe = (size_t*)output_mm;
	for (i = 0; i < DEPTH_X_RES * DEPTH_Y_RES * sizeof(uint16_t) / sizeof(size_t); i++) wipe[i] = DEPTH_NO_MM_VALUE;

	uint16_t unpack[8];

	uint32_t target_offset = DEPTH_Y_RES * reg->reg_pad_info.start_lines;
	uint32_t x,y,source_index = 8;

	for (y = 0; y < DEPTH_Y_RES; y++) {
		for (x = 0; x < DEPTH_X_RES; x++) {
			
			if (source_index == 8) {
				unpack_8_pixels( input_packed, unpack );
				source_index = 0;
				input_packed += 11;
			}

			// get the value at the current depth pixel, convert to millimeters
			uint16_t metric_depth = reg->raw_to_mm_shift[ unpack[source_index++] ];
			
			// so long as the current pixel has a depth value
			if (metric_depth == DEPTH_NO_MM_VALUE) continue;
			if (metric_depth >= DEPTH_MAX_METRIC_VALUE) continue;
			
			// calculate the new x and y location for that pixel
			// using registration_table for the basic rectification
			// and depth_to_rgb_shift for determining the x shift
			uint32_t reg_index = DEPTH_MIRROR_X ? ((y + 1) * DEPTH_X_RES - x - 1) : (y * DEPTH_X_RES + x);
			uint32_t nx = (reg->registration_table[reg_index][0] + reg->depth_to_rgb_shift[metric_depth]) / REG_X_VAL_SCALE;
			uint32_t ny =  reg->registration_table[reg_index][1];

			// ignore anything outside the image bounds
			if (nx >= DEPTH_X_RES) continue;

			// convert nx, ny to an index in the depth image array
			uint32_t target_index = (DEPTH_MIRROR_X ? ((ny + 1) * DEPTH_X_RES - nx - 1) : (ny * DEPTH_X_RES + nx)) - target_offset;

			// get the current value at the new location
			uint16_t current_depth = output_mm[target_index];

			// make sure the new location is empty, or the new value is closer
			if ((current_depth == DEPTH_NO_MM_VALUE) || (current_depth > metric_depth)) {
				output_mm[target_index] = metric_depth; // always save depth at current location

				#ifdef DENSE_REGISTRATION
					// if we're not on the first row, or the first column
					if ((nx > 0) && (ny > 0)) {
						output_mm[target_index - DEPTH_X_RES    ] = metric_depth; // save depth at (x,y-1)
						output_mm[target_index - DEPTH_X_RES - 1] = metric_depth; // save depth at (x-1,y-1)
						output_mm[target_index               - 1] = metric_depth; // save depth at (x-1,y)
					} else if (ny > 0) {
						output_mm[target_index - DEPTH_X_RES] = metric_depth; // save depth at (x,y-1)
					} else if (nx > 0) {
						output_mm[target_index - 1] = metric_depth; // save depth at (x-1,y)
					}
				#endif
			}
		}
	}
	return 0;
}


void freenect_create_dxdy_tables(double* RegXTable, double* RegYTable, int32_t resX, int32_t resY, freenect_reg_info* regdata ) {

	int64_t AX6 = regdata->ax;
	int64_t BX6 = regdata->bx;
	int64_t CX2 = regdata->cx;
	int64_t DX2 = regdata->dx;

	int64_t AY6 = regdata->ay;
	int64_t BY6 = regdata->by;
	int64_t CY2 = regdata->cy;
	int64_t DY2 = regdata->dy;

	// don't merge the shift operations - necessary for proper 32-bit clamping of extracted values
	int32_t deltaBetaX = (regdata->dx_beta_inc << 8) >> 8;
	int32_t deltaBetaY = (regdata->dy_beta_inc << 8) >> 8;

	int64_t dX0 = (regdata->dx_start << 13) >> 4;
	int64_t dY0 = (regdata->dy_start << 13) >> 4;

	int64_t dXdX0 = (regdata->dxdx_start << 11) >> 3;
	int64_t dXdY0 = (regdata->dxdy_start << 11) >> 3;
	int64_t dYdX0 = (regdata->dydx_start << 11) >> 3;
	int64_t dYdY0 = (regdata->dydy_start << 11) >> 3;

	int64_t dXdXdX0 = (regdata->dxdxdx_start << 5) << 3;
	int64_t dYdXdX0 = (regdata->dydxdx_start << 5) << 3;
	int64_t dYdXdY0 = (regdata->dydxdy_start << 5) << 3;
	int64_t dXdXdY0 = (regdata->dxdxdy_start << 5) << 3;
	int64_t dYdYdX0 = (regdata->dydydx_start << 5) << 3;
	int64_t dYdYdY0 = (regdata->dydydy_start << 5) << 3;

	int32_t betaX = (regdata->dx_beta_start << 15) >> 8;
	int32_t betaY = (regdata->dy_beta_start << 15) >> 8;

	int32_t row,col,tOffs = 0;

	for (row = 0 ; row < resY ; row++) {

		dXdXdX0 += CX2;

		dXdX0   += dYdXdX0 >> 8;
		dYdXdX0 += DX2;
		
		dX0     += dYdX0 >> 6;
		dYdX0   += dYdYdX0 >> 8;
		dYdYdX0 += BX6;
		
		dXdXdY0 += CY2;

		dXdY0   += dYdXdY0 >> 8;
		dYdXdY0 += DY2;
		
		betaY   += deltaBetaY;
		dY0     += dYdY0 >> 6;
		dYdY0   += dYdYdY0 >> 8;
		dYdYdY0 += BY6;
		
		int64_t coldXdXdY0 = dXdXdY0, coldXdY0 = dXdY0, coldY0 = dY0;
		
		int64_t coldXdXdX0 = dXdXdX0, coldXdX0 = dXdX0, coldX0 = dX0;
		int32_t colBetaX = betaX;

		for (col = 0 ; col < resX ; col++, tOffs++) {

			RegXTable[tOffs] = coldX0 * (1.0/(1<<17));
			RegYTable[tOffs] = coldY0 * (1.0/(1<<17));
			
			colBetaX   += deltaBetaX;
			coldX0     += coldXdX0 >> 6;
			coldXdX0   += coldXdXdX0 >> 8;
			coldXdXdX0 += AX6;

			coldY0     += coldXdY0 >> 6;
			coldXdY0   += coldXdXdY0 >> 8;
			coldXdXdY0 += AY6;
		}
	}
}

void freenect_init_registration_table(int32_t (*registration_table)[2], freenect_reg_info* reg_info) {

	double regtable_dx[DEPTH_X_RES*DEPTH_Y_RES];
	double regtable_dy[DEPTH_X_RES*DEPTH_Y_RES];
	int32_t x,y,index = 0;

	// create temporary dx/dy tables
	freenect_create_dxdy_tables( regtable_dx, regtable_dy, DEPTH_X_RES, DEPTH_Y_RES, reg_info );

	// pre-process the table, do sanity checks and convert it from double to ints (for better performance)
	for (y = 0; y < DEPTH_Y_RES; y++) {
		for (x = 0; x < DEPTH_X_RES; x++, index++) {

			double new_x = x + regtable_dx[index] + DEPTH_X_OFFSET;
			double new_y = y + regtable_dy[index] + DEPTH_Y_OFFSET;
			
			if ((new_x < 0) || (new_y < 0) || (new_x >= DEPTH_X_RES) || (new_y >= DEPTH_Y_RES))
				new_x = 2 * DEPTH_X_RES; // set illegal value on purpose

			registration_table[index][0] = new_x * REG_X_VAL_SCALE;
			registration_table[index][1] = new_y;
		}
	}
}


// TODO: can these be extracted from the Kinect?
double paramCoeff = 4;
double constShift = 200;
double shiftScale = 10;
double pixelSizeFactor = 1;

/// convert raw shift value to metric depth (in mm)
uint16_t freenect_raw_to_mm(uint16_t raw, freenect_zero_plane_info* zpi) {
	double fixedRefX = ((raw - (paramCoeff * constShift / pixelSizeFactor)) / paramCoeff) - S2D_CONST_OFFSET;
	double metric = fixedRefX * zpi->reference_pixel_size * pixelSizeFactor;
	return shiftScale * ((metric * zpi->reference_distance / (zpi->dcmos_emitter_dist - metric)) + zpi->reference_distance);
}


int freenect_init_registration(freenect_device* dev, freenect_registration* reg) {

	uint16_t i;

	if (reg == NULL) reg = &(dev->registration);

	// default values ripped from my Kinect
	freenect_reg_info ritmp = { 2048330528, 1964, 56, -26, 600, 6161, -13, 2825, 684, 5, 6434, 10062, 130801, 0, 0, 170, 136, 2095986, 890, 763, 2096378, 134215474, 134217093, 134216989, 134216925, 0, 134216984, 0, 134214659 }; reg->reg_info = ritmp;
	freenect_reg_pad_info rptmp = { 0, 0, 0 }; reg->reg_pad_info = rptmp;
	freenect_zero_plane_info zptmp = { 7.5, 2.3, 120, 0.1042 }; reg->zero_plane_info = zptmp;

	// if device is connected, retrieve data
	if (dev) {
		reg->reg_info        = freenect_get_reg_info( dev );
		reg->reg_pad_info    = freenect_get_reg_pad_info( dev );
		reg->zero_plane_info = freenect_get_zero_plane_info( dev );
	}

	// TODO: determine why this ugly hack had a positive effect for low distances
	// for very unclear reasons, setting this value to -0.5
	// results in a much more accurate depth -> RGB X shift
	// reg->zero_plane_info.dcmos_rcmos_dist = -0.5;

	reg->raw_to_mm_shift    = malloc( sizeof(uint16_t) * DEPTH_MAX_RAW_VALUE );
	reg->depth_to_rgb_shift = malloc( sizeof( int32_t) * DEPTH_MAX_METRIC_VALUE );
	reg->registration_table = malloc( sizeof( int32_t) * DEPTH_X_RES * DEPTH_Y_RES * 2 );

	for (i = 0; i < DEPTH_MAX_RAW_VALUE; i++)
		reg->raw_to_mm_shift[i] = freenect_raw_to_mm( i, &(reg->zero_plane_info) );
	reg->raw_to_mm_shift[DEPTH_NO_RAW_VALUE] = DEPTH_NO_MM_VALUE;
	
	freenect_init_depth_to_rgb( reg->depth_to_rgb_shift, &(reg->zero_plane_info) );
	
	freenect_init_registration_table( reg->registration_table, &(reg->reg_info) );

	return 0;
}

void freenect_cleanup_registration(freenect_registration* reg) {
	free( &(reg->reg_info) );
	free( &(reg->reg_pad_info) );
	free( &(reg->zero_plane_info) );
}

