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

#ifndef USB_LIBUSB10
#define USB_LIBUSB10

#include <libusb.h>

#if defined(__APPLE__)
/* OS X likes things on powers of 2 boundaries, and needs WAY larger
   packets/xfers to deal with what it's getting. Feel free to play
   with these numbers, but keep them in multiples of 8, otherwise you
   may get weirdness only seen with USB Prober on level 4 or
   higher. */
#define PKTS_PER_XFER 512
#define NUM_XFERS 64
#define DEPTH_PKTBUF 2048
#define RGB_PKTBUF 2048
#else
#define PKTS_PER_XFER 16
#define NUM_XFERS 16
#define DEPTH_PKTBUF 1920
#define RGB_PKTBUF 1920
#endif


typedef struct {
	libusb_context *ctx;
	int should_free_ctx;
} fnusb_ctx;

typedef struct {
	freenect_device *parent; //so we can go up from the libusb userdata
	libusb_device_handle *dev;
} fnusb_dev;

typedef struct {
	fnusb_dev *parent; //so we can go up from the libusb userdata
	struct libusb_transfer **xfers;
	uint8_t *buffer;
	fnusb_iso_cb cb;
	int num_xfers;
	int pkts;
	int len;
} fnusb_isoc_stream;

int fnusb_init(fnusb_ctx *ctx, freenect_usb_context *usb_ctx);
int fnusb_shutdown(fnusb_ctx *ctx);
int fnusb_process_events(fnusb_ctx *ctx);

int fnusb_open_subdevices(freenect_device *dev, int index);

int fnusb_start_iso(fnusb_dev *dev, fnusb_isoc_stream *strm, fnusb_iso_cb cb, int ep, int xfers, int pkts, int len);
int fnusb_stop_iso(fnusb_dev *dev, fnusb_isoc_stream *strm);

int fnusb_control(fnusb_dev *dev, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t wLength);


#endif
