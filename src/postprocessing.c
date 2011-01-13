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
#include <math.h>

#include "freenect_internal.h"

uint8_t interpolate(uint8_t* bufin, float x, float y, int height, int width){
	uint8_t p1[3], p2[3], p3[3], p4[3]; /* 3 Channels */

	int i1,j1,i2,j2;
	i1 = x;
	j1 = y;
	i2 = i1 +1;
	j2 = j1 +1;

	/* die punkte pN muessen aber die pixelwerte enthalten... */
	p1[0] = bufin[(j1*640+i1)*3+0];
	p1[1] = bufin[(j1*640+i1)*3+1];
	p1[2] = bufin[(j1*640+i1)*3+2];

	if(i2 < width){
		p2[0] = bufin[(j1*640+i1)*3+0];
		p2[1] = bufin[(j1*640+i1)*3+1];
		p2[2] = bufin[(j1*640+i1)*3+2];
	}
	else *p2 = *p1;
	if(j2<height){
		p3[0] = bufin[(j2*640+i1)*3+0];
		p3[1] = bufin[(j2*640+i1)*3+1];
		p3[2] = bufin[(j2*640+i1)*3+2];
		if(i2 < width){
			p4[0] = bufin[(j2*640+i2)*3+0];
			p4[1] = bufin[(j2*640+i2)*3+1];
			p4[2] = bufin[(j2*640+i2)*3+2];
		}
		else *p4 = *p1;
	}
	else {
		*p3 = *p1;
		*p4 = *p1;
	}
	return *p3;
}

void freenect_init_undistort_map_video(freenect_device* dev, freenect_param_undistort* dist_coeffs, freenect_param_intrinsic* intr, int inverse)
{
	freenect_calculate_undistort_map(dist_coeffs, intr, &dev->map_video, inverse);
}

void freenect_init_undistort_map_depth(freenect_device* dev, freenect_param_undistort* dist_coeffs, freenect_param_intrinsic* intr, int inverse)
{
	freenect_calculate_undistort_map(dist_coeffs, intr, &dev->map_depth, inverse);
}

void freenect_calculate_undistort_map(freenect_param_undistort* dist_coeffs, freenect_param_intrinsic* intr, freenect_undistort_map* map, int inverse)
{
	int i, j, width=640, height=480;
	float xsq=0, x=0, ysq=0, y=0;
	float xx=0, yy=0;
	float uu=0, vv=0, u=0, v=0;
	int ii=0, jj=0;
	float rsq=0;
	float prefix = inverse == FREENECT_MAP_INVERSE ? -1 : 1 ;
	float k1 = prefix * dist_coeffs->k1;
	float k2 = prefix * dist_coeffs->k2;
	float k3 = prefix * dist_coeffs->k3;
	float p1 = prefix * dist_coeffs->p1;
	float p2 = prefix * dist_coeffs->p2;
	float fx = intr->fx;
	float fy = intr->fy;
	float cx = intr->cx;
	float cy = intr->cy;
	for(j=0; j<height; j++){
		for(i=0; i<width; i++){
			u = i - cx;
			v = j - cy;
			x = u/fx;
			y = v/fy;
			xsq = x*x;
			ysq = y*y;
			rsq =  xsq + ysq;
			xx = x*(1+k1*rsq + k2*rsq*rsq + k3*rsq*rsq*rsq) + 2*p1*x*y + p2*(rsq + 2*xsq);
			yy = y*(1+k1*rsq + k2*rsq*rsq + k3*rsq*rsq*rsq) + p1*(rsq + 2*ysq) + 2*p2*x*y;
			uu = xx*fx;
			ii = uu+cx+0.5;
			vv = yy*fy;
			jj = vv+cy+0.5;
			map->u[j*640+i] = ii;
			map->v[j*640+i] = jj;
		}
	}
}

// this function undistorts the actual buffer according to the
// map  freenect_device::map_u and map_v
void freenect_undistort_video(freenect_device *dev, void* _bufin, void* _bufout)
{
	uint8_t *bufin = (uint8_t*) _bufin;
	uint8_t *bufout = (uint8_t*) _bufout;
	int i, j, width=640, height=480;
	int i1, j1, i2, j2;
	for(j=0; j<height; j++){
		for(i=0; i<width; i++){
			i1 = dev->map_video.u[j*640+i];
			j1 = dev->map_video.v[j*640+i];
			i2 = i1 +1;
			j2 = j1 +1;
			if(i1 >=0 && i1 <= 640 && j1 >= 0 && j1 <= 480){ /* check if not out of bounds */
				bufout[(j*640+i)*3+0] = bufin[(j1*640+i1)*3 + 0]; /*r*/
				bufout[(j*640+i)*3+1] = bufin[(j1*640+i1)*3 + 1]; /*g*/
				bufout[(j*640+i)*3+2] = bufin[(j1*640+i1)*3 + 2]; /*b*/
			}
		}
	}
}

void freenect_undistort_depth(freenect_device *dev, void* _bufin, void* _bufout)
{
	uint16_t *bufin = (uint16_t*) _bufin;
	uint16_t *bufout = (uint16_t*) _bufout;
	int i, j, width=640, height=480;
	int i1, j1, i2, j2;
	for(j=0; j<height; j++){
		for(i=0; i<width; i++){
			i1 = dev->map_depth.u[j*640+i];
			j1 = dev->map_depth.v[j*640+i];
			i2 = i1 +1;
			j2 = j1 +1;
			if(i1 >=0 && i1 < 640 && j1 >= 0 && j1 < 480){ // check if not out of bounds
				bufout[j*640+i] = bufin[j1*640+i1]; // depth
			}
		}
	}
}

/** intrinsic matrix:
 *   fx,   0,  cx
 *    0,  fy,  cy
 *    0,   0,   1
 */

/** extrinsic matrix:
 *   r11  r12  r13
 *   r21  r22  r23
 *   t31  r32  r33
 */

/** translational vector
 * t1
 * t2
 * t3
 */

/*
void freenect_init_projection_matrix(freenect_device *dev, freenect_param_intrinsic* intr, freenect_param_extrinsic* extr)
{
	// zeile mal spalte
	dev->pm.p11 = intr->fx * extr->r11 + intr->cx * extr->r31;
	dev->pm.p12 = intr->fx * extr->r12 + intr->cx * extr->r32;
	dev->pm.p13 = intr->fx * extr->r13 + intr->cx * extr->r33;
	dev->pm.p14 = intr->fx * extr->t1  + intr->cx * extr->t3;
	dev->pm.p21 = intr->fy * extr->r21 + intr->cx * extr->r31;
	dev->pm.p22 = intr->fy * extr->r22 + intr->cx * extr->r32;
	dev->pm.p23 = intr->fy * extr->r23 + intr->cx * extr->r33;
	dev->pm.p24 = intr->fy * extr->t1  + intr->cx * extr->t3;
	dev->pm.p31 = extr->r31;
	dev->pm.p32 = extr->r32;
	dev->pm.p33 = extr->r33;
	dev->pm.p34 = extr->t3;
}*/

//void freenect_xyz2uv(float* in, int* out, freenect_projection_matrix* pm){
	/* zeile mal spalte
	 * out[0] = u      in[0] = X
	 * out[1] = v      in[1] = Y
	 *                 in[2] = Z
	 */		
//	out[0] = pm->p11*in[0] + pm->p12*in[1] + pm->p13*in[2];
//	out[1] = pm->p21*in[0] + pm->p22*in[1] + pm->p23*in[2];
//}


//void freenect_uv2xyz(int* in, float* out, freenect_projection_matrix* pm)
//{
//}

