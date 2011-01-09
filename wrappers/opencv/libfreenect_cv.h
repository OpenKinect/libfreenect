#ifndef FREENECT_CV_H
#define FREENECT_CV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libfreenect_sync.h"
#include <opencv/cv.h>

void freenect_sync_get_depth_cv(IplImage *image, int index);
void freenect_sync_get_rgb_cv(IplImage *image, int index);

#ifdef __cplusplus
}
#endif

#endif