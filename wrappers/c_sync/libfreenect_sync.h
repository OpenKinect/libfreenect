#ifndef FREENECT_SYNC_H
#define FREENECT_SYNC_H

int freenect_sync_get_rgb(char **rgb, uint32_t *timestamp);
/*  Synchronous rgb function, starts the runloop if it isn't running

    Args:
        rgb: Populated with a pointer to a RGB buffer (of size RGB_BYTES)
        timestamp: Populated with a pointer to the associated timestamp

    Returns:
        Nonzero on error.
*/
int freenect_sync_get_depth(char **depth, uint32_t *timestamp);
/*  Synchronous depth function, starts the runloop if it isn't running

    Args:
        depth: Populated with a pointer to a depth buffer (of size DEPTH_BYTES)
        timestamp: Populated with a pointer to the associated timestamp

    Returns:
        Nonzero on error.
*/
void freenect_sync_stop();
#endif
