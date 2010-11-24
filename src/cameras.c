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
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "freenect_internal.h"

struct pkt_hdr {
	uint8_t magic[2];
	uint8_t pad;
	uint8_t flag;
	uint8_t unk1;
	uint8_t seq;
	uint8_t unk2;
	uint8_t unk3;
	uint32_t timestamp;
};

extern const struct caminit inits[];
extern const int num_inits;

static int stream_process(packet_stream *strm, uint8_t *pkt, int len)
{
	uint8_t sof;
	uint8_t mof;
	uint8_t eof;
	int got_frame;
	uint8_t lost;
	int left;
	uint8_t *dbuf;

	struct pkt_hdr *hdr = (struct pkt_hdr*)pkt;
	uint8_t *data = pkt + sizeof(*hdr);
	int datalen = len - sizeof(*hdr);
	
	if (len < 12)
		return 0;

	if (hdr->magic[0] != 'R' || hdr->magic[1] != 'B') {
		printf("[Stream %02x] Invalid magic %02x%02x\n", strm->flag, hdr->magic[0], hdr->magic[1]);
		return 0;
	}

	//printf("[Stream %02x] %02x\n", strm->flag, hdr->flag);

	sof = strm->flag|1;
	mof = strm->flag|2;
	eof = strm->flag|5;

	// sync if required, dropping packets until SOF
	if (!strm->synced) {
		if (hdr->flag != sof) {
			//printf("[Stream %02x] not synced yet...\n", strm->flag);
			return 0;
		}
		strm->synced = 1;
		strm->seq = hdr->seq;
		strm->pkt_num = 0;
		strm->valid_pkts = 0;
		strm->got_pkts = 0;
	}

	got_frame = 0;

	// handle lost packets
	if (strm->seq != hdr->seq) {
		lost = strm->seq - hdr->seq;
		printf("[Stream %02x] lost %d packets\n", strm->flag, lost);
		if (lost > 5) {
			printf("[Stream %02x] lost too many packets, resyncing...\n", strm->flag);
			strm->synced = 0;
			return 0;
		}
		strm->seq = hdr->seq;
		left = strm->pkts_per_frame - strm->pkt_num;
		if (left <= lost) {
			strm->pkt_num = lost - left;
			strm->valid_pkts = strm->got_pkts;
			strm->got_pkts = 0;
			got_frame = 1;
			strm->timestamp = strm->last_timestamp;
		} else {
			strm->pkt_num += lost;
		}
	}

	// check the header to make sure it's what we expect
	if (!(strm->pkt_num == 0 && hdr->flag == sof) &&
	    !(strm->pkt_num == strm->pkts_per_frame-1 && hdr->flag == eof) &&
	    !(strm->pkt_num > 0 && strm->pkt_num < strm->pkts_per_frame-1 && hdr->flag == mof)) {
		printf("[Stream %02x] Inconsistent flag %02x with %d packets in buf (%d total), resyncing...\n",
		       strm->flag, hdr->flag, strm->pkt_num, strm->pkts_per_frame);
		strm->synced = 0;
		return 0;
	}

	// copy data
	if (datalen > strm->pkt_size) {
		printf("[Stream %02x] Expected %d data bytes, but got %d. Dropping...\n", strm->flag, strm->pkt_size, datalen);
		return 0;
	}

	if (datalen != strm->pkt_size && hdr->flag != eof)
		printf("[Stream %02x] Expected %d data bytes, but got only %d\n", strm->flag, strm->pkt_size, datalen);

	dbuf = strm->buf + strm->pkt_num * strm->pkt_size;
	memcpy(dbuf, data, datalen);

	strm->pkt_num++;
	strm->seq++;
	strm->got_pkts++;

	strm->last_timestamp = hdr->timestamp;

	if (strm->pkt_num == strm->pkts_per_frame) {
		strm->pkt_num = 0;
		strm->valid_pkts = strm->got_pkts;
		strm->got_pkts = 0;
		strm->timestamp = hdr->timestamp;
		return 1;
	} else {
		return got_frame;
	}
}

static void depth_process(freenect_device *dev, uint8_t *pkt, int len)
{
	int i;
	int got_frame;
	int bitshift;
	int idx;
	uint32_t word;

	if (len == 0)
		return;

	got_frame = stream_process(&dev->depth_stream, pkt, len);

	if (!got_frame)
		return;

	printf("GOT DEPTH FRAME %d/%d packets arrived, TS %08x\n",
	       dev->depth_stream.valid_pkts, dev->depth_stream.pkts_per_frame, dev->depth_stream.timestamp);

	bitshift = 0;
	for (i=0; i<(640*480); i++) {
		idx = (i*11)/8;
		word = (dev->depth_raw[idx]<<16) | (dev->depth_raw[idx+1]<<8) | dev->depth_raw[idx+2];
		dev->depth_frame[i] = ((word >> (13-bitshift)) & 0x7ff);
		bitshift = (bitshift + 11) % 8;
	}

	if (dev->depth_cb)
		dev->depth_cb(dev, dev->depth_frame, dev->depth_stream.timestamp);
}

static void rgb_process(freenect_device *dev, uint8_t *pkt, int len)
{
	int x,y,i;
	int got_frame;
	uint8_t *rgb_frame;
	if (len == 0)
		return;

	got_frame = stream_process(&dev->rgb_stream, pkt, len);

	if (!got_frame)
		return;

	printf("GOT RGB FRAME %d/%d packets arrived, TS %08x\n", dev->rgb_stream.valid_pkts,
	       dev->rgb_stream.pkts_per_frame, dev->rgb_stream.timestamp);

	rgb_frame = NULL;

	if (dev->rgb_format == FREENECT_FORMAT_BAYER) {
		rgb_frame = dev->rgb_raw;
	} else {
		rgb_frame = dev->rgb_frame;
		/* Pixel arrangement:
		 * G R G R G R G R
		 * B G B G B G B G
		 * G R G R G R G R
		 * B G B G B G B G
		 * G R G R G R G R
		 * B G B G B G B G
		 */
		for (y=0; y<480; y++) {
			for (x=0; x<640; x++) {
				i = (y*640+x);
				if ((y&1) == 0) {
					if ((x&1) == 0) {
						// topleft G pixel
						uint8_t rr = dev->rgb_raw[i+1];
						uint8_t rl = x == 0 ? rr : dev->rgb_raw[i-1];
						uint8_t bb = dev->rgb_raw[i+640];
						uint8_t bt = y == 0 ? bb : dev->rgb_raw[i-640];
						rgb_frame[3*i+0] = (rl+rr)>>1;
						rgb_frame[3*i+1] = dev->rgb_raw[i];
						rgb_frame[3*i+2] = (bt+bb)>>1;
					} else {
						// R pixel
						uint8_t gl = dev->rgb_raw[i-1];
						uint8_t gr = x == 639 ? gl : dev->rgb_raw[i+1];
						uint8_t gb = dev->rgb_raw[i+640];
						uint8_t gt = y == 0 ? gb : dev->rgb_raw[i-640];
						uint8_t bbl = dev->rgb_raw[i+639];
						uint8_t btl = y == 0 ? bbl : dev->rgb_raw[i-641];
						uint8_t bbr = x == 639 ? bbl : dev->rgb_raw[i+641];
						uint8_t btr = x == 639 ? btl : y == 0 ? bbr : dev->rgb_raw[i-639];
						rgb_frame[3*i+0] = dev->rgb_raw[i];
						rgb_frame[3*i+1] = (gl+gr+gb+gt)>>2;
						rgb_frame[3*i+2] = (bbl+btl+bbr+btr)>>2;
					}
				} else {
					if ((x&1) == 0) {
						// B pixel
						uint8_t gr = dev->rgb_raw[i+1];
						uint8_t gl = x == 0 ? gr : dev->rgb_raw[i-1];
						uint8_t gt = dev->rgb_raw[i-640];
						uint8_t gb = y == 479 ? gt : dev->rgb_raw[i+640];
						uint8_t rtr = dev->rgb_raw[i-639];
						uint8_t rbr = y == 479 ? rtr : dev->rgb_raw[i-641];
						uint8_t rtl = x == 0 ? rtr : dev->rgb_raw[i-641];
						uint8_t rbl = x == 0 ? rbr : y == 479 ? rtl : dev->rgb_raw[i+639];
						rgb_frame[3*i+0] = (rbl+rtl+rbr+rtr)>>2;
						rgb_frame[3*i+1] = (gl+gr+gb+gt)>>2;
						rgb_frame[3*i+2] = dev->rgb_raw[i];
					} else {
						// botright G pixel
						uint8_t bl = dev->rgb_raw[i-1];
						uint8_t br = x == 639 ? bl : dev->rgb_raw[i+1];
						uint8_t rt = dev->rgb_raw[i-640];
						uint8_t rb = y == 479 ? rt : dev->rgb_raw[i+640];
						rgb_frame[3*i+0] = (rt+rb)>>1;
						rgb_frame[3*i+1] = dev->rgb_raw[i];
						rgb_frame[3*i+2] = (bl+br)>>1;
					}
				}
			}
		}
	}

	if (dev->rgb_cb)
		dev->rgb_cb(dev, rgb_frame, dev->rgb_stream.timestamp);
}

struct cam_hdr {
	uint8_t magic[2];
	uint16_t len;
	uint16_t cmd;
	uint16_t tag;
};

static void send_init(freenect_device *dev)
{
	int i, j, ret;
	uint8_t obuf[0x400];
	uint8_t ibuf[0x200];
	struct cam_hdr *chdr = (void*)obuf;
	struct cam_hdr *rhdr = (void*)ibuf;

	ret = fnusb_control(&dev->usb_cam, 0x80, 0x06, 0x3ee, 0, ibuf, 0x12);
	printf("First xfer: %d\n", ret);

	chdr->magic[0] = 0x47;
	chdr->magic[1] = 0x4d;
	
	for (i=0; i<num_inits; i++) {
		const struct caminit *ip = &inits[i];
		chdr->cmd = ip->command;
		chdr->tag = ip->tag;
		chdr->len = ip->cmdlen / 2;
		memcpy(obuf+sizeof(*chdr), ip->cmddata, ip->cmdlen);
		ret = fnusb_control(&dev->usb_cam, 0x40, 0, 0, 0, obuf, ip->cmdlen + sizeof(*chdr));
		printf("CTL CMD %04x %04x = %d\n", chdr->cmd, chdr->tag, ret);
		do {
			ret = fnusb_control(&dev->usb_cam, 0xc0, 0, 0, 0, ibuf, 0x200);
		} while (ret == 0);
		printf("CTL RES = %d\n", ret);
		if (rhdr->magic[0] != 0x52 || rhdr->magic[1] != 0x42) {
			printf("Bad magic %02x %02x\n", rhdr->magic[0], rhdr->magic[1]);
			continue;
		}
		if (rhdr->cmd != chdr->cmd) {
			printf("Bad cmd %02x != %02x\n", rhdr->cmd, chdr->cmd);
			continue;
		}
		if (rhdr->tag != chdr->tag) {
			printf("Bad tag %04x != %04x\n", rhdr->tag, chdr->tag);
			continue;
		}
		if (rhdr->len != (ret-sizeof(*rhdr))/2) {
			printf("Bad len %04x != %04x\n", rhdr->len, (int)(ret-sizeof(*rhdr))/2);
			continue;
		}
		if (rhdr->len != (ip->replylen/2) || memcmp(ibuf+sizeof(*rhdr), ip->replydata, ip->replylen)) {
			printf("Expected: ");
			for (j=0; j<ip->replylen; j++) {
				printf("%02x ", ip->replydata[j]);
			}
			printf("\nGot:      ");
			for (j=0; j<(rhdr->len*2); j++) {
				printf("%02x ", ibuf[j+sizeof(*rhdr)]);
			}
			printf("\n");
		}
	}
	dev->cam_inited = 1;
}

int freenect_start_depth(freenect_device *dev)
{
	int res;

	dev->depth_stream.buf = dev->depth_raw;
	dev->depth_stream.pkts_per_frame = dev->depth_format == FREENECT_FORMAT_11_BIT ?
			DEPTH_PKTS_11_BIT_PER_FRAME : DEPTH_PKTS_10_BIT_PER_FRAME;
	dev->depth_stream.pkt_size = DEPTH_PKTDSIZE;
	dev->depth_stream.synced = 0;
	dev->depth_stream.flag = 0x70;
	
	res = fnusb_start_iso(&dev->usb_cam, &dev->depth_isoc, depth_process, 0x82, NUM_XFERS, PKTS_PER_XFER, DEPTH_PKTBUF);

	if(!dev->cam_inited)
		send_init(dev);
	return res;
}
int freenect_start_rgb(freenect_device *dev)
{
	int res;

	dev->rgb_stream.buf = dev->rgb_raw;
	dev->rgb_stream.pkts_per_frame = RGB_PKTS_PER_FRAME;
	dev->rgb_stream.pkt_size = RGB_PKTDSIZE;
	dev->rgb_stream.synced = 0;
	dev->rgb_stream.flag = 0x80;

	res = fnusb_start_iso(&dev->usb_cam, &dev->rgb_isoc, rgb_process, 0x81, NUM_XFERS, PKTS_PER_XFER, RGB_PKTBUF);

	if(!dev->cam_inited)
		send_init(dev);
	return res;
}

int freenect_stop_depth(freenect_device *dev)
{
	printf("%s NOT IMPLEMENTED YET\n", __FUNCTION__);
	return 0;
}
int freenect_stop_rgb(freenect_device *dev)
{
	printf("%s NOT IMPLEMENTED YET\n", __FUNCTION__);
	return 0;
}

void freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb)
{
	dev->depth_cb = cb;
}

void freenect_set_rgb_callback(freenect_device *dev, freenect_rgb_cb cb)
{
	dev->rgb_cb = cb;
}

int freenect_set_rgb_format(freenect_device *dev, freenect_rgb_format fmt)
{
	dev->rgb_format = fmt;
	return 0;
}
