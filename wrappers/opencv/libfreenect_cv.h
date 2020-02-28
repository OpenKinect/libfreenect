#pragma once

#include <opencv2/core/core_c.h>

#ifdef __cplusplus
extern "C" {
#endif

	IplImage *freenect_sync_get_depth_cv(int index);
	IplImage *freenect_sync_get_rgb_cv(int index);

#ifdef __cplusplus
}
#endif
