#include <libfreenect.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

int bMirror = 1;


#define RGB_REG_X_RES 640
#define RGB_REG_Y_RES 512
#define XN_DEPTH_XRES 640
#define XN_DEPTH_YRES 480
#define XN_CMOS_VGAOUTPUT_XRES 1280
#define XN_SENSOR_WIN_OFFET_X 1
#define XN_SENSOR_WIN_OFFET_Y 1
#define RGB_REG_X_VAL_SCALE 16
#define S2D_PEL_CONST 10
#define S2D_CONST_OFFSET 0.375
#define XN_DEVICE_MAX_DEPTH 10000
#define XN_DEVICE_SENSOR_NO_DEPTH_VALUE 0

#define DEVICE_MAX_SHIFT_VALUE 2048

#define DENSE_REGISTRATION


/// fill the table of horizontal shift values for metric depth -> RGB conversion
void freenect_init_depth_to_rgb(int16_t* depth_to_rgb, freenect_zero_plane_info* zpi) {

	uint32_t i,xScale = XN_CMOS_VGAOUTPUT_XRES / XN_DEPTH_XRES;
	
	double pelSize = 1.0 / (zpi->reference_pixel_size * xScale * S2D_PEL_CONST);
	double pelDCC = zpi->dcmos_rcmos_dist * pelSize * S2D_PEL_CONST;
	double pelDSR = zpi->reference_distance * pelSize * S2D_PEL_CONST;
	
	memset(depth_to_rgb, XN_DEVICE_SENSOR_NO_DEPTH_VALUE, XN_DEVICE_MAX_DEPTH * sizeof(int16_t));
	
	for (i = 0; i < XN_DEVICE_MAX_DEPTH; i++) {
		double curDepth = i * pelSize;
		depth_to_rgb[i] = ((pelDCC * (curDepth - pelDSR) / curDepth) + (S2D_CONST_OFFSET)) * RGB_REG_X_VAL_SCALE;
	}
}


int freenect_apply_registration(freenect_registration* reg, uint16_t* input_raw, uint16_t* output_mm) {

	memset(output_mm, XN_DEVICE_SENSOR_NO_DEPTH_VALUE, XN_DEPTH_XRES * XN_DEPTH_YRES * sizeof(uint16_t)); // clear the output image
	uint32_t constOffset = XN_DEPTH_YRES * reg->reg_pad_info.start_lines;
	
	uint32_t x,y,sourceIndex = 0;
	for (y = 0; y < XN_DEPTH_YRES; y++) {
		uint32_t registrationOffset = bMirror ? (y + 1) * (XN_DEPTH_XRES * 2) - 2 : y * XN_DEPTH_XRES * 2;
		int16_t* curRegistrationTable = reg->registration_table+registrationOffset;
		
		for (x = 0; x < XN_DEPTH_XRES; x++) {
		
			// get the value at the current depth pixel, convert to millimeters
			uint16_t newDepthValue = reg->raw_to_mm_shift[ input_raw[sourceIndex] ];
			
			// so long as the current pixel has a depth value
			if (newDepthValue != XN_DEVICE_SENSOR_NO_DEPTH_VALUE) {
			
				// calculate the new x and y location for that pixel
				// using curRegistrationTable for the basic rectification
				// and depthToRgbShift for determining the x shift
				uint32_t nx = (uint32_t) (curRegistrationTable[0] + reg->depth_to_rgb_shift[newDepthValue]) / RGB_REG_X_VAL_SCALE;
				uint32_t ny = curRegistrationTable[1];
				
				// ignore anything outside the image bounds
				if (nx < XN_DEPTH_XRES) {
					// convert nx, ny to an index in the depth image array
					uint32_t targetIndex = bMirror ? (ny + 1) * XN_DEPTH_XRES - nx - 2 : (ny * XN_DEPTH_XRES) + nx;
					targetIndex -= constOffset;
					
					// get the current value at the new location
					uint16_t curDepthValue = output_mm[targetIndex];
					// make sure the new location is empty, or the new value is closer
					if ((curDepthValue == XN_DEVICE_SENSOR_NO_DEPTH_VALUE) || (curDepthValue > newDepthValue)) {
						output_mm[targetIndex] = newDepthValue; // always save depth at current location
#ifdef DENSE_REGISTRATION
						// if we're not on the first row, or the first column
						if ( nx > 0 && ny > 0 ) {
							output_mm[targetIndex - XN_DEPTH_XRES] = newDepthValue; // save depth north
							output_mm[targetIndex - XN_DEPTH_XRES - 1] = newDepthValue; // save depth northwest
							output_mm[targetIndex - 1] = newDepthValue; // save depth west
						}
						// if we're on the first column
						else if( ny > 0 ) {
							output_mm[targetIndex - XN_DEPTH_XRES] = newDepthValue; // save depth north
						}
						// if we're on the first row
						else if( nx > 0 ) {
							output_mm[targetIndex - 1] = newDepthValue; // save depth west
						}
#endif
					}
				}
			}
			curRegistrationTable += bMirror ? -2 : +2;
			sourceIndex++;
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

void BuildRegTable1080(int16_t* m_pDepthToShiftTable, int16_t* m_pRegistrationTable, freenect_reg_info* RegData, freenect_reg_pad_info* m_padInfo) {	
	double* RegXTable = malloc(sizeof(double)*RGB_REG_X_RES*RGB_REG_Y_RES);
	double* RegYTable = malloc(sizeof(double)*RGB_REG_X_RES*RGB_REG_Y_RES);
	
	// int16_t* pRGBRegDepthToShiftTable = (int16_t*)m_pDepthToShiftTable;  // unused
	uint16_t nDepthXRes = XN_DEPTH_XRES;
	uint16_t nDepthYRes = XN_DEPTH_YRES;
	// uint32_t nXScale = XN_CMOS_VGAOUTPUT_XRES / XN_DEPTH_XRES; // unused
	double* pRegXTable = (double*)RegXTable;
	double* pRegYTable = (double*)RegYTable;
	int16_t* pRegTable = (int16_t*)m_pRegistrationTable;
	double nNewX = 0;
	double nNewY = 0;
	
	// Create the dx dy tables
	freenect_create_dxdy_tables(RegXTable, RegYTable, nDepthXRes,	nDepthYRes, RegData );
	
	// Pre-process the table, do sanity checks and convert it from double to ints (for better performance)
	int32_t nY,nX;
	for (nY=0; nY<nDepthYRes; nY++) {
		for (nX=0; nX<nDepthXRes; nX++) {
			nNewX = (nX + *pRegXTable + XN_SENSOR_WIN_OFFET_X) * RGB_REG_X_VAL_SCALE;
			nNewY = (nY + *pRegYTable + XN_SENSOR_WIN_OFFET_Y);
			
			if (nNewY < 1) {
				nNewY = 1;
				nNewX = ((nDepthXRes*4) * RGB_REG_X_VAL_SCALE); // set illegal value on purpose
			}
			
			if (nNewX < 1) {
				nNewX = ((nDepthXRes*4) * RGB_REG_X_VAL_SCALE); // set illegal value on purpose
			}
			
			if (nNewY > nDepthYRes) {
				nNewY = nDepthYRes;
				goto FinishLoop;
			}
			
			*pRegTable = nNewX;
			*(pRegTable+1) = nNewY;
			
			pRegXTable++;
			pRegYTable++;
			pRegTable+=2;
		}
	}
	
FinishLoop:
	free( RegXTable );
	free( RegYTable );
}


// TODO: can these be extracted from the Kinect?
int32_t paramCoeff = 4;
int32_t constShift = 200;
int32_t shiftScale = 10;

/// convert raw shift value to metric depth (in mm)
uint16_t freenect_raw_to_mm(uint16_t raw, freenect_zero_plane_info* zpi) {
	double fixedRefX = ((raw - (paramCoeff * constShift)) / paramCoeff) - S2D_CONST_OFFSET;
	double metric = fixedRefX * zpi->reference_pixel_size;
	return shiftScale * ((metric * zpi->reference_distance / (zpi->dcmos_emitter_dist - metric)) + zpi->reference_distance);
}


int freenect_init_registration(freenect_device* dev, freenect_registration* reg) {

	uint16_t i;

	reg->reg_info        = freenect_get_reg_info( dev );
	reg->reg_pad_info    = freenect_get_reg_pad_info( dev );
	reg->zero_plane_info = freenect_get_zero_plane_info( dev );

	/*freenect_reg_info ritmp = { 2048330528, 1964, 56, -26, 600, 6161, -13, 2825, 684, 5, 6434, 10062, 130801, 0, 0, 170, 136, 2095986, 890, 763, 2096378, 134215474, 134217093, 134216989, 134216925, 0, 134216984, 0, 134214659 }; reg->reg_info = ritmp;
	freenect_reg_pad_info rptmp = { 0, 0, 0 }; reg->reg_pad_info = rptmp;
	freenect_zero_plane_info zptmp = { 7.5, 2.3, 120, 0.1042 }; reg->zero_plane_info = zptmp;*/

	reg->raw_to_mm_shift    = malloc( sizeof(uint16_t) * DEVICE_MAX_SHIFT_VALUE );
	reg->depth_to_rgb_shift = malloc( sizeof( int16_t) * XN_DEVICE_MAX_DEPTH );
	reg->registration_table = malloc( sizeof( int16_t) * XN_DEPTH_XRES * XN_DEPTH_YRES * 2 );

	reg->raw_to_mm_shift[0] = 0; // 0 == no depth value
	for (i = 1; i < DEVICE_MAX_SHIFT_VALUE; i++)
		reg->raw_to_mm_shift[i] = freenect_raw_to_mm( i, &(reg->zero_plane_info) );
	
	freenect_init_depth_to_rgb( reg->depth_to_rgb_shift, &(reg->zero_plane_info) );
	
	BuildRegTable1080( reg->depth_to_rgb_shift, reg->registration_table, &(reg->reg_info), &(reg->reg_pad_info) );

	return 0;
}

