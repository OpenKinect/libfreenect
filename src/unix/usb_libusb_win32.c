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
#include <stdlib.h>
#include <usb.h>
#include "freenect_internal.h"

int fnusb_init(fnusb_ctx *ctx, freenect_usb_context *usb_ctx)
{	
	int i;

	for (i=0; i < USB_MAX_STREAMS; i++)
	{
		open_streams[i] = NULL;
	}
	usb_init();
    usb_find_busses();
    usb_find_devices();
	ctx->should_free_ctx = 1;
	return 0;		
}

int fnusb_shutdown(fnusb_ctx *ctx)
{
	return 0;
}

void iso_callback(fnusb_isoc_stream *strm);
int fnusb_process_events(fnusb_ctx *ctx)
{
	int i;
	int read, ret;
	uint8_t *buf;
	fnusb_isoc_stream* stream;

	for (i=0; i<USB_MAX_STREAMS; i++)
	{
		stream = open_streams[i];
		if (NULL == stream)
			continue;

		read = usb_reap_async_nocancel(stream->xfers[stream->xfer_index].context, 10000);
		//printf("read %d bytes\n", read);
		if (read < 0)
		{
			if (read != -116)
			{
				printf("error: %s\n", usb_strerror());
				usb_cancel_async(stream->xfers[stream->xfer_index].context);
			}
		}
		else
		{
			if (read > 0)
			{
				iso_callback(stream, read);	
			}
			buf = (uint8_t*)stream->xfers[stream->xfer_index].buffer;

			ZeroMemory(buf, stream->pkts * stream->len);
			ret = usb_submit_async(stream->xfers[stream->xfer_index].context, (char*)stream->xfers[i].buffer, stream->pkts * stream->len);
			if( ret < 0 ){
				printf("error: %s\n", usb_strerror());
				usb_cancel_async(stream->xfers[stream->xfer_index].context);
			}

			stream->xfer_index++;
			if (stream->xfer_index >= stream->num_xfers)
			{
				stream->xfer_index = 0;
			}	
		}
	}

	
}	

struct usb_device* fnusb_find_device_by_vid_pid(unsigned short vid, unsigned short pid, unsigned int index)
{
	struct usb_bus *busses;
	struct usb_bus *bus;
    int c, i, a, count = 0;

	busses = usb_get_busses();

	for (bus = busses; bus; bus = bus->next) {

		struct usb_device *dev;

		for (dev = bus->devices; dev; dev = dev->next) {

			if (dev->descriptor.idProduct == pid && dev->descriptor.idVendor == vid ) {
				if (count == index)
				{
					return dev;
				}
				count++;
			}
		}
	}
	return NULL;
}

int fnusb_open_subdevices(freenect_device *dev, int index)
{
	struct usb_device *cam_device = NULL;
	struct usb_dev_handle *cam_handle;

	dev->usb_cam.parent = dev;
	
	cam_device = fnusb_find_device_by_vid_pid(0x45e, 0x2ae, index);
		
	if(NULL == cam_device) return -1; //Can't find device
	
	cam_handle = usb_open(cam_device);
	dev->usb_cam.dev = cam_handle;

	if(usb_set_configuration(cam_handle, 1) < 0) return -2; //open failed
	if(usb_claim_interface(cam_handle, 0) < 0) return -2; //open failed

	return 0;
}

int fnusb_close_subdevices(freenect_device *dev)
{
	return -2;//close not implemented
}

static void iso_callback(fnusb_isoc_stream *stream, int read)
{
	int i, j, ret;
	
	uint8_t *buf = (uint8_t*)stream->xfers[stream->xfer_index].buffer;

	for (j=0; j<read; j++)
	{
		if (buf[j] == 'R' &&
			buf[j+1] == 'B')
		{
			//printf("RB: %d\n", j);
			stream->cb(stream->parent->parent, &buf[j], stream->len);
		}
	}
/*
	for (i=0; i<stream->pkts; i++) {
		stream->cb(stream->parent->parent, buf, stream->len);
		
		buf += stream->len;
	}*/
	
}

int fnusb_start_iso(fnusb_dev *dev, fnusb_isoc_stream *strm, fnusb_iso_cb cb, int ep, int num_xfers, int pkts, int len)
{
	int ret, i;
	strm->parent = dev;
	strm->cb = cb;
	strm->num_xfers = num_xfers;
	strm->pkts = pkts;
	strm->len = len;
	strm->xfers = (fnusb_xfer*)malloc(sizeof(fnusb_xfer) * num_xfers);
	strm->xfer_index = 0;

	for (i=0; i<num_xfers; i++) {
		printf("Creating EP %02x transfer #%d\n", ep, i);		
		
		strm->xfers[i].context = NULL;
		strm->xfers[i].buffer = (uint8_t*)malloc(sizeof(uint8_t) * strm->len * strm->pkts);
		ZeroMemory(strm->xfers[i].buffer, strm->pkts * strm->len);

		usb_isochronous_setup_async(dev->dev, &strm->xfers[i].context, ep, len);
		ret = usb_submit_async(strm->xfers[i].context, (char*)strm->xfers[i].buffer, pkts * len);
		if (ret < 0)
			printf("Failed to submit xfer %d: %d\n", i, ret);

	}

	for (i=0; i<USB_MAX_STREAMS; i++)
	{
		if (open_streams[i] == NULL)
			open_streams[i] = strm;
	}
	
	return 0;

}

int fnusb_stop_iso(fnusb_dev *dev, fnusb_isoc_stream *strm)
{
	return -1;
}

int fnusb_control(fnusb_dev *dev, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, char *data, uint16_t wLength)
{
	return usb_control_msg(dev->dev, bmRequestType, bRequest, wValue, wIndex, data, wLength, 160);
}
