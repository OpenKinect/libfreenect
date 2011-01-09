#include "libfreenect_cv.h"

void freenect_sync_get_depth_cv(IplImage *image, int index)
{
	void *depth_data = 0;
	unsigned int timestamp;
	freenect_sync_get_depth(&depth_data, &timestamp, index, FREENECT_DEPTH_11BIT);
	unsigned char *depth = (unsigned char*) depth_data;
	cvSetData(image, depth, FREENECT_FRAME_W * 2);
}

void freenect_sync_get_rgb_cv(IplImage *image, int index)
{
	void *video_data = 0;
	unsigned int timestamp;
	freenect_sync_get_video(&video_data, &timestamp, index, FREENECT_VIDEO_RGB);
	unsigned char *video = (unsigned char*) video_data;
	cvSetData(image, video, FREENECT_FRAME_W * 3);
}
