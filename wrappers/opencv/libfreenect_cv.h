#pragma once

#include <opencv/cv.h>

#ifdef __cplusplus
extern "C" {
#endif

	IplImage *freenect_sync_get_depth_cv(int index);
	IplImage *freenect_sync_get_rgb_cv(int index);

#ifdef __cplusplus
}
#endif
