#include <libfreenect.h>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*struct RegistrationInfo {
	// from home kinect
	static const int32_t nRGS_DX_START = 3145;
	static const int32_t nRGS_AX = 1681;
	static const int32_t nRGS_BX = -5;
	static const int32_t nRGS_CX = 1;
	static const int32_t nRGS_DX = 468;

	static const int32_t nRGS_DY_START = 6771;
	static const int32_t nRGS_AY = -8;
	static const int32_t nRGS_BY = 2647;
	static const int32_t nRGS_CY = 599;
	static const int32_t nRGS_DY = 11;

	static const int32_t nRGS_DX_BETA_START = 10062;
	static const int32_t nRGS_DY_BETA_START = 130801;

	static const int32_t nRGS_DX_BETA_INC = 170;
	static const int32_t nRGS_DY_BETA_INC = 136;
	static const int32_t nRGS_DXDX_START = 2095911;
	static const int32_t nRGS_DXDY_START = 682;
	static const int32_t nRGS_DYDX_START = 651;
	static const int32_t nRGS_DYDY_START = 2096067;

	static const int32_t nRGS_DXDXDX_START = 134215532;
	static const int32_t nRGS_DYDXDX_START = 134217276;
	static const int32_t nRGS_DXDXDY_START = 134217118;
	static const int32_t nRGS_DYDXDY_START = 134216919;
	static const int32_t nRGS_DYDYDX_START = 134217176;
	static const int32_t nRGS_DYDYDY_START = 134215010;
}; */

	std::vector<uint16_t> depth;
	//ofImage color;
	
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
#define XN_SENSOR_DEPTH_RGB_CMOS_DISTANCE 2.4 // seems to be different according to usb (says 2.3)
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

// extracts a packed integer of length fieldWidth starting at fieldOffset from regValue
int32_t GetFieldValueSigned(uint32_t regValue, int32_t fieldWidth, int32_t fieldOffset) {
	int32_t val = (int) (regValue >> fieldOffset);
	val = (val << (32 - fieldWidth)) >> (32 - fieldWidth);
	return val;
}

static void incrementalFitting50(int64_t dPrev, int64_t ddPrev, int64_t dddPrev, int64_t coeff, int32_t betaPrev, int32_t dBeta, int64_t &dCurr, int64_t &ddCurr, int64_t &dddCurr, int32_t &betaCurr);

static void incrementalFitting50(int64_t ddPrev, int64_t dddPrev, int64_t coeff, int64_t &ddCurr, int64_t &dddCurr) {
	int64_t dummy1;
	int32_t dummy2;
	incrementalFitting50(0, ddPrev, dddPrev, coeff, 0, 0, dummy1, ddCurr, dddCurr, dummy2);
}

static void incrementalFitting50(int64_t dddPrev, int64_t coeff, int64_t &dddCurr) {
	int64_t dummy1, dummy2;
	int32_t dummy3;
	incrementalFitting50(0, 0, dddPrev, coeff, 0, 0, dummy1, dummy2, dddCurr, dummy3);
}

void incrementalFitting50(int64_t dPrev, int64_t ddPrev, int64_t dddPrev, int64_t coeff, int32_t betaPrev, int32_t dBeta, int64_t &dCurr, int64_t &ddCurr, int64_t &dddCurr, int32_t &betaCurr)
{
	dCurr = dPrev+(ddPrev>>6);
	ddCurr = ddPrev+(dddPrev>>8);
	dddCurr = dddPrev+coeff;
	betaCurr = betaPrev+dBeta;
}

void CreateDXDYTables (double* RegXTable, double* RegYTable,
											 int32_t resX, int32_t resY,
											 int64_t AX6, int64_t BX6, int64_t CX2, int64_t DX2,
											 int32_t deltaBetaX,
											 int64_t AY6, int64_t BY6, int64_t CY2, int64_t DY2,
											 int32_t deltaBetaY,
											 int64_t dX0, int64_t dY0,
											 int64_t dXdX0, int64_t dXdY0, int64_t dYdX0, int64_t dYdY0,
											 int64_t dXdXdX0, int64_t dYdXdX0, int64_t dYdXdY0, int64_t dXdXdY0,
											 int64_t dYdYdX0, int64_t dYdYdY0,
											 int32_t betaX, int32_t betaY) {
	dX0 <<= 9;
	dY0 <<= 9;
	dXdX0 <<= 8;
	dXdY0 <<= 8;
	dYdX0 <<= 8;
	dYdY0 <<= 8;
	dXdXdX0 <<= 8;
	dYdXdX0 <<= 8;
	dYdXdY0 <<= 8;
	dXdXdY0 <<= 8;
	dYdYdX0 <<= 8;
	dYdYdY0 <<= 8;
	betaX <<= 7;
	betaY <<= 7;
	
	int32_t tOffs = 0;
	
	for(int32_t row = 0 ; row<resY ; row++)
	{
		incrementalFitting50(dXdXdX0, CX2, dXdXdX0);
		incrementalFitting50(dXdX0, dYdXdX0, DX2, dXdX0, dYdXdX0);
		
		incrementalFitting50(dX0, dYdX0, dYdYdX0, BX6, betaX, 0, dX0, dYdX0, dYdYdX0, betaX);
		
		int64_t coldXdXdX0 = dXdXdX0, coldXdX0 = dXdX0, coldX0 = dX0;
		int32_t colBetaX = betaX;
		
		incrementalFitting50(dXdXdY0, CY2, dXdXdY0);
		incrementalFitting50(dXdY0, dYdXdY0, DY2, dXdY0, dYdXdY0);
		
		incrementalFitting50(dY0, dYdY0, dYdYdY0, BY6, betaY, deltaBetaY, dY0, dYdY0, dYdYdY0, betaY);
		
		int64_t coldXdXdY0 = dXdXdY0, coldXdY0 = dXdY0, coldY0 = dY0;
		int32_t colBetaY = betaY;
		
		for(int32_t col = 0 ; col<resX ; col++, tOffs++)
		{
			RegXTable[tOffs] = coldX0 * (1.0/(1<<17));
			RegYTable[tOffs] = coldY0 * (1.0/(1<<17));
			
			incrementalFitting50(coldX0, coldXdX0, coldXdXdX0, AX6, colBetaX, deltaBetaX, coldX0, coldXdX0, coldXdXdX0, colBetaX);
			incrementalFitting50(coldY0, coldXdY0, coldXdXdY0, AY6, colBetaY, 0, coldY0, coldXdY0, coldXdXdY0, colBetaY);
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
	CreateDXDYTables(RegXTable, RegYTable,
									 nDepthXRes,	nDepthYRes,
									 GetFieldValueSigned(RegData.nRGS_AX, 32, 0),
									 GetFieldValueSigned(RegData.nRGS_BX, 32, 0),
									 GetFieldValueSigned(RegData.nRGS_CX, 32, 0),
									 GetFieldValueSigned(RegData.nRGS_DX, 32, 0),
									 GetFieldValueSigned(RegData.nRGS_DX_BETA_INC, 24, 0),
									 GetFieldValueSigned(RegData.nRGS_AY, 32, 0),
									 GetFieldValueSigned(RegData.nRGS_BY, 32, 0),
									 GetFieldValueSigned(RegData.nRGS_CY, 32, 0),
									 GetFieldValueSigned(RegData.nRGS_DY, 32, 0),
									 GetFieldValueSigned(RegData.nRGS_DY_BETA_INC, 24, 0),
									 GetFieldValueSigned(RegData.nRGS_DX_START, 19, 0),
									 GetFieldValueSigned(RegData.nRGS_DY_START, 19, 0),
									 GetFieldValueSigned(RegData.nRGS_DXDX_START, 21, 0),
									 GetFieldValueSigned(RegData.nRGS_DXDY_START, 21, 0),
									 GetFieldValueSigned(RegData.nRGS_DYDX_START, 21, 0),
									 GetFieldValueSigned(RegData.nRGS_DYDY_START, 21, 0),
									 GetFieldValueSigned(RegData.nRGS_DXDXDX_START, 27, 0),
									 GetFieldValueSigned(RegData.nRGS_DYDXDX_START, 27, 0),
									 GetFieldValueSigned(RegData.nRGS_DYDXDY_START, 27, 0),
									 GetFieldValueSigned(RegData.nRGS_DXDXDY_START, 27, 0),
									 GetFieldValueSigned(RegData.nRGS_DYDYDX_START, 27, 0),
									 GetFieldValueSigned(RegData.nRGS_DYDYDY_START, 27, 0),
									 GetFieldValueSigned(RegData.nRGS_DX_BETA_START, 17, 0),
									 GetFieldValueSigned(RegData.nRGS_DY_BETA_START, 17, 0)
									 );
	
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
		//input[i] = rawToMillimeters(depth.getPixels()[i]);
		input[i] = RawToDepth(depth[i]);
	}
	
	for(int i = 0; i < 1024; i++) {
		printf("\t%d\t%f\n",RawToDepth(i),rawToMillimeters(i));
	}

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

