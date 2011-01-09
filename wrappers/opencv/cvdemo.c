#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <stdio.h>
#include "libfreenect_cv.h"

IplImage *GlViewColor(IplImage *depth)
{
	static IplImage *image = 0;
	if (!image) image = cvCreateImage(cvSize(640,480), 8, 3);
	unsigned char *depth_mid = (unsigned char *)image->imageData;
	int i;
	for (i = 0; i < 640*480; i++) {
		int lb = ((short *)depth->imageData)[i] % 256;
		int ub = ((short *)depth->imageData)[i] / 256;
		switch (ub) {
			case 0:
				depth_mid[3*i+2] = 255;
				depth_mid[3*i+1] = 255-lb;
				depth_mid[3*i+0] = 255-lb;
				break;
			case 1:
				depth_mid[3*i+2] = 255;
				depth_mid[3*i+1] = lb;
				depth_mid[3*i+0] = 0;
				break;
			case 2:
				depth_mid[3*i+2] = 255-lb;
				depth_mid[3*i+1] = 255;
				depth_mid[3*i+0] = 0;
				break;
			case 3:
				depth_mid[3*i+2] = 0;
				depth_mid[3*i+1] = 255;
				depth_mid[3*i+0] = lb;
				break;
			case 4:
				depth_mid[3*i+2] = 0;
				depth_mid[3*i+1] = 255-lb;
				depth_mid[3*i+0] = 255;
				break;
			case 5:
				depth_mid[3*i+2] = 0;
				depth_mid[3*i+1] = 0;
				depth_mid[3*i+0] = 255-lb;
				break;
			default:
				depth_mid[3*i+2] = 0;
				depth_mid[3*i+1] = 0;
				depth_mid[3*i+0] = 0;
				break;
		}
	}
	return image;
}

int main(int argc, char **argv)
{
	IplImage *video_image = cvCreateImageHeader(cvSize(FREENECT_FRAME_W, FREENECT_FRAME_H), 8, 3);
	IplImage *depth_image = cvCreateImageHeader(cvSize(FREENECT_FRAME_W, FREENECT_FRAME_H), 16, 1);
	
	while (cvWaitKey(1000/30) < 0) {
		freenect_sync_get_rgb_cv(video_image, 0);
		if (!video_image) {
		    printf("Error: Kinect not connected?\n");
		    return -1;
		}
		cvCvtColor(video_image, video_image, CV_RGB2BGR);
		
		freenect_sync_get_depth_cv(depth_image, 0);
		if (!depth_image) {
		    printf("Error: Kinect not connected?\n");
		    return -1;
		}
		cvShowImage("RGB", video_image);
		cvShowImage("Depth", GlViewColor(depth_image));
	}
	return 0;
}
