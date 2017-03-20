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

#ifndef WIN32
#include <stdint.h>
#else
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned int ssize_t;
#endif

typedef uint16_t freenect_depth;
typedef uint8_t freenect_pixel;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#define STRUCT
#else
#define STRUCT struct
#endif

#define FREENECT_FRAME_W 640
#define FREENECT_FRAME_H 480
#define FREENECT_FRAME_PIX (FREENECT_FRAME_H*FREENECT_FRAME_W)
#define FREENECT_RGB_SIZE (FREENECT_FRAME_PIX*3)
#define FREENECT_BAYER_SIZE (FREENECT_FRAME_PIX)
#define FREENECT_DEPTH_SIZE (FREENECT_FRAME_PIX*sizeof(freenect_depth))
#define FREENECT_COUNTS_PER_G 819

typedef enum {
	FREENECT_FORMAT_RGB = 0,
	FREENECT_FORMAT_BAYER = 1,
} freenect_rgb_format;

typedef enum {
	LED_OFF    = 0,
	LED_GREEN  = 1,
	LED_RED    = 2,
	LED_YELLOW = 3,
	LED_BLINK_YELLOW = 4,
	LED_BLINK_GREEN = 5,
	LED_BLINK_RED_YELLOW = 6
} freenect_led_options;

typedef enum {
	FREENECT_FORMAT_11_BIT = 0,
	FREENECT_FORMAT_10_BIT = 1
} freenect_depth_format;

struct _freenect_context;
typedef struct _freenect_context freenect_context;

struct _freenect_device;
typedef struct _freenect_device freenect_device;

#ifndef WIN32
#define EXPORT
// usb backend specific section
#include <libusb.h>
typedef libusb_context* freenect_usb_context;
//
#else
#define EXPORT __declspec(dllexport) 
typedef void* freenect_usb_context;
#endif

typedef enum {
	FREENECT_LOG_FATAL = 0,
	FREENECT_LOG_ERROR,
	FREENECT_LOG_WARNING,
	FREENECT_LOG_NOTICE,
	FREENECT_LOG_INFO,
	FREENECT_LOG_DEBUG,
	FREENECT_LOG_SPEW,
	FREENECT_LOG_FLOOD,
} freenect_loglevel;

EXPORT int freenect_init(freenect_context **ctx, freenect_usb_context *usb_ctx);

EXPORT int freenect_shutdown(freenect_context *ctx);

typedef void (*freenect_log_cb)(freenect_context *dev, freenect_loglevel level, const char *msg);

EXPORT void freenect_set_log_level(freenect_context *ctx, freenect_loglevel level);
EXPORT void freenect_set_log_callback(freenect_context *ctx, freenect_log_cb cb);

EXPORT int freenect_process_events(freenect_context *ctx);

EXPORT int freenect_num_devices(freenect_context *ctx);
EXPORT int freenect_open_device(freenect_context *ctx, freenect_device **dev, int index);
EXPORT int freenect_close_device(freenect_device *dev);

EXPORT void freenect_set_user(freenect_device *dev, void *user);
EXPORT void *freenect_get_user(freenect_device *dev);

typedef void (*freenect_depth_cb)(freenect_device *dev, freenect_depth *depth, uint32_t timestamp);
typedef void (*freenect_rgb_cb)(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp);

EXPORT void freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb);
EXPORT void freenect_set_rgb_callback(freenect_device *dev, freenect_rgb_cb cb);
EXPORT int freenect_set_rgb_format(freenect_device *dev, freenect_rgb_format fmt);
EXPORT int freenect_set_depth_format(freenect_device *dev, freenect_depth_format fmt);

EXPORT int freenect_start_depth(freenect_device *dev);
EXPORT int freenect_start_rgb(freenect_device *dev);
EXPORT int freenect_stop_depth(freenect_device *dev);
EXPORT int freenect_stop_rgb(freenect_device *dev);

EXPORT int freenect_set_tilt_degs(freenect_device *dev, double angle);
EXPORT int freenect_set_led(freenect_device *dev, freenect_led_options option);

EXPORT int freenect_get_raw_accel(freenect_device *dev, int16_t* x, int16_t* y, int16_t* z);
EXPORT int freenect_get_mks_accel(freenect_device *dev, double* x, double* y, double* z);

#ifdef __cplusplus
}
#endif

#endif //

