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
#include <dc1394/dc1394.h>

#include "freenect_internal.h"

struct pkt_hdr {
	uint8_t magic[2];
	uint16_t flag;
	uint16_t seq;
	uint16_t size;
	uint32_t timestamp;
};

extern const struct caminit inits[];
extern const int num_inits;

static int stream_process(freenect_context *ctx, packet_stream *strm, uint8_t *pkt, int len)
{
	if (len < 2)
		return 0;

  uint8_t* end = pkt + len; //mark the end
  int got_frame = 0;

  while( pkt < end )
  {
    switch(strm->state)
    {
      case 0: //looking for magic
        {
          if (pkt[1] == 'B' && pkt[0] == 'R')  //Found a match
          {
            strm->state = 1; //next state
            //fprintf(stderr,"got magic\n");
          }
          else
          {
            pkt++;
            pkt++;
          }
        }
        break;

      case 1: //Got a magic match
        {
          // Pull out the header info, hope that is complete

          struct pkt_hdr *hdr = (void*)pkt;
          //uint8_t *data = pkt + sizeof(*hdr);
          //int datalen = len - sizeof(*hdr);


          if (hdr->magic[0] != 'R' || hdr->magic[1] != 'B') {
            FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_NOTICE, \
                   "Invalid magic %02x%02x\n", hdr->magic[0], hdr->magic[1]);
            //return 0;
            continue;
          }

          hdr->flag = hdr->flag >> 8 | hdr->flag << 8;
          hdr->seq = hdr->seq >> 8 | hdr->seq << 8;
          hdr->size = hdr->size >> 8 | hdr->size << 8;
          strm->flag = hdr->flag;
          //fprintf(stderr, "header: flag=%x seq=%x size=%x time=%x \n", hdr->flag, hdr->seq, hdr->size, hdr->timestamp);
          if((hdr->flag & 0xF) == 0x1)
          {
            //fprintf(stderr,"==> START <==\n");
            strm->buf_ptr = strm->buf;  //reset to beginning
          }

          //FN_FLOOD("Packet with flag: %02x\n", hdr->flag);

          strm->bytes_left = hdr->size - sizeof(struct pkt_hdr); // count down until we get all the bytes
          strm->state = 2;
#if 0
          uint8_t sof = strm->flag|1;
          uint8_t mof = strm->flag|2;
          uint8_t eof = strm->flag|5;


          // sync if required, dropping packets until SOF
          if (!strm->synced) {
            if (hdr->flag != sof) {
              FN_SPEW("[Stream %02x] Not synced yet...\n", strm->flag);
              //return 0;
              continue;
            }
            strm->synced = 1;
            strm->seq = hdr->seq;
            strm->pkt_num = 0;
            strm->valid_pkts = 0;
            strm->got_pkts = 0;
          }


          // handle lost packets
          if (strm->seq != hdr->seq) {
            uint8_t lost = hdr->seq - strm->seq;
            FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_INFO, \
                   "[Stream %02x] Lost %d packets\n", strm->flag, lost);
            if (lost > 5) {
              FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_NOTICE, \
                     "[Stream %02x] Lost too many packets, resyncing...\n", strm->flag);
              strm->synced = 0;
              //return 0;
              continue;
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
            //return got_frame;
            continue;
          }

          // copy data
          if (datalen > strm->pkt_size) {
            FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_WARNING, \
                   "[Stream %02x] Expected %d data bytes, but got %d. Dropping...\n", strm->flag, strm->pkt_size, datalen);
            //return got_frame;
            continue;
          }

          if (hdr->size - sizeof(struct pkt_hdr) != datalen)
            FN_LOG(strm->valid_frames < 2 ? LL_SPEW : LL_WARNING, \
                   "[Stream %02x] Expected %d data bytes, but header reports only %d\n", strm->flag, datalen, hdr->size);

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
            //return 1;
            continue;
          } else {
            //return got_frame;
            continue;
          }
#endif
          pkt += sizeof(struct pkt_hdr);
        }
        break;

      case 2: // get the data
        {
          uint8_t *data = pkt;

          size_t bytes_to_copy = ((end-data) < strm->bytes_left ? (end-data):strm->bytes_left);
          //uint8_t *dbuf = strm->buf + (FRAME_PIX - strm->bytes_left);
          uint8_t *dbuf = strm->buf_ptr;
          memcpy(dbuf, data, bytes_to_copy);
          strm->bytes_left -= bytes_to_copy;
          strm->buf_ptr += bytes_to_copy;
          //fprintf(stderr,"Copy %d bytes with bytes_left=%d\n", bytes_to_copy, strm->bytes_left);
          pkt += bytes_to_copy;

          if(strm->bytes_left == 0)
          {
            strm->state = 0;
            if(((strm->flag & 0x0F) == 0x05))  //marks end
            {
              //fprintf(stderr,"==> END <==\n");
              got_frame = 1;
            }
          }
        }
        break;
    }
  }

  return got_frame;
}

static void stream_initbufs(freenect_context *ctx, packet_stream *strm, int rlen, int plen)
{
	if (strm->usr_buf) {
		strm->lib_buf = NULL;
		strm->proc_buf = strm->usr_buf;
	} else {
		strm->lib_buf = malloc(plen);
		strm->proc_buf = strm->lib_buf;
	}

	if (rlen == 0) {
		strm->split_bufs = 0;
		strm->raw_buf = strm->proc_buf;
	} else {
		strm->split_bufs = 1;
		strm->raw_buf = malloc(rlen);
	}
}

static void stream_freebufs(freenect_context *ctx, packet_stream *strm)
{
	if (strm->split_bufs)
		free(strm->raw_buf);
	if (strm->lib_buf)
		free(strm->lib_buf);

	strm->raw_buf = NULL;
	strm->proc_buf = NULL;
	strm->lib_buf = NULL;
}

static int stream_setbuf(freenect_context *ctx, packet_stream *strm, void *pbuf)
{
	if (!strm->running) {
		strm->usr_buf = pbuf;
		return 0;
	} else {
		if (!pbuf && !strm->lib_buf) {
			FN_ERROR("Attempted to set buffer to NULL but stream was started with no internal buffer\n");
			return -1;
		}
		strm->usr_buf = pbuf;

		if (!pbuf)
			strm->proc_buf = strm->lib_buf;
		else
			strm->proc_buf = pbuf;

		if (!strm->split_bufs)
			strm->raw_buf = strm->proc_buf;
		return 0;
	}
}

// Unpack buffer of (vw bit) data into padded 16bit buffer.
static inline void convert_packed_to_16bit(uint8_t *raw, uint16_t *frame, int vw)
{
	int mask = (1 << vw) - 1;
	int j = 640*480;
	uint32_t buffer = 0;
	int bitsIn = 0;
	while (j--) {
		while (bitsIn < vw) {
			buffer = (buffer << 8) | *(raw++);
			bitsIn += 8;
		}
		bitsIn -= vw;
		*(frame++) = (buffer >> bitsIn) & mask;
	}
}

static void depth_process(freenect_device *dev, uint8_t *pkt, int len)
{
	freenect_context *ctx = dev->parent;

	if (len == 0)
		return;

	if (!dev->depth.running)
		return;

	int got_frame = stream_process(ctx, &dev->depth, pkt, len);

	if (!got_frame)
		return;

	FN_SPEW("Got depth frame %d/%d packets arrived, TS %08x\n",
	       dev->depth_stream.valid_pkts, dev->depth_stream.pkts_per_frame, dev->depth_stream.timestamp);

	switch (dev->depth_format) {
		case FREENECT_FORMAT_11_BIT:
			convert_packed_to_16bit(dev->depth.raw_buf, dev->depth.proc_buf, 11);
			break;
		case FREENECT_FORMAT_10_BIT:
			convert_packed_to_16bit(dev->depth.raw_buf, dev->depth.proc_buf, 10);
			break;
		case FREENECT_FORMAT_PACKED_10_BIT:
		case FREENECT_FORMAT_PACKED_11_BIT:
			break;
	}
	if (dev->depth_cb)
		dev->depth_cb(dev, dev->depth.proc_buf, dev->depth.timestamp);
}

static void convert_bayer_to_rgb(uint8_t *raw_buf, uint8_t *proc_buf)
{
	int x,y;
	/* Pixel arrangement:
	 * G R G R G R G R
	 * B G B G B G B G
	 * G R G R G R G R
	 * B G B G B G B G
	 * G R G R G R G R
	 * B G B G B G B G
	 *
	 * To convert a Bayer-pattern into RGB you have to handle four pattern
	 * configurations:
	 * 1)         2)         3)         4)
	 *      B1      B1 G1 B2   R1 G1 R2      R1       <- previous line
	 *   R1 G1 R2   G2 R1 G3   G2 B1 G3   B1 G1 B2    <- current line
	 *      B2      B3 G4 B4   R3 G4 R4      R2       <- next line
	 *   ^  ^  ^
	 *   |  |  next pixel
	 *   |  current pixel
	 *   previous pixel
	 *
	 * The RGB values (r,g,b) for each configuration are calculated as
	 * follows:
	 *
	 * 1) r = (R1 + R2) / 2
	 *    g =  G1
	 *    b = (B1 + B2) / 2
	 *
	 * 2) r =  R1
	 *    g = (G1 + G2 + G3 + G4) / 4
	 *    b = (B1 + B2 + B3 + B4) / 4
	 *
	 * 3) r = (R1 + R2 + R3 + R4) / 4
	 *    g = (G1 + G2 + G3 + G4) / 4
	 *    b =  B1
	 *
	 * 4) r = (R1 + R2) / 2
	 *    g =  G1
	 *    b = (B1 + B2) / 2
	 *
	 * To efficiently calculate these values, two 32bit integers are used
	 * as "shift-buffers". One integer to store the 3 horizontal bayer pixel
	 * values (previous, current, next) of the current line. The other
	 * integer to store the vertical average value of the bayer pixels
	 * (previous, current, next) of the previous and next line.
	 *
	 * The boundary conditions for the first and last line and the first
	 * and last column are solved via mirroring the second and second last
	 * line and the second and second last column.
	 *
	 * To reduce slow memory access, the values of a rgb pixel are packet
	 * into a 32bit variable and transfered together.
	 */

	uint8_t *dst = proc_buf; // pointer to destination

	uint8_t *prevLine;        // pointer to previous, current and next line
	uint8_t *curLine;         // of the source bayer pattern
	uint8_t *nextLine;

	// storing horizontal values in hVals:
	// previous << 16, current << 8, next
	uint32_t hVals;
	// storing vertical averages in vSums:
	// previous << 16, current << 8, next
	uint32_t vSums;

	// init curLine and nextLine pointers
	curLine  = raw_buf;
	nextLine = curLine + 640;
	for (y = 0; y < 480; ++y) {

		if ((y > 0) && (y < 479))
			prevLine = curLine - 640; // normal case
		else if (y == 0)
			prevLine = nextLine;      // top boundary case
		else
			nextLine = prevLine;      // bottom boundary case

		// init horizontal shift-buffer with current value
		hVals  = (*(curLine++) << 8);
		// handle left column boundary case
		hVals |= (*curLine << 16);
		// init vertical average shift-buffer with current values average
		vSums = ((*(prevLine++) + *(nextLine++)) << 7) & 0xFF00;
		// handle left column boundary case
		vSums |= ((*prevLine + *nextLine) << 15) & 0xFF0000;

		// store if line is odd or not
		uint8_t yOdd = y & 1;
		// the right column boundary case is not handled inside this loop
		// thus the "639"
		for (x = 0; x < 639; ++x) {
			// place next value in shift buffers
			hVals |= *(curLine++);
			vSums |= (*(prevLine++) + *(nextLine++)) >> 1;

			// calculate the horizontal sum as this sum is needed in
			// any configuration
			uint8_t hSum = ((uint8_t)(hVals >> 16) + (uint8_t)(hVals)) >> 1;

			if (yOdd == 0) {
				if ((x & 1) == 0) {
					// Configuration 1
					*(dst++) = hSum;		// r
					*(dst++) = hVals >> 8;	// g
					*(dst++) = vSums >> 8;	// b
				} else {
					// Configuration 2
					*(dst++) = hVals >> 8;
					*(dst++) = (hSum + (uint8_t)(vSums >> 8)) >> 1;
					*(dst++) = ((uint8_t)(vSums >> 16) + (uint8_t)(vSums)) >> 1;
				}
			} else {
				if ((x & 1) == 0) {
					// Configuration 3
					*(dst++) = ((uint8_t)(vSums >> 16) + (uint8_t)(vSums)) >> 1;
					*(dst++) = (hSum + (uint8_t)(vSums >> 8)) >> 1;
					*(dst++) = hVals >> 8;
				} else {
					// Configuration 4
					*(dst++) = vSums >> 8;
					*(dst++) = hVals >> 8;
					*(dst++) = hSum;
				}
			}

			// shift the shift-buffers
			hVals <<= 8;
			vSums <<= 8;
		} // end of for x loop
		// right column boundary case, mirroring second last column
		hVals |= (uint8_t)(hVals >> 16);
		vSums |= (uint8_t)(vSums >> 16);

		// the horizontal sum simplifies to the second last column value
		uint8_t hSum = (uint8_t)(hVals);

		if (yOdd == 0) {
			if ((x & 1) == 0) {
				*(dst++) = hSum;
				*(dst++) = hVals >> 8;
				*(dst++) = vSums >> 8;
			} else {
				*(dst++) = hVals >> 8;
				*(dst++) = (hSum + (uint8_t)(vSums >> 8)) >> 1;
				*(dst++) = vSums;
			}
		} else {
			if ((x & 1) == 0) {
				*(dst++) = vSums;
				*(dst++) = (hSum + (uint8_t)(vSums >> 8)) >> 1;
				*(dst++) = hVals >> 8;
			} else {
				*(dst++) = vSums >> 8;
				*(dst++) = hVals >> 8;
				*(dst++) = hSum;
			}
		}

	} // end of for y loop
}

static void rgb_process(freenect_device *dev, uint8_t *pkt, int len)
{
	freenect_context *ctx = dev->parent;

	if (len == 0)
		return;

	if (!dev->rgb.running)
		return;

	int got_frame = stream_process(ctx, &dev->rgb, pkt, len);

	if (!got_frame)
		return;

  //fprintf(stderr, "GOT RGB\n");
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
    dc1394_bayer_decoding_8bit( dev->rgb_raw, rgb_frame, FREENECT_FRAME_W, FREENECT_FRAME_H, DC1394_COLOR_FILTER_GRBG, DC1394_BAYER_METHOD_BILINEAR );
	}

	if (dev->rgb_cb)
		dev->rgb_cb(dev, dev->rgb.proc_buf, dev->rgb.timestamp);
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

	if (dev->depth.running)
		return -1;

	switch (dev->depth_format) {
		case FREENECT_FORMAT_11_BIT:
			stream_initbufs(ctx, &dev->depth, FREENECT_PACKED_DEPTH_11_SIZE, FREENECT_DEPTH_SIZE);
			dev->depth.pkts_per_frame = DEPTH_PKTS_11_BIT_PER_FRAME;
			break;
		case FREENECT_FORMAT_10_BIT:
			stream_initbufs(ctx, &dev->depth, FREENECT_PACKED_DEPTH_10_SIZE, FREENECT_DEPTH_SIZE);
			dev->depth.pkts_per_frame = DEPTH_PKTS_10_BIT_PER_FRAME;
			break;
		case FREENECT_FORMAT_PACKED_11_BIT:
			stream_initbufs(ctx, &dev->depth, 0, FREENECT_PACKED_DEPTH_11_SIZE);
			dev->depth.pkts_per_frame = DEPTH_PKTS_11_BIT_PER_FRAME;
			break;
		case FREENECT_FORMAT_PACKED_10_BIT:
			stream_initbufs(ctx, &dev->depth, 0, FREENECT_PACKED_DEPTH_10_SIZE);
			dev->depth.pkts_per_frame = DEPTH_PKTS_10_BIT_PER_FRAME;
			break;
	}

	dev->depth.pkt_size = DEPTH_PKTDSIZE;
	dev->depth.synced = 0;
	dev->depth.flag = 0x70;
	dev->depth.valid_frames = 0;
  dev->depth_stream.buf_ptr = dev->depth_stream.buf = dev->depth_raw;
  dev->depth_stream.state = 0;

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
	}
	write_register(dev, 0x13, 0x01);
	write_register(dev, 0x14, 0x1e);
	write_register(dev, 0x06, 0x02); // start depth stream

	dev->depth.running = 1;
	return 0;
}

int freenect_start_rgb(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;

	if (dev->rgb.running)
		return -1;

	if (dev->rgb_format == FREENECT_FORMAT_RGB)
		stream_initbufs(ctx, &dev->rgb, FREENECT_BAYER_SIZE, FREENECT_RGB_SIZE);
	else
		stream_initbufs(ctx, &dev->rgb, 0, FREENECT_BAYER_SIZE);

	dev->rgb.pkts_per_frame = RGB_PKTS_PER_FRAME;
	dev->rgb.pkt_size = RGB_PKTDSIZE;
	dev->rgb.synced = 0;
	dev->rgb.flag = 0x80;
	dev->rgb.valid_frames = 0;
	dev->rgb_stream.buf_ptr = dev->rgb_stream.buf = dev->rgb_raw;
  dev->rgb_stream.state = 0;

	res = fnusb_start_iso(&dev->usb_cam, &dev->rgb_isoc, rgb_process, 0x81, NUM_XFERS, PKTS_PER_XFER, RGB_PKTBUF);
	if (res < 0)
		return res;

	write_register(dev, 0x05, 0x00); // reset rgb stream
	write_register(dev, 0x0c, 0x00); // bayer image format
	write_register(dev, 0x0d, 0x01); // set resolution 1=640x480 2=1280x1024 3=1600x1200
	write_register(dev, 0x0e, 30); // 30Hz bayer
	write_register(dev, 0x05, 0x01); // start rgb stream
	write_register(dev, 0x47, 0x00); // disable Hflip

	dev->rgb.running = 1;
	return 0;
}

int freenect_stop_depth(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;

	if (!dev->depth.running)
		return -1;

	dev->depth.running = 0;
	write_register(dev, 0x06, 0x00); // stop depth stream

	res = fnusb_stop_iso(&dev->usb_cam, &dev->depth_isoc);
	if (res < 0) {
		FN_ERROR("Failed to stop depth isochronous stream: %d\n", res);
		return res;
	}

	stream_freebufs(ctx, &dev->depth);
	return 0;
}

int freenect_stop_rgb(freenect_device *dev)
{
	freenect_context *ctx = dev->parent;
	int res;

	if (!dev->rgb.running)
		return -1;

	dev->rgb.running = 0;
	write_register(dev, 0x05, 0x00); // stop rgb stream

	res = fnusb_stop_iso(&dev->usb_cam, &dev->rgb_isoc);
	if (res < 0) {
		FN_ERROR("Failed to stop RGB isochronous stream: %d\n", res);
		return res;
	}

	stream_freebufs(ctx, &dev->rgb);
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
	freenect_context *ctx = dev->parent;
	if (dev->rgb.running) {
		FN_ERROR("Tried to set RGB format while stream is active\n");
		return -1;
	}

	switch (fmt) {
		case FREENECT_FORMAT_RGB:
		case FREENECT_FORMAT_BAYER:
			dev->rgb_format = fmt;
			return 0;
		default:
			FN_ERROR("Invalid RGB format %d\n", fmt);
			return -1;
	}
}

int freenect_set_depth_format(freenect_device *dev, freenect_depth_format fmt)
{
	freenect_context *ctx = dev->parent;
	if (dev->depth.running) {
		FN_ERROR("Tried to set depth format while stream is active\n");
		return -1;
	}
	switch (fmt) {
		case FREENECT_FORMAT_11_BIT:
		case FREENECT_FORMAT_10_BIT:
		case FREENECT_FORMAT_PACKED_11_BIT:
		case FREENECT_FORMAT_PACKED_10_BIT:
			dev->depth_format = fmt;
			return 0;
		default:
			FN_ERROR("Invalid depth format %d\n", fmt);
			return -1;
	}
	return 0;
}

int freenect_set_depth_buffer(freenect_device *dev, void *buf)
{
	return stream_setbuf(dev->parent, &dev->depth, buf);
}

int freenect_set_rgb_buffer(freenect_device *dev, freenect_pixel *buf)
{
	return stream_setbuf(dev->parent, &dev->rgb, buf);
}
