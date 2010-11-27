/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 Brandyn White (bwhite@dappervision.com)
 *                    Andrew Miller (amiller@dappervision.com)
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
