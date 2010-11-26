#ifndef FREENECT_SYNC_H
#define FREENECT_SYNC_H

#define DEPTH_BYTES 614400 // 480 * 640 * 2
#define RGB_BYTES 921600  // 480 * 640 * 3
char *freenect_get_depth();
char *freenect_get_rgb();
void freenect_sync_stop();

#endif
