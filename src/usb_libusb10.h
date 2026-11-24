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

#pragma once

#include "libfreenect.h"
#include <libusb.h>

// There are a few rules: PKTS_PER_XFER * NUM_XFERS <= 1000, PKTS_PER_XFER % 8 == 0.
#if defined(__APPLE__)
  #define DEPTH_PKTBUF 2048
  #define VIDEO_PKTBUF 2048

  #define PKTS_PER_XFER 128
  #define NUM_XFERS 4
#else
  #define DEPTH_PKTBUF 1920
  #define VIDEO_PKTBUF 1920

  #if defined(_WIN32)
    // 16 x 32 packets = 64 ms queued per stream at high speed. With 8 transfers
    // (32 ms), WinUSB dropped depth packets while transfers were resubmitted.
    #define PKTS_PER_XFER 32
    #define NUM_XFERS 16
  #else
    #define PKTS_PER_XFER 16
    #define NUM_XFERS 16
  #endif
#endif

// High-speed isochronous transfers must cover whole 1 ms frames (8 microframes).
// WinUSB rejects transfers that don't (WinUsb_RegisterIsochBuffer), so enforce it.
#if (PKTS_PER_XFER % 8) != 0
  #error "PKTS_PER_XFER must be a multiple of 8"
#endif
#if (PKTS_PER_XFER * NUM_XFERS) > 1000
  #error "PKTS_PER_XFER * NUM_XFERS must not exceed 1000"
#endif

typedef struct {
	libusb_context *ctx;
	int should_free_ctx;
	int using_usbdk; // Windows only: the UsbDk backend was requested via LIBFREENECT_USBDK=1
} fnusb_ctx;

typedef struct {
	freenect_device *parent; //so we can go up from the libusb userdata
	libusb_device_handle *dev;
	int device_dead; // set to 1 when the underlying libusb_device_handle vanishes (ie, Kinect was unplugged)
	int VID;
	int PID;
} fnusb_dev;

// Isochronous stream health (iso_diag.c), counted in windows of
// FN_ISO_WINDOW_MS; the first window after a start is ignored.
// Empty packets alone are normal: the Kinect fills only part of the
// microframes (640x480 colour about 60%, depth about 90%). A stream with
// (almost) only empty packets delivers nothing; packets that fail with a USB
// error status are lost data.
#define FN_ISO_WINDOW_MS 2000
#define FN_ISO_NO_DATA_PERCENT 95 // at least this many empty: no data arrives
#define FN_ISO_LOSS_PERCENT 10    // at least this many failed: heavy loss
typedef enum { FN_ISO_OK = 0, FN_ISO_NO_DATA = 1, FN_ISO_HEAVY_LOSS = 2 } fn_iso_verdict;
typedef struct {
	uint64_t window_start_ms;
	uint32_t pkts, empty, failed;                // current window
	uint32_t last_pkts, last_empty, last_failed; // last complete window
	int windows;
	int reported;                   // bit per fn_iso_verdict already reported
} fn_iso_diag;
// Count pkts packets: empty of them without data (failed ones included),
// failed of them with an error status. Returns a verdict when a
// window completes with a problem not yet reported since the stream started.
fn_iso_verdict fn_iso_diag_add(fn_iso_diag *d, uint64_t now_ms, int pkts, int empty, int failed);
// 1 (and a description in name) if this looks like a virtual machine or WSL2.
int fn_detect_vm(char *name, int size);

typedef struct {
	fnusb_dev *parent; //so we can go up from the libusb userdata
	struct libusb_transfer **xfers;
	uint8_t *xfer_done; // per transfer: 1 once it has finished for good
	uint8_t *buffer;
	fnusb_iso_cb cb;
	int num_xfers;
	int pkts;
	int len;
	int dead;
	int dead_xfers;
	unsigned char endpoint;
	int error_logged; // a failed transfer was logged; reset by a completed one
	int resubmit_logged; // a failed resubmission was logged (once per stream start)
	fn_iso_diag diag;
} fnusb_isoc_stream;

int fnusb_num_devices(freenect_context *ctx);
int fnusb_list_device_attributes(freenect_context *ctx, struct freenect_device_attributes** attribute_list);

int fnusb_init(fnusb_ctx *ctx, freenect_usb_context *usb_ctx);
void fnusb_check_version(freenect_context *ctx);
int fnusb_shutdown(fnusb_ctx *ctx);
int fnusb_process_events(fnusb_ctx *ctx);
int fnusb_process_events_timeout(fnusb_ctx *ctx, struct timeval* timeout);

int fnusb_open_subdevices(freenect_device *dev, int index);
int fnusb_close_subdevices(freenect_device *dev);

int fnusb_start_iso(fnusb_dev *dev, fnusb_isoc_stream *strm, fnusb_iso_cb cb, unsigned char endpoint, int xfers, int pkts, int len);
int fnusb_stop_iso(fnusb_dev *dev, fnusb_isoc_stream *strm);
int fnusb_get_max_iso_packet_size(fnusb_dev *dev, unsigned char endpoint, int default_size);

int fnusb_control(fnusb_dev *dev, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t wLength);
int fnusb_bulk(fnusb_dev *dev, uint8_t endpoint, uint8_t *data, int len, int *transferred);
int fnusb_clear_halt(fnusb_dev *dev, uint8_t endpoint);
int fnusb_num_interfaces(fnusb_dev *dev);
int fnusb_has_audio_class(fnusb_dev *dev);
