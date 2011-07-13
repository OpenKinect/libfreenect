#include <libfreenect.h>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <math.h>

	std::vector<uint16_t> depth;
	
	std::vector<uint16_t> input, output;
	
	std::vector<int16_t> m_pDepthToShiftTable;
	std::vector<int16_t> m_pRegistrationTable;

RegistrationInfo RegData = { 2048330528, 1964, 56, -26, 600, 6161, -13, 2825, 684, 5, 6434, 10062, 130801, 0, 0, 170, 136, 2095986, 890, 763, 2096378, 134215474, 134217093, 134216989, 134216925, 0, 134216984, 0, 134214659 };
RegistrationPadInfo m_padInfo = { 0, 0, 0 };

bool bMirror = true;

double dPlanePixelSize = 0.1042; // fReferencePixelSize
uint64_t nPlaneDsr = 120; // fReferenceDistance
double planeDcl = 7.5; // pConfig->fEmitterDCmosDistancea
// double fDCmosRCmosDistance = 2.3;
int32_t paramCoeff = 4; // pConfig->nParamCoeff
int32_t constShift = 200; // pConfig->nConstShift
int32_t shiftScale = 10; // pConfig->nShiftScale


#define RGB_REG_X_RES 640
#define RGB_REG_Y_RES 512
#define XN_DEPTH_XRES 640
#define XN_DEPTH_YRES 480
#define XN_CMOS_VGAOUTPUT_XRES 1280
#define XN_SENSOR_WIN_OFFET_X 1
#define XN_SENSOR_WIN_OFFET_Y 1
#define RGB_REG_X_VAL_SCALE 16
#define S2D_PEL_CONST 10
#define XN_SENSOR_DEPTH_RGB_CMOS_DISTANCE 2.3 // seems to be different according to usb (says 2.3)
#define S2D_CONST_OFFSET 0.375
#define XN_DEVICE_MAX_DEPTH 10000
#define XN_DEVICE_SENSOR_NO_DEPTH_VALUE 0

#define DEVICE_MAX_SHIFT_VALUE 2048

#define DENSE_REGISTRATION

void BuildDepthToRgbShiftTable(int16_t* depthToRgbShift) {
	uint32_t xScale = XN_CMOS_VGAOUTPUT_XRES / XN_DEPTH_XRES;
	
	double pelSize = 1.0 / (dPlanePixelSize * xScale * S2D_PEL_CONST);
	double pelDCC = XN_SENSOR_DEPTH_RGB_CMOS_DISTANCE * pelSize * S2D_PEL_CONST;
	double pelDSR = nPlaneDsr * pelSize * S2D_PEL_CONST;
	
	memset(depthToRgbShift, XN_DEVICE_SENSOR_NO_DEPTH_VALUE, XN_DEVICE_MAX_DEPTH * sizeof(int16_t));
	
	for (uint32_t i = 0; i < XN_DEVICE_MAX_DEPTH; i++) {
		double curDepth = i * pelSize;
		depthToRgbShift[i] = ((pelDCC * (curDepth - pelDSR) / curDepth) + (S2D_CONST_OFFSET)) * RGB_REG_X_VAL_SCALE;
	}
}


void Apply1080(uint16_t* depthInput, uint16_t* depthOutput, int16_t* registrationTable, int16_t* depthToRgbShift, RegistrationPadInfo& padInfo) {
	memset(depthOutput, XN_DEVICE_SENSOR_NO_DEPTH_VALUE, XN_DEPTH_XRES * XN_DEPTH_YRES * sizeof(uint16_t)); // clear the output image
	uint32_t constOffset = XN_DEPTH_YRES * padInfo.nStartLines;
	
	uint32_t sourceIndex = 0;
	for (uint32_t y = 0; y < XN_DEPTH_YRES; y++) {
		uint32_t registrationOffset = bMirror ? (y + 1) * (XN_DEPTH_XRES * 2) - 2 : y * XN_DEPTH_XRES * 2;
		int16_t* curRegistrationTable = (int16_t*) &registrationTable[registrationOffset];
		
		for (uint32_t x = 0; x < XN_DEPTH_XRES; x++) {
		
			// get the value at the current depth pixel
			uint16_t newDepthValue = depthInput[sourceIndex];
			
			// so long as the current pixel has a depth value
			if (newDepthValue != XN_DEVICE_SENSOR_NO_DEPTH_VALUE) {
			
				// calculate the new x and y location for that pixel
				// using curRegistrationTable for the basic rectification
				// and depthToRgbShift for determining the x shift
				uint32_t nx = (uint32_t) (curRegistrationTable[0] + depthToRgbShift[newDepthValue]) / RGB_REG_X_VAL_SCALE;
				uint32_t ny = curRegistrationTable[1];
				
				// ignore anything outside the image bounds
				if (nx < XN_DEPTH_XRES) {
					// convert nx, ny to an index in the depth image array
					uint32_t targetIndex = bMirror ? (ny + 1) * XN_DEPTH_XRES - nx - 2 : (ny * XN_DEPTH_XRES) + nx;
					targetIndex -= constOffset;
					
					// get the current value at the new location
					uint16_t curDepthValue = depthOutput[targetIndex];
					// make sure the new location is empty, or the new value is closer
					if ((curDepthValue == XN_DEVICE_SENSOR_NO_DEPTH_VALUE) || (curDepthValue > newDepthValue)) {
						depthOutput[targetIndex] = newDepthValue; // always save depth at current location
#ifdef DENSE_REGISTRATION
						// if we're not on the first row, or the first column
						if ( nx > 0 && ny > 0 ) {
							depthOutput[targetIndex - XN_DEPTH_XRES] = newDepthValue; // save depth north
							depthOutput[targetIndex - XN_DEPTH_XRES - 1] = newDepthValue; // save depth northwest
							depthOutput[targetIndex - 1] = newDepthValue; // save depth west
						}
						// if we're on the first column
						else if( ny > 0 ) {
							depthOutput[targetIndex - XN_DEPTH_XRES] = newDepthValue; // save depth north
						}
						// if we're on the first row
						else if( nx > 0 ) {
							depthOutput[targetIndex - 1] = newDepthValue; // save depth west
						}
#endif
					}
				}
			}
			curRegistrationTable += bMirror ? -2 : +2;
			sourceIndex++;
		}
	}
}


void CreateDXDYTables (double* RegXTable, double* RegYTable, int32_t resX, int32_t resY, RegistrationInfo regdata ) {

	int64_t AX6 = regdata.nRGS_AX;
	int64_t BX6 = regdata.nRGS_BX;
	int64_t CX2 = regdata.nRGS_CX;
	int64_t DX2 = regdata.nRGS_DX;

	int64_t AY6 = regdata.nRGS_AY;
	int64_t BY6 = regdata.nRGS_BY;
	int64_t CY2 = regdata.nRGS_CY;
	int64_t DY2 = regdata.nRGS_DY;

	// don't merge the shift operations - necessary for proper 32-bit clamping of extracted values
	int32_t deltaBetaX = (regdata.nRGS_DX_BETA_INC << 8) >> 8;
	int32_t deltaBetaY = (regdata.nRGS_DY_BETA_INC << 8) >> 8;

	int64_t dX0 = (regdata.nRGS_DX_START << 13) >> 4;
	int64_t dY0 = (regdata.nRGS_DY_START << 13) >> 4;

	int64_t dXdX0 = (regdata.nRGS_DXDX_START << 11) >> 3;
	int64_t dXdY0 = (regdata.nRGS_DXDY_START << 11) >> 3;
	int64_t dYdX0 = (regdata.nRGS_DYDX_START << 11) >> 3;
	int64_t dYdY0 = (regdata.nRGS_DYDY_START << 11) >> 3;

	int64_t dXdXdX0 = (regdata.nRGS_DXDXDX_START << 5) << 3;
	int64_t dYdXdX0 = (regdata.nRGS_DYDXDX_START << 5) << 3;
	int64_t dYdXdY0 = (regdata.nRGS_DYDXDY_START << 5) << 3;
	int64_t dXdXdY0 = (regdata.nRGS_DXDXDY_START << 5) << 3;
	int64_t dYdYdX0 = (regdata.nRGS_DYDYDX_START << 5) << 3;
	int64_t dYdYdY0 = (regdata.nRGS_DYDYDY_START << 5) << 3;

	int32_t betaX = (regdata.nRGS_DX_BETA_START << 15) >> 8;
	int32_t betaY = (regdata.nRGS_DY_BETA_START << 15) >> 8;

	int32_t tOffs = 0;

	for(int32_t row = 0 ; row<resY ; row++)
	{
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

			for(int32_t col = 0 ; col<resX ; col++, tOffs++)
		{
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

void BuildRegTable1080(int16_t* m_pDepthToShiftTable, int16_t* m_pRegistrationTable, RegistrationInfo& RegData, RegistrationPadInfo& m_padInfo) {	
	double* RegXTable = new double[RGB_REG_X_RES*RGB_REG_Y_RES];
	double* RegYTable = new double[RGB_REG_X_RES*RGB_REG_Y_RES];
	
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
	CreateDXDYTables(RegXTable, RegYTable, nDepthXRes,	nDepthYRes, RegData );
	
	// Pre-process the table, do sanity checks and convert it from double to ints (for better performance)
	for (int32_t nY=0; nY<nDepthYRes; nY++) {
		for (int32_t nX=0; nX<nDepthXRes; nX++) {
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
	delete [] RegXTable;
	delete [] RegYTable;
}

// conversion routines

uint16_t RawToDepth(uint16_t raw) {
	double fixedRefX = ((raw - (paramCoeff * constShift)) / paramCoeff) - 0.375;
	double metric = fixedRefX * dPlanePixelSize;
	return shiftScale * ((metric * nPlaneDsr / (planeDcl - metric)) + nPlaneDsr);
}

const float
k1 = 127.50,
k2 = 2842.5,
k3 = 1.1863;

inline float rawToMillimeters(float raw) {
	return k1 * tan((raw / k2) + k3);
}

// main routine

int main() {

	int n = XN_DEPTH_XRES * XN_DEPTH_YRES;

	depth.resize(n);
	input.resize(n);
	output.resize(n);

	FILE* df = fopen("depth.raw","r");
	fread(&(depth[0]),640*480*2,1,df);
	fclose(df);

	for(int i = 0; i < n; i++) {
		input[i] = rawToMillimeters(depth[i]);
		//input[i] = RawToDepth(depth[i]);
	}
	
	/*for(int i = 0; i < 1024; i++) {
		printf("\t%d\t%f\n",RawToDepth(i),rawToMillimeters(i));
	}*/

	m_pDepthToShiftTable.resize(XN_DEVICE_MAX_DEPTH);
	m_pRegistrationTable.resize(XN_DEPTH_XRES * XN_DEPTH_YRES * 2);
	
	BuildDepthToRgbShiftTable(&m_pDepthToShiftTable[0]);
	
	BuildRegTable1080(&m_pDepthToShiftTable[0], 
										&m_pRegistrationTable[0], 
										RegData, 
										m_padInfo);
										
	Apply1080(&input[0], &output[0], &m_pRegistrationTable[0], &m_pDepthToShiftTable[0], m_padInfo);
	
	FILE* cf = fopen("calib.pgm","w+");
	fprintf(cf,"P5 640 480 65535 ");
	fwrite(&(output[0]),640*480*2,1,cf);
	fclose(cf);
}

