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

#ifndef FREENECT_INTERNAL_H
#define FREENECT_INTERNAL_H

#include <stdint.h>

#include "libfreenect.h"

typedef void (*fnusb_iso_cb)(freenect_device *dev, uint8_t *buf, int len);

#include "usb_libusb10.h"

struct _freenect_context {
	freenect_loglevel log_level;
	freenect_log_cb log_cb;
	fnusb_ctx usb;
};

#define LL_FATAL FREENECT_LOG_FATAL
#define LL_ERROR FREENECT_LOG_ERROR
#define LL_WARNING FREENECT_LOG_WARNING
#define LL_NOTICE FREENECT_LOG_NOTICE
#define LL_INFO FREENECT_LOG_INFO
#define LL_DEBUG FREENECT_LOG_DEBUG
#define LL_SPEW FREENECT_LOG_SPEW
#define LL_FLOOD FREENECT_LOG_FLOOD

void fn_log(freenect_context *ctx, freenect_loglevel level, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));

#define FN_LOG(level, ...) fn_log(ctx, level, __VA_ARGS__)

#define FN_FATAL(...) FN_LOG(LL_FATAL, __VA_ARGS__)
#define FN_ERROR(...) FN_LOG(LL_ERROR, __VA_ARGS__)
#define FN_WARNING(...) FN_LOG(LL_WARNING, __VA_ARGS__)
#define FN_NOTICE(...) FN_LOG(LL_NOTICE, __VA_ARGS__)
#define FN_INFO(...) FN_LOG(LL_INFO, __VA_ARGS__)
#define FN_DEBUG(...) FN_LOG(LL_DEBUG, __VA_ARGS__)
#define FN_SPEW(...) FN_LOG(LL_SPEW, __VA_ARGS__)
#define FN_FLOOD(...) FN_LOG(LL_FLOOD, __VA_ARGS__)

#define DEPTH_RAW_10_BIT_SIZE 384000
#define DEPTH_RAW_11_BIT_SIZE 422400
#define FRAME_H FREENECT_FRAME_H
#define FRAME_W FREENECT_FRAME_W
#define FRAME_PIX FREENECT_FRAME_PIX

#define DEPTH_PKTSIZE 1760
#define RGB_PKTSIZE 1920

#define DEPTH_PKTDSIZE (DEPTH_PKTSIZE-12)
#define RGB_PKTDSIZE (RGB_PKTSIZE-12)

#define DEPTH_PKTS_10_BIT_PER_FRAME ((DEPTH_RAW_10_BIT_SIZE+DEPTH_PKTDSIZE-1)/DEPTH_PKTDSIZE)
#define DEPTH_PKTS_11_BIT_PER_FRAME ((DEPTH_RAW_11_BIT_SIZE+DEPTH_PKTDSIZE-1)/DEPTH_PKTDSIZE)
#define RGB_PKTS_PER_FRAME ((FRAME_PIX+RGB_PKTDSIZE-1)/RGB_PKTDSIZE)

#define MS_MAGIC_VENDOR 0x45e
#define MS_MAGIC_CAMERA_PRODUCT 0x02ae
#define MS_MAGIC_MOTOR_PRODUCT 0x02b0

typedef struct {
	uint8_t flag;
	int synced;
	uint8_t seq;
	int got_pkts;
	int pkt_num;
	int pkts_per_frame;
	int pkt_size;
	int valid_pkts;
	uint32_t last_timestamp;
	uint32_t timestamp;
	uint8_t *buf;
} packet_stream;

struct _freenect_device {
	freenect_context *parent;
	void *user_data;

	// Cameras
	fnusb_dev usb_cam;
	fnusb_isoc_stream depth_isoc;
	fnusb_isoc_stream rgb_isoc;

	freenect_depth_cb depth_cb;
	freenect_rgb_cb rgb_cb;
	freenect_rgb_format rgb_format;
	freenect_depth_format depth_format;

	int cam_inited;

	packet_stream depth_stream;
	uint8_t depth_raw[DEPTH_RAW_11_BIT_SIZE];
	uint16_t depth_frame[FRAME_PIX];

	packet_stream rgb_stream;
	uint8_t rgb_raw[FRAME_PIX];
	uint8_t rgb_frame[3*FRAME_PIX];

	// Audio
	// Motor
	fnusb_dev usb_motor;

};

struct caminit {
	uint16_t command;
	uint16_t tag;
	int cmdlen;
	int replylen;
	uint8_t cmddata[1024];
	uint8_t replydata[1024];
};

#endif
