/********************************* COPYRIGHT NOTICE *******************************\
 Original code for Bayer->BGR/RGB conversion is provided by Dirk Schaefer
 from MD-Mathematische Dienste GmbH. Below is the copyright notice:
 
 IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 By downloading, copying, installing or using the software you agree
 to this license. If you do not agree to this license, do not download,
 install, copy or use the software.
 
 Contributors License Agreement:
 
 Copyright (c) 2002,
 MD-Mathematische Dienste GmbH
 Im Defdahl 5-10
 44141 Dortmund
 Germany
 www.md-it.de
 
 Redistribution and use in source and binary forms,
 with or without modification, are permitted provided
 that the following conditions are met: 
 
 Redistributions of source code must retain
 the above copyright notice, this list of conditions and the following disclaimer. 
 Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution. 
 The name of Contributor may not be used to endorse or promote products
 derived from this software without specific prior written permission. 
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.
 \**********************************************************************************/

// Call this in rgb_process as Bayer2BGR( rgb_buf, 640, rgb_frame, 640*3, 640, 480);
// Depending on whether the image is used in OpenCV or Glut, "blue" (see below) needs to be -1 (OpenCV) or 1 (Glut)

#include "bayer.h"

void Bayer2BGR( const uint8_t *bayer0, int bayer_step, uint8_t *dst0, int dst_step, unsigned int width, unsigned int height)
{
    int blue = 1;
    int start_with_green = 1;
	
    memset( dst0, 0, width*3*sizeof(dst0[0]) );
    memset( dst0 + (height - 1)*dst_step, 0, width*3*sizeof(dst0[0]) );
    dst0 += dst_step + 3 + 1;
    height -= 2;
    width -= 2;
	
    for( ; height-- > 0; bayer0 += bayer_step, dst0 += dst_step )
    {
        int t0, t1;
        const uint8_t* bayer = bayer0;
        uint8_t* dst = dst0;
        const uint8_t* bayer_end = bayer + width;
		
        dst[-4] = dst[-3] = dst[-2] = dst[width*3-1] =
		dst[width*3] = dst[width*3+1] = 0;
		
        if( width <= 0 )
            continue;
		
        if( start_with_green )
        {
            t0 = (bayer[1] + bayer[bayer_step*2+1] + 1) >> 1;
            t1 = (bayer[bayer_step] + bayer[bayer_step+2] + 1) >> 1;
            dst[-blue] = (uint8_t)t0;
            dst[0] = bayer[bayer_step+1];
            dst[blue] = (uint8_t)t1;
            bayer++;
            dst += 3;
        }
		
        if( blue > 0 )
        {
            for( ; bayer <= bayer_end - 2; bayer += 2, dst += 6 )
            {
                t0 = (bayer[0] + bayer[2] + bayer[bayer_step*2] +
                      bayer[bayer_step*2+2] + 2) >> 2;
                t1 = (bayer[1] + bayer[bayer_step] +
                      bayer[bayer_step+2] + bayer[bayer_step*2+1]+2) >> 2;
                dst[-1] = (uint8_t)t0;
                dst[0] = (uint8_t)t1;
                dst[1] = bayer[bayer_step+1];
				
                t0 = (bayer[2] + bayer[bayer_step*2+2] + 1) >> 1;
                t1 = (bayer[bayer_step+1] + bayer[bayer_step+3] + 1) >> 1;
                dst[2] = (uint8_t)t0;
                dst[3] = bayer[bayer_step+2];
                dst[4] = (uint8_t)t1;
            }
        }
        else
        {
            for( ; bayer <= bayer_end - 2; bayer += 2, dst += 6 )
            {
                t0 = (bayer[0] + bayer[2] + bayer[bayer_step*2] +
                      bayer[bayer_step*2+2] + 2) >> 2;
                t1 = (bayer[1] + bayer[bayer_step] +
                      bayer[bayer_step+2] + bayer[bayer_step*2+1]+2) >> 2;
                dst[1] = (uint8_t)t0;
                dst[0] = (uint8_t)t1;
                dst[-1] = bayer[bayer_step+1];
				
                t0 = (bayer[2] + bayer[bayer_step*2+2] + 1) >> 1;
                t1 = (bayer[bayer_step+1] + bayer[bayer_step+3] + 1) >> 1;
                dst[4] = (uint8_t)t0;
                dst[3] = bayer[bayer_step+2];
                dst[2] = (uint8_t)t1;
            }
        }
		
        if( bayer < bayer_end )
        {
            t0 = (bayer[0] + bayer[2] + bayer[bayer_step*2] +
                  bayer[bayer_step*2+2] + 2) >> 2;
            t1 = (bayer[1] + bayer[bayer_step] +
                  bayer[bayer_step+2] + bayer[bayer_step*2+1]+2) >> 2;
            dst[-blue] = (uint8_t)t0;
            dst[0] = (uint8_t)t1;
            dst[blue] = bayer[bayer_step+1];
            bayer++;
            dst += 3;
        }
		
        blue = -blue;
        start_with_green = !start_with_green;
    }
}