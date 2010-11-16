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
	fnusb_ctx usb;
};

#define DEPTH_RAW_SIZE 422400
#define FRAME_H FREENECT_FRAME_H
#define FRAME_W FREENECT_FRAME_W
#define FRAME_PIX FREENECT_FRAME_PIX

#define DEPTH_PKTSIZE 1760
#define RGB_PKTSIZE 1920

#define DEPTH_PKTDSIZE (DEPTH_PKTSIZE-12)
#define RGB_PKTDSIZE (RGB_PKTSIZE-12)

#define DEPTH_PKTS_PER_FRAME ((DEPTH_RAW_SIZE+DEPTH_PKTDSIZE-1)/DEPTH_PKTDSIZE)
#define RGB_PKTS_PER_FRAME ((FRAME_PIX+RGB_PKTDSIZE-1)/RGB_PKTDSIZE)

#define inf(...) \
	do { \
		fprintf(stderr, "INF: "); \
		fprintf(stderr, __VA_ARGS__); \
	} while(0)

#define wrn(...) \
	do { \
		fprintf(stderr, "WRN: "); \
		fprintf(stderr, __VA_ARGS__); \
	} while(0)

#define err(...) \
	do { \
		fprintf(stderr, "ERR: "); \
		fprintf(stderr, __VA_ARGS__); \
	} while(0)

#ifdef LIBFREENECT_DBG
#define dbg(...) \
	do { \
		fprintf(stderr, "DBG: "); \
		fprintf(stderr, __VA_ARGS__); \
	} while(0)
#else
#define dbg(...)
#endif

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

	int cam_inited;

	packet_stream depth_stream;
	uint8_t depth_raw[DEPTH_RAW_SIZE];
	uint16_t depth_frame[FRAME_PIX];

	packet_stream rgb_stream;
	uint8_t rgb_raw[FRAME_PIX];
	uint8_t rgb_frame[3*FRAME_PIX];

	// Audio
	// Motor
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
