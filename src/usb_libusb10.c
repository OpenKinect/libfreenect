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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>
#include "freenect_internal.h"

int fnusb_init(fnusb_ctx *ctx, freenect_usb_context *usb_ctx)
{
	int res;
	if (!usb_ctx) {
		res = libusb_init(&ctx->ctx);
		if (res >= 0) {
			ctx->should_free_ctx = 1;
			return 0;
		} else {
			ctx->should_free_ctx = 0;
			ctx->ctx = NULL;
			return res;
		}
	} else {
		ctx->ctx = usb_ctx;
		ctx->should_free_ctx = 0;
		return 0;
	}
}

int fnusb_shutdown(fnusb_ctx *ctx)
{
	//int res;
	if (ctx->should_free_ctx) {
		libusb_exit(ctx->ctx);
		ctx->ctx = NULL;
	}
	return 0;
}

int fnusb_process_events(fnusb_ctx *ctx)
{
	return libusb_handle_events(ctx->ctx);
}

int fnusb_open_subdevices(freenect_device *dev, int index)
{
	freenect_context *ctx = dev->parent;

	dev->usb_cam.parent = dev;
	dev->usb_cam.dev = NULL;
	dev->usb_motor.parent = dev;
	dev->usb_motor.dev = NULL;

	libusb_device **devs; //pointer to pointer of device, used to retrieve a list of devices
	ssize_t cnt = libusb_get_device_list (dev->parent->usb.ctx, &devs); //get the list of devices
	if (cnt < 0)
		return -1;

	int i = 0, nr_cam = 0, nr_mot = 0;
	int res;
	struct libusb_device_descriptor desc;

	for (i = 0; i < cnt; i++) {
		int r = libusb_get_device_descriptor (devs[i], &desc);
		if (r < 0)
			continue;

		if (desc.idVendor != VID_MICROSOFT)
			continue;

		// Search for the camera
		if (!dev->usb_cam.dev && desc.idProduct == PID_NUI_CAMERA) {
			// If the index given by the user matches our camera index
			if (nr_cam == index) {
				res = libusb_open (devs[i], &dev->usb_cam.dev);
				if (res < 0 || !dev->usb_cam.dev) {
					FN_ERROR("Could not open camera: %d\n", res);
					dev->usb_cam.dev = NULL;
					break;
				}
				res = libusb_claim_interface (dev->usb_cam.dev, 0);
				if (res < 0) {
					FN_ERROR("Could not claim interface on camera: %d\n", res);
					libusb_close(dev->usb_cam.dev);
					dev->usb_cam.dev = NULL;
					break;
				}

        struct libusb_config_descriptor* config;
        if( libusb_get_active_config_descriptor( devs[i], &config) == 0)
        {
          fprintf(stderr, "Got config\n  Number of Interfaces=%d\n", config->bNumInterfaces);
          uint16_t packetSize = config->interface[0].altsetting[0].endpoint[0].wMaxPacketSize;
          uint16_t transactions = packetSize >> 11;
          uint16_t size = packetSize & 0x7ff;
          packetSize = (transactions +1) * size;
          
          fprintf(stderr, "  numberOfTransactions=%d PacketSize=%d MaxPacketSize=%d\n", transactions, size, packetSize );

          int xx;
          for( xx = 0; xx < config->interface[0].altsetting[0].bNumEndpoints; ++xx)
          {
            int endpoint = config->interface[0].altsetting[0].endpoint[xx].bEndpointAddress;
            enum libusb_transfer_type transfer_type = config->interface[0].altsetting[0].endpoint[xx].bmAttributes;
            fprintf(stderr, "  endpoint=%x transfer_type=%d\n", endpoint, transfer_type);
          }

          libusb_free_config_descriptor(config);
        }

			} else {
				nr_cam++;
			}
		}

		// Search for the motor
		if (!dev->usb_motor.dev && desc.idProduct == PID_NUI_MOTOR) {
			// If the index given by the user matches our camera index
			if (nr_mot == index) {
				res = libusb_open (devs[i], &dev->usb_motor.dev);
				if (res < 0 || !dev->usb_motor.dev) {
					FN_ERROR("Could not open motor: %d\n", res);
					dev->usb_motor.dev = NULL;
					break;
				}
				res = libusb_claim_interface (dev->usb_motor.dev, 0);
				if (res < 0) {
					FN_ERROR("Could not claim interface on motor: %d\n", res);
					libusb_close(dev->usb_motor.dev);
					dev->usb_motor.dev = NULL;
					break;
				}
			} else {
				nr_mot++;
			}
		}
	}

	libusb_free_device_list (devs, 1);  // free the list, unref the devices in it

	if (dev->usb_cam.dev && dev->usb_motor.dev) {
		return 0;
	} else {
		if (dev->usb_cam.dev) {
			libusb_release_interface(dev->usb_cam.dev, 0);
			libusb_close(dev->usb_cam.dev);
		}
		if (dev->usb_motor.dev) {
			libusb_release_interface(dev->usb_motor.dev, 0);
			libusb_close(dev->usb_motor.dev);
		}
		return -1;
	}
}

int fnusb_close_subdevices(freenect_device *dev)
{
	if (dev->usb_cam.dev) {
		libusb_release_interface(dev->usb_cam.dev, 0);
		libusb_close(dev->usb_cam.dev);
		dev->usb_cam.dev = NULL;
	}
	if (dev->usb_motor.dev) {
		libusb_release_interface(dev->usb_motor.dev, 0);
		libusb_close(dev->usb_motor.dev);
		dev->usb_motor.dev = NULL;
	}
	return 0;
}

static void iso_callback(struct libusb_transfer *xfer)
{
	int i;
	fnusb_isoc_stream *strm = xfer->user_data;

	if (strm->dead) {
		freenect_context *ctx = strm->parent->parent->parent;
		strm->dead_xfers++;
		FN_SPEW("EP %02x transfer complete, %d left\n", xfer->endpoint, strm->num_xfers - strm->dead_xfers);
		return;
	}


	if(xfer->status == LIBUSB_TRANSFER_COMPLETED) {
		//uint8_t *buf = (void*)xfer->buffer;
		//for (i=0; i<strm->pkts; i++) {
    uint32_t byte_count = 0;

		for (i=0; i<xfer->num_iso_packets; i++) {
      if((xfer->iso_packet_desc[i].status == LIBUSB_TRANSFER_COMPLETED) && (xfer->iso_packet_desc[i].actual_length != 0))
      {
        uint8_t *buf = libusb_get_iso_packet_buffer_simple(xfer, i);
        if(xfer->buffer + byte_count != buf)  //missing data
        {
          fprintf(stderr,"move buffer\n");
          memcpy(xfer->buffer + byte_count, buf, xfer->iso_packet_desc[i].actual_length);
        }
        byte_count += xfer->iso_packet_desc[i].actual_length;

        //strm->cb(strm->parent->parent, buf, xfer->iso_packet_desc[i].actual_length);
        //buf += strm->len;
      }
      libusb_submit_transfer(xfer);
		}
    strm->cb(strm->parent->parent, xfer->buffer, byte_count);
	} else {
		freenect_context *ctx = strm->parent->parent->parent;
		FN_WARNING("Isochronous transfer error: %d\n", xfer->status);
		strm->dead_xfers++;
	}
}

int fnusb_start_iso(fnusb_dev *dev, fnusb_isoc_stream *strm, fnusb_iso_cb cb, int ep, int xfers, int pkts, int len)
{
	freenect_context *ctx = dev->parent->parent;
	int ret, i, xx;

  uint32_t max_packet_size = libusb_get_max_packet_size(libusb_get_device(dev->dev), 0x81);

  uint32_t transactions = max_packet_size >> 11;
  uint32_t packet_size = max_packet_size & 0x7ff;
  uint32_t allowed_max = (transactions + 1) * packet_size;
  uint32_t bufferSize = 32 * allowed_max;
  int num_xfers =  bufferSize / allowed_max;
  //fprintf(stderr, "max_packet_size=%d transactions=%d packet_size=%d allowed_max=%d num_xfers=%d bufferSize=%d\n", max_packet_size, transactions, packet_size, allowed_max, num_xfers, bufferSize );


	strm->parent = dev;
	strm->cb = cb;
	strm->num_xfers = xfers;
	strm->pkts = num_xfers;
	strm->len = bufferSize;
	//strm->buffer = malloc(bufferSize);
	strm->xfers = malloc(sizeof(struct libusb_transfer*) * num_xfers);
	strm->dead = 0;
	strm->dead_xfers = 0;


	for (i=0; i<xfers; i++) {
		FN_SPEW("Creating EP %02x transfer #%d\n", ep, i);
		strm->xfers[i] = libusb_alloc_transfer(num_xfers);

    uint8_t *bufp = (uint8_t*)malloc(bufferSize);
		libusb_fill_iso_transfer(strm->xfers[i], dev->dev, ep, bufp, bufferSize, num_xfers, iso_callback, strm, 0);

		libusb_set_iso_packet_lengths(strm->xfers[i], allowed_max);

		ret = libusb_submit_transfer(strm->xfers[i]);
		if (ret < 0)
			FN_WARNING("Failed to submit isochronous transfer %d: %d\n", i, ret);
	}

	return 0;

}

int fnusb_stop_iso(fnusb_dev *dev, fnusb_isoc_stream *strm)
{
	freenect_context *ctx = dev->parent->parent;
	int i;

	strm->dead = 1;

	for (i=0; i<strm->num_xfers; i++)
		libusb_cancel_transfer(strm->xfers[i]);

	while (strm->dead_xfers < strm->num_xfers) {
		libusb_handle_events(ctx->usb.ctx);
	}

	for (i=0; i<strm->num_xfers; i++)
		libusb_free_transfer(strm->xfers[i]);

	free(strm->buffer);
	free(strm->xfers);

	memset(strm, 0, sizeof(*strm));
	return 0;
}

int fnusb_control(fnusb_dev *dev, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t *data, uint16_t wLength)
{
	return libusb_control_transfer(dev->dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, 0);
}
