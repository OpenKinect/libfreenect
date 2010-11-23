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
#include <unistd.h>

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

static int stream_process(freenect_context *ctx, packet_stream *strm, uint8_t *pkt, int len)
{
	if (len < 12)
		return 0;

	struct pkt_hdr *hdr = (void*)pkt;
	uint8_t *data = pkt + sizeof(*hdr);
	int datalen = len - sizeof(*hdr);

	if (hdr->magic[0] != 'R' || hdr->magic[1] != 'B') {
		FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_NOTICE, \
		       "[Stream %02x] Invalid magic %02x%02x\n", strm->flag, hdr->magic[0], hdr->magic[1]);
		return 0;
	}

	FN_FLOOD("[Stream %02x] Packet with flag: %02x\n", strm->flag, hdr->flag);

	uint8_t sof = strm->flag|1;
	uint8_t mof = strm->flag|2;
	uint8_t eof = strm->flag|5;

	// sync if required, dropping packets until SOF
	if (!strm->synced) {
		if (hdr->flag != sof) {
			FN_SPEW("[Stream %02x] Not synced yet...\n", strm->flag);
			return 0;
		}
		strm->synced = 1;
		strm->seq = hdr->seq;
		strm->pkt_num = 0;
		strm->valid_pkts = 0;
		strm->got_pkts = 0;
	}

	int got_frame = 0;

	// handle lost packets
	if (strm->seq != hdr->seq) {
		uint8_t lost = strm->seq - hdr->seq;
		FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_INFO, \
		       "[Stream %02x] Lost %d packets\n", strm->flag, lost);
		if (lost > 5) {
			FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_NOTICE, \
			       "[Stream %02x] Lost too many packets, resyncing...\n", strm->flag);
			strm->synced = 0;
			return 0;
		}
		strm->seq = hdr->seq;
		int left = strm->pkts_per_frame - strm->pkt_num;
		if (left <= lost) {
			strm->pkt_num = lost - left;
			strm->valid_pkts = strm->got_pkts;
			strm->got_pkts = 0;
			got_frame = 1;
			strm->timestamp = strm->last_timestamp;
			strm->valid_frames++;
		} else {
			strm->pkt_num += lost;
		}
	}

	// check the header to make sure it's what we expect
	if (!(strm->pkt_num == 0 && hdr->flag == sof) &&
	    !(strm->pkt_num == strm->pkts_per_frame-1 && hdr->flag == eof) &&
	    !(strm->pkt_num > 0 && strm->pkt_num < strm->pkts_per_frame-1 && hdr->flag == mof)) {
		FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_NOTICE, \
		       "[Stream %02x] Inconsistent flag %02x with %d packets in buf (%d total), resyncing...\n",
		       strm->flag, hdr->flag, strm->pkt_num, strm->pkts_per_frame);
		strm->synced = 0;
		return got_frame;
	}

	// copy data
	if (datalen > strm->pkt_size) {
		FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_WARNING, \
		       "[Stream %02x] Expected %d data bytes, but got %d. Dropping...\n", strm->flag, strm->pkt_size, datalen);
		return got_frame;
	}

	if (datalen != strm->pkt_size && hdr->flag != eof)
		FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_WARNING, \
		       "[Stream %02x] Expected %d data bytes, but got only %d\n", strm->flag, strm->pkt_size, datalen);

	uint8_t *dbuf = strm->buf + strm->pkt_num * strm->pkt_size;
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
		strm->valid_frames++;
		return 1;
	} else {
		return got_frame;
	}
}

// Unpack buffer of (vw bit) data into padded 16bit buffer.
static inline void convert_packed_to_16bit(uint8_t *raw, uint16_t *frame, int vw)
{
	int mask = (1 << vw) - 1;
	int i;
	int bitshift = 0;
	for (i=0; i<(640*480); i++) {
		int idx = (i*vw)/8;
		uint32_t word = (raw[idx]<<(16)) | (raw[idx+1]<<8) | raw[idx+2];
		frame[i] = ((word >> (((3*8)-vw)-bitshift)) & mask);
		bitshift = (bitshift + vw) % 8;
	}
}

static void depth_process(freenect_device *dev, uint8_t *pkt, int len)
{
	freenect_context *ctx = dev->parent;

	if (len == 0)
		return;

	if (!dev->depth_running)
		return;

	int got_frame = stream_process(ctx, &dev->depth_stream, pkt, len);

	if (!got_frame)
		return;

	FN_SPEW("Got depth frame %d/%d packets arrived, TS %08x\n",
	       dev->depth_stream.valid_pkts, dev->depth_stream.pkts_per_frame, dev->depth_stream.timestamp);

	switch (dev->depth_format) {
		case FREENECT_FORMAT_11_BIT:
			convert_packed_to_16bit(dev->depth_raw, dev->depth_frame, 11);
			if (dev->depth_cb)
				dev->depth_cb(dev, dev->depth_frame, dev->depth_stream.timestamp);
			break;
		case FREENECT_FORMAT_10_BIT:
			convert_packed_to_16bit(dev->depth_raw, dev->depth_frame, 10);
			if (dev->depth_cb)
				dev->depth_cb(dev, dev->depth_frame, dev->depth_stream.timestamp);
			break;
		case FREENECT_FORMAT_PACKED_10_BIT:
		case FREENECT_FORMAT_PACKED_11_BIT:
			if (dev->depth_cb)
				dev->depth_cb(dev, dev->depth_raw, dev->depth_stream.timestamp);
			break;
	}
}

static void rgb_process(freenect_device *dev, uint8_t *pkt, int len)
{
	freenect_context *ctx = dev->parent;
	int x,y,i;

	if (len == 0)
		return;

	if (!dev->rgb_running)
		return;

	int got_frame = stream_process(ctx, &dev->rgb_stream, pkt, len);

	if (!got_frame)
		return;

	FN_SPEW("Got RGB frame %d/%d packets arrived, TS %08x\n", dev->rgb_stream.valid_pkts,
	       dev->rgb_stream.pkts_per_frame, dev->rgb_stream.timestamp);

	uint8_t *rgb_frame = NULL;

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

typedef struct {
	uint8_t magic[2];
	uint16_t len;
	uint16_t cmd;
	uint16_t tag;
} cam_hdr;

static int send_cmd(freenect_device *dev, uint16_t cmd, void *cmdbuf, unsigned int cmd_len, void *replybuf, unsigned int reply_len)
{
	freenect_context *ctx = dev->parent;
	int res, actual_len;
	uint8_t obuf[0x400];
	uint8_t ibuf[0x200];
	cam_hdr *chdr = (void*)obuf;
	cam_hdr *rhdr = (void*)ibuf;

	if (cmd_len & 1 || cmd_len > (0x400 - sizeof(*chdr))) {
		FN_ERROR("send_cmd: Invalid command length (0x%x)\n", cmd_len);
		return -1;
	}

	chdr->magic[0] = 0x47;
	chdr->magic[1] = 0x4d;
	chdr->cmd = cmd;
	chdr->tag = dev->cam_tag;
	chdr->len = cmd_len / 2;

	memcpy(obuf+sizeof(*chdr), cmdbuf, cmd_len);

	res = fnusb_control(&dev->usb_cam, 0x40, 0, 0, 0, obuf, cmd_len + sizeof(*chdr));
	FN_SPEW("Control cmd=%04x tag=%04x len=%04x: %d\n", cmd, dev->cam_tag, cmd_len, res);
	if (res < 0) {
		FN_ERROR("send_cmd: Output control transfer failed (%d)\n", res);
		return res;
	}

	do {
		actual_len = fnusb_control(&dev->usb_cam, 0xc0, 0, 0, 0, ibuf, 0x200);
	} while (actual_len == 0);
	FN_SPEW("Control reply: %d\n", res);
	if (actual_len < sizeof(*rhdr)) {
		FN_ERROR("send_cmd: Input control transfer failed (%d)\n", res);
		return res;
	}
	actual_len -= sizeof(*rhdr);

	if (rhdr->magic[0] != 0x52 || rhdr->magic[1] != 0x42) {
		FN_ERROR("send_cmd: Bad magic %02x %02x\n", rhdr->magic[0], rhdr->magic[1]);
		return -1;
	}
	if (rhdr->cmd != chdr->cmd) {
		FN_ERROR("send_cmd: Bad cmd %02x != %02x\n", rhdr->cmd, chdr->cmd);
		return -1;
	}
	if (rhdr->tag != chdr->tag) {
		FN_ERROR("send_cmd: Bad tag %04x != %04x\n", rhdr->tag, chdr->tag);
		return -1;
	}
	if (rhdr->len != (actual_len/2)) {
		FN_ERROR("send_cmd: Bad len %04x != %04x\n", rhdr->len, (int)(actual_len/2));
		return -1;
	}

	if (actual_len > reply_len) {
		FN_WARNING("send_cmd: Data buffer is %d bytes long, but got %d bytes\n", reply_len, actual_len);
		memcpy(replybuf, ibuf+sizeof(*rhdr), reply_len);
	} else {
		memcpy(replybuf, ibuf+sizeof(*rhdr), actual_len);
	}

	dev->cam_tag++;

	return actual_len;
}

static int write_register(freenect_device *dev, uint16_t reg, uint16_t data)
{
	freenect_context *ctx = dev->parent;
	uint16_t reply[2];
	uint16_t cmd[2];
	int res;

	cmd[0] = reg;
	cmd[1] = data;

	FN_DEBUG("Write Reg 0x%04x <= 0x%02x\n", reg, data);
	res = send_cmd(dev, 0x03, cmd, 4, reply, 4);
	if (res < 0)
		return res;
	if (res != 2) {
		FN_WARNING("send_cmd returned %d [%04x %04x], 0000 expected\n", res, reply[0], reply[1]);
	}
	return 0;
}

int freenect_start_depth(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;

	if (dev->depth_running)
		return -1;

	dev->depth_stream.buf = dev->depth_raw;
	dev->depth_stream.pkts_per_frame =
			dev->depth_format == FREENECT_FORMAT_11_BIT ?
			DEPTH_PKTS_11_BIT_PER_FRAME : DEPTH_PKTS_10_BIT_PER_FRAME;
	dev->depth_stream.pkt_size = DEPTH_PKTDSIZE;
	dev->depth_stream.synced = 0;
	dev->depth_stream.flag = 0x70;
	dev->depth_stream.valid_frames = 0;

	res = fnusb_start_iso(&dev->usb_cam, &dev->depth_isoc, depth_process, 0x82, NUM_XFERS, PKTS_PER_XFER, DEPTH_PKTBUF);
	if (res < 0)
		return res;

	write_register(dev, 0x06, 0x00); // reset depth stream
	switch (dev->depth_format) {
		case FREENECT_FORMAT_11_BIT:
		case FREENECT_FORMAT_PACKED_11_BIT:
			write_register(dev, 0x12, 0x03);
			break;
		case FREENECT_FORMAT_10_BIT:
		case FREENECT_FORMAT_PACKED_10_BIT:
			write_register(dev, 0x12, 0x02);
			break;
		default:
			FN_ERROR("Invalid depth format %d\n", dev->depth_format);
			break;
	}
	write_register(dev, 0x13, 0x01);
	write_register(dev, 0x14, 0x1e);
	write_register(dev, 0x06, 0x02); // start depth stream

	dev->depth_running = 1;
	return 0;
}

int freenect_start_rgb(freenect_device *dev)
{
	int res;

	if (dev->rgb_running)
		return -1;

	dev->rgb_stream.buf = dev->rgb_raw;
	dev->rgb_stream.pkts_per_frame = RGB_PKTS_PER_FRAME;
	dev->rgb_stream.pkt_size = RGB_PKTDSIZE;
	dev->rgb_stream.synced = 0;
	dev->rgb_stream.flag = 0x80;
	dev->rgb_stream.valid_frames = 0;

	res = fnusb_start_iso(&dev->usb_cam, &dev->rgb_isoc, rgb_process, 0x81, NUM_XFERS, PKTS_PER_XFER, RGB_PKTBUF);
	if (res < 0)
		return res;

	write_register(dev, 0x05, 0x00); // reset rgb stream
	write_register(dev, 0x0c, 0x00);
	write_register(dev, 0x0d, 0x01);
	write_register(dev, 0x0e, 0x1e); // 30Hz bayer
	write_register(dev, 0x05, 0x01); // start rgb stream
	write_register(dev, 0x47, 0x00); // disable Hflip

	dev->rgb_running = 1;
	return 0;
}

int freenect_stop_depth(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;

	if (!dev->depth_running)
		return -1;

	dev->depth_running = 0;
	write_register(dev, 0x06, 0x00); // stop depth stream

	res = fnusb_stop_iso(&dev->usb_cam, &dev->depth_isoc);
	if (res < 0) {
		FN_ERROR("Failed to stop depth isochronous stream: %d\n", res);
		return res;
	}

	return 0;
}

int freenect_stop_rgb(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;

	if (!dev->rgb_running)
		return -1;

	dev->rgb_running = 0;
	write_register(dev, 0x05, 0x00); // stop rgb stream

	res = fnusb_stop_iso(&dev->usb_cam, &dev->rgb_isoc);
	if (res < 0) {
		FN_ERROR("Failed to stop RGB isochronous stream: %d\n", res);
		return res;
	}

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

int freenect_set_depth_format(freenect_device *dev, freenect_depth_format fmt)
{
	dev->depth_format = fmt;
	return 0;
}
