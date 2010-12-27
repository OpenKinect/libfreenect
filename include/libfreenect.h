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

#define FREENECT_FRAME_W 640 /**< Default video frame width */
#define FREENECT_FRAME_H 480 /**< Default video frame height */
#define FREENECT_FRAME_PIX (FREENECT_FRAME_H*FREENECT_FRAME_W) /**< Pixel count for default video frame */

#define FREENECT_IR_FRAME_W 640 /**< Default depth frame width */
#define FREENECT_IR_FRAME_H 488	/**< Default depth frame height */
#define FREENECT_IR_FRAME_PIX (FREENECT_IR_FRAME_H*FREENECT_IR_FRAME_W)	/**< Pixel count for default depth frame */

#define FREENECT_VIDEO_RGB_SIZE (FREENECT_FRAME_PIX*3) /**< Size of the decompressed rgb frame */
#define FREENECT_VIDEO_BAYER_SIZE (FREENECT_FRAME_PIX) /**< Size of the bayer frame */
#define FREENECT_VIDEO_YUV_RGB_SIZE (FREENECT_VIDEO_RGB_SIZE) /**< Size of the rgb YUV frame */
#define FREENECT_VIDEO_YUV_RAW_SIZE (FREENECT_FRAME_PIX*2) /**< Size of the raw YUV frame */
#define FREENECT_VIDEO_IR_8BIT_SIZE (FREENECT_IR_FRAME_PIX)	/**< Size of the 8 bit IR video frame */
#define FREENECT_VIDEO_IR_10BIT_SIZE (FREENECT_IR_FRAME_PIX*sizeof(uint16_t)) /**< Size of the 10 bit IR video frame */
#define FREENECT_VIDEO_IR_10BIT_PACKED_SIZE 390400 /**< Size of the packed 10 bit IR video frame */

#define FREENECT_DEPTH_11BIT_SIZE (FREENECT_FRAME_PIX*sizeof(uint16_t)) /**< Size of the 11 bit depth frame */
#define FREENECT_DEPTH_10BIT_SIZE FREENECT_DEPTH_11BIT_SIZE	/**< Size of the 10 bit depth frame */
#define FREENECT_DEPTH_11BIT_PACKED_SIZE 422400	/**< Size of the packed 11 bit depth frame */
#define FREENECT_DEPTH_10BIT_PACKED_SIZE 384000	/**< Size of the packed 10 bit depth frame */

#define FREENECT_COUNTS_PER_G 819 /**< Ticks per G for accelerometer as set per http://www.kionix.com/Product%20Sheets/KXSD9%20Product%20Brief.pdf */

/// Enumeration of video frame information states.
/// See http://openkinect.org/wiki/Protocol_Documentation#RGB_Camera for more information.
typedef enum {
	FREENECT_VIDEO_RGB             = 0, /**< Decompressed RGB mode (demosaicing done by libfreenect) */
	FREENECT_VIDEO_BAYER           = 1, /**< Bayer compressed mode (raw information from camera) */
	FREENECT_VIDEO_IR_8BIT         = 2, /**< 8-bit IR mode  */
	FREENECT_VIDEO_IR_10BIT        = 3, /**< 10-bit IR mode */
	FREENECT_VIDEO_IR_10BIT_PACKED = 4, /**< 10-bit packed IR mode */
	FREENECT_VIDEO_YUV_RGB         = 5, /**< YUV RGB mode */
	FREENECT_VIDEO_YUV_RAW         = 6, /**< YUV Raw mode */
} freenect_video_format;

/// Enumeration of LED states
/// See http://openkinect.org/wiki/Protocol_Documentation#Setting_LED for more information.
typedef enum {
	LED_OFF              = 0, /**< Turn LED off */
	LED_GREEN            = 1, /**< Turn LED to Green */
	LED_RED              = 2, /**< Turn LED to Red */
	LED_YELLOW           = 3, /**< Turn LED to Yellow */
	LED_BLINK_GREEN      = 4, /**< Make LED blink Green */
	// 5 is same as 4, LED blink Green
	LED_BLINK_RED_YELLOW = 6, /**< Make LED blink Red/Yellow */
} freenect_led_options;

/// Enumeration of depth frame states
/// See http://openkinect.org/wiki/Protocol_Documentation#RGB_Camera for more information.
typedef enum {
	FREENECT_DEPTH_11BIT        = 0, /**< 11 bit depth information in one uint16_t/pixel */
	FREENECT_DEPTH_10BIT        = 1, /**< 10 bit depth information in one uint16_t/pixel */
	FREENECT_DEPTH_11BIT_PACKED = 2, /**< 11 bit packed depth information */
	FREENECT_DEPTH_10BIT_PACKED = 3, /**< 10 bit packed depth information */
} freenect_depth_format;


/// Enumeration of tilt motor status
typedef enum {
	TILT_STATUS_STOPPED = 0x00, /**< Tilt motor is stopped */
	TILT_STATUS_LIMIT   = 0x01, /**< Tilt motor has reached movement limit */
	TILT_STATUS_MOVING  = 0x04, /**< Tilt motor is currently moving to new position */
} freenect_tilt_status_code;

/// Data from the tilt motor and accelerometer
typedef struct {
	int16_t                   accelerometer_x; /**< Raw accelerometer data for X-axis, see FREENECT_COUNTS_PER_G for conversion */
	int16_t                   accelerometer_y; /**< Raw accelerometer data for Y-axis, see FREENECT_COUNTS_PER_G for conversion */
	int16_t                   accelerometer_z; /**< Raw accelerometer data for Z-axis, see FREENECT_COUNTS_PER_G for conversion */
	int8_t                    tilt_angle;      /**< Raw tilt motor angle encoder information */
	freenect_tilt_status_code tilt_status;     /**< State of the tilt motor (stopped, moving, etc...) */
} freenect_raw_tilt_state;

struct _freenect_context;
typedef struct _freenect_context freenect_context; /**< Holds information about the usb context. */

struct _freenect_device;
typedef struct _freenect_device freenect_device; /**< Holds device information. */

// usb backend specific section
#ifdef _WIN32
  /* frees Windows users of the burden of specifying the path to <libusb-1.0/libusb.h> */
  typedef void freenect_usb_context;
#else
  #include <libusb-1.0/libusb.h>
  typedef libusb_context freenect_usb_context; /**< Holds libusb-1.0 specific information */
#endif
//

/// If Win32, export all functions for DLL usage
#ifndef _WIN32
  #define FREENECTAPI /**< DLLExport information for windows, set to nothing on other platforms */
#else
  /**< DLLExport information for windows, set to nothing on other platforms */
  #ifdef __cplusplus
    #define FREENECTAPI extern "C" __declspec(dllexport)
  #else
    // this is required when building from a Win32 port of gcc without being
    // forced to compile all of the library files (.c) with g++...
    #define FREENECTAPI __declspec(dllexport)
  #endif
#endif

/// Enumeration of message logging levels
typedef enum {
	FREENECT_LOG_FATAL = 0,     /**< Log for crashing/non-recoverable errors */
	FREENECT_LOG_ERROR,         /**< Log for major errors */
	FREENECT_LOG_WARNING,       /**< Log for warning messages */
	FREENECT_LOG_NOTICE,        /**< Log for important messages */
	FREENECT_LOG_INFO,          /**< Log for normal messages */
	FREENECT_LOG_DEBUG,         /**< Log for useful development messages */
	FREENECT_LOG_SPEW,          /**< Log for slightly less useful messages */
	FREENECT_LOG_FLOOD,         /**< Log EVERYTHING. May slow performance. */
} freenect_loglevel;

/**
 * Initialize a freenect context and do any setup required for
 * platform specific USB libraries.
 *
 * @param ctx Address of pointer to freenect context struct to allocate and initialize
 * @param usb_ctx USB context to initialize. Can be NULL if not using multiple contexts.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_init(freenect_context **ctx, freenect_usb_context *usb_ctx);

/**
 * Closes the device if it is open, and frees the context
 *
 * @param ctx freenect context to close/free
 *
 * @return 0 on success
 */
FREENECTAPI int freenect_shutdown(freenect_context *ctx);

/// Typedef for logging callback functions
typedef void (*freenect_log_cb)(freenect_context *dev, freenect_loglevel level, const char *msg);

/**
 * Set the log level for the specified freenect context
 *
 * @param ctx context to set log level for
 * @param level log level to use (see freenect_loglevel enum)
 */
FREENECTAPI void freenect_set_log_level(freenect_context *ctx, freenect_loglevel level);

/**
 * Callback for log messages (i.e. for rerouting to a file instead of
 * stdout)
 *
 * @param ctx context to set log callback for
 * @param cb callback function pointer
 */
FREENECTAPI void freenect_set_log_callback(freenect_context *ctx, freenect_log_cb cb);

/**
 * Calls the platform specific usb event processor
 *
 * @param ctx context to process events for
 *
 * @return 0 on success, other values on error, platform/library dependant
 */
FREENECTAPI int freenect_process_events(freenect_context *ctx);

/**
 * Return the number of kinect devices currently connected to the
 * system
 *
 * @param ctx Context to access device count through
 *
 * @return Number of devices connected, < 0 on error
 */
FREENECTAPI int freenect_num_devices(freenect_context *ctx);

/**
 * Opens a kinect device via a context. Index specifies the index of
 * the device on the current state of the bus. Bus resets may cause
 * indexes to shift.
 *
 * @param ctx Context to open device through
 * @param dev Device structure to assign opened device to
 * @param index Index of the device on the bus
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_open_device(freenect_context *ctx, freenect_device **dev, int index);

/**
 * Closes a device that is currently open
 *
 * @param dev Device to close
 *
 * @return 0 on success
 */
FREENECTAPI int freenect_close_device(freenect_device *dev);

/**
 * Set the device user data, for passing generic information into
 * callbacks
 *
 * @param dev Device to attach user data to
 * @param user User data to attach
 */
FREENECTAPI void freenect_set_user(freenect_device *dev, void *user);

/**
 * Retrieve the pointer to user data from the device struct
 *
 * @param dev Device from which to get user data
 *
 * @return Pointer to user data
 */
FREENECTAPI void *freenect_get_user(freenect_device *dev);

/// Typedef for depth image received event callbacks
typedef void (*freenect_depth_cb)(freenect_device *dev, void *depth, uint32_t timestamp);
/// Typedef for video image received event callbacks
typedef void (*freenect_video_cb)(freenect_device *dev, void *video, uint32_t timestamp);

/**
 * Set callback for depth information received event
 *
 * @param dev Device to set callback for
 * @param cb Function pointer for processing depth information
 */
FREENECTAPI void freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb);

/**
 * Set callback for video information received event
 *
 * @param dev Device to set callback for
 * @param cb Function pointer for processing video information
 */
FREENECTAPI void freenect_set_video_callback(freenect_device *dev, freenect_video_cb cb);

/**
 * Set the format for depth information
 *
 * @param dev Device to set depth information format for
 * @param fmt Format of depth information. See freenect_depth_format enum.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_set_depth_format(freenect_device *dev, freenect_depth_format fmt);

/**
 * Set the format for video information
 *
 * @param dev Device to set video information format for
 * @param fmt Format of video information. See freenect_video_format enum.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_set_video_format(freenect_device *dev, freenect_video_format fmt);

/**
 * Set the buffer to store depth information to. Size of buffer is
 * dependant on depth format. See FREENECT_DEPTH_*_SIZE defines for
 * more information.
 *
 * @param dev Device to set depth buffer for.
 * @param buf Buffer to store depth information to.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_set_depth_buffer(freenect_device *dev, void *buf);

/**
 * Set the buffer to store depth information to. Size of buffer is
 * dependant on video format. See FREENECT_VIDEO_*_SIZE defines for
 * more information.
 *
 * @param dev Device to set video buffer for.
 * @param buf Buffer to store video information to.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_set_video_buffer(freenect_device *dev, void *buf);

/**
 * Start the depth information stream for a device.
 *
 * @param dev Device to start depth information stream for.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_start_depth(freenect_device *dev);

/**
 * Start the video information stream for a device.
 *
 * @param dev Device to start video information stream for.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_start_video(freenect_device *dev);

/**
 * Stop the depth information stream for a device
 *
 * @param dev Device to stop depth information stream on.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_stop_depth(freenect_device *dev);

/**
 * Stop the video information stream for a device
 *
 * @param dev Device to stop video information stream on.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_stop_video(freenect_device *dev);

/**
 * Updates the accelerometer state using a blocking control message
 * call.
 *
 * @param dev Device to get accelerometer data from
 *
 * @return 0 on success, < 0 on error. Accelerometer data stored to
 * device struct.
 */
FREENECTAPI int freenect_update_tilt_state(freenect_device *dev);

/**
 * Retrieve the tilt state from a device
 *
 * @param dev Device to retrieve tilt state from
 *
 * @return The tilt state struct of the device
 */
FREENECTAPI freenect_raw_tilt_state* freenect_get_tilt_state(freenect_device *dev);

/**
 * Return the tilt state, in degrees with respect to the horizon
 *
 * @param state The tilt state struct from a device
 *
 * @return Current degree of tilt of the device
 */
FREENECTAPI double freenect_get_tilt_degs(freenect_raw_tilt_state *state);

/**
 * Set the tilt state of the device, in degrees with respect to the
 * horizon. Uses blocking control message call to update
 * device. Function return does not reflect state of device, device
 * may still be moving to new position after the function returns. Use
 * freenect_get_tilt_status() to find current movement state.
 *
 * @param dev Device to set tilt state
 * @param angle Angle the device should tilt to
 *
 * @return 0 on success, < 0 on error.
 */
FREENECTAPI int freenect_set_tilt_degs(freenect_device *dev, double angle);

/**
 * Return the movement state of the tilt motor (moving, stopped, etc...)
 *
 * @param state Raw state struct to get the tilt status code from
 *
 * @return Status code of the tilt device. See
 * freenect_tilt_status_code enum for more info.
 */
FREENECTAPI freenect_tilt_status_code freenect_get_tilt_status(freenect_raw_tilt_state *state);

/**
 * Set the state of the LED. Uses blocking control message call to
 * update device.
 *
 * @param dev Device to set the LED state
 * @param option LED state to set on device. See freenect_led_options enum.
 *
 * @return 0 on success, < 0 on error
 */
FREENECTAPI int freenect_set_led(freenect_device *dev, freenect_led_options option);

/**
 * Get the axis-based gravity adjusted accelerometer state, as laid
 * out via the accelerometer data sheet, which is available at
 *
 * http://www.kionix.com/Product%20Sheets/KXSD9%20Product%20Brief.pdf
 *
 * @param state State to extract accelerometer data from
 * @param x Stores X-axis accelerometer state
 * @param y Stores Y-axis accelerometer state
 * @param z Stores Z-axis accelerometer state
 */
FREENECTAPI void freenect_get_mks_accel(freenect_raw_tilt_state *state, double* x, double* y, double* z);

#ifdef __cplusplus
}
#endif

#endif //

