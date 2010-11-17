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

#ifndef LIBFREENECT_H
#define LIBFREENECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t freenect_depth;
typedef uint8_t freenect_pixel;

#define FREENECT_FRAME_W 640
#define FREENECT_FRAME_H 480
#define FREENECT_FRAME_PIX (FREENECT_FRAME_H*FREENECT_FRAME_W)
#define FREENECT_RGB_SIZE (FREENECT_FRAME_PIX*3)
#define FREENECT_BAYER_SIZE (FREENECT_FRAME_PIX)
#define FREENECT_DEPTH_SIZE (FREENECT_FRAME_PIX*sizeof(freenect_depth))

typedef enum {
	FREENECT_FORMAT_RGB = 0,
	FREENECT_FORMAT_BAYER = 1,
} freenect_rgb_format;

struct _freenect_context;
typedef struct _freenect_context freenect_context;

struct _freenect_device;
typedef struct _freenect_device freenect_device;

// usb backend specific section
#include <libusb.h>
typedef libusb_context freenect_usb_context;
//

int freenect_init(freenect_context **ctx, freenect_usb_context *usb_ctx);
int freenect_shutdown(freenect_context *ctx);

int freenect_process_events(freenect_context *ctx);

int freenect_num_devices(freenect_context *ctx);
int freenect_open_device(freenect_context *ctx, freenect_device **dev, int index);
int freenect_close_device(freenect_device *dev);

void freenect_set_user(freenect_device *dev, void *user);
void *freenect_get_user(freenect_device *dev);

typedef void (*freenect_depth_cb)(freenect_device *dev, freenect_depth *depth, uint32_t timestamp);
typedef void (*freenect_rgb_cb)(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp);

void freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb);
void freenect_set_rgb_callback(freenect_device *dev, freenect_rgb_cb cb);
int freenect_set_rgb_format(freenect_device *dev, freenect_rgb_format fmt);

int freenect_start_depth(freenect_device *dev);
int freenect_start_rgb(freenect_device *dev);
int freenect_stop_depth(freenect_device *dev);
int freenect_stop_rgb(freenect_device *dev);

#ifdef __cplusplus
}
#endif

#endif
