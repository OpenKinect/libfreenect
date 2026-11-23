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

/*
 * Reassembly of depth and video frames from the camera's isochronous packets
 * (moved here from cameras.c, so that it can be tested without a Kinect).
 *
 * Each packet starts with a 12-byte header: magic "RB", a zero byte, the
 * type (stream flag | 1 start of frame, | 2 middle, | 5 end of frame), a
 * sequence number and the device's time stamp.
 *
 * A frame is delivered only if every one of its packets arrived complete,
 * as the Microsoft driver does: it completes a frame only when exactly the
 * expected number of bytes arrived. A frame with a lost, malformed or short
 * packet has holes filled with whatever the buffer held before, so it is
 * dropped. LIBFREENECT_INCOMPLETE_FRAMES=1 delivers such frames anyway, as
 * libfreenect did before.
 */

#include <stdlib.h>
#include <string.h>

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

static int is_power_of_ten(unsigned int n)
{
	while (n >= 10 && n % 10 == 0)
		n /= 10;
	return n == 1;
}

// The end of a frame was reached (or lost). Returns the size to deliver, or 0
// if the frame is dropped.
static int frame_end(freenect_context *ctx, packet_stream *strm, int size, freenect_loglevel l_notice)
{
	int bad = strm->frame_bad;
	strm->frame_bad = 0;
	strm->valid_frames++;
	if (bad && !strm->keep_incomplete) {
		strm->dropped_frames++;
		// 1st, 10th, 100th...: visible, but never a flood
		if (is_power_of_ten(strm->dropped_frames))
			FN_LOG(l_notice, "[Stream %02x] Dropped an incomplete frame (%u so far)\n", strm->flag, strm->dropped_frames);
		return 0;
	}
	if (strm->skip_frames > 0) {
		strm->skip_frames--;
		FN_SPEW("[Stream %02x] Skipping a frame after the stream start\n", strm->flag);
		return 0;
	}
	strm->mid_us = strm->sof_us + (strm->eof_us - strm->sof_us) / 2;
	strm->delivered++;
	return size;
}

FN_INTERNAL int stream_process(freenect_context *ctx, packet_stream *strm, uint8_t *pkt, int len, freenect_chunk_cb cb, void *user_data)
{
	if (len < 12)
		return 0;

	struct pkt_hdr *hdr = (struct pkt_hdr*)pkt;
	uint8_t *data = pkt + sizeof(*hdr);
	int datalen = len - sizeof(*hdr);

	freenect_loglevel l_debug = LL_DEBUG;
	freenect_loglevel l_info = LL_INFO;
	freenect_loglevel l_notice = LL_NOTICE;
	freenect_loglevel l_warning = LL_WARNING;
	// A stream start always loses a few packets: stay quiet about that, but
	// only for about a second, so that a stream that never gets going still
	// says why.
	strm->pkts_seen++;
	if (strm->valid_frames < 2 && strm->pkts_seen < FN_STREAM_QUIET_PKTS)
		l_debug = l_info = l_notice = l_warning = LL_SPEW;
	if (!strm->stall_reported && strm->valid_frames == 0 && strm->pkts_seen >= FN_STREAM_STALL_PKTS) {
		strm->stall_reported = 1;
		FN_WARNING("[Stream %02x] No complete frame after %u packets: %u lost, %u with an invalid header, "
		           "%u too long, %u too short; %u resyncs after lost packets, %u after out-of-order packets "
		           "(last: type %02x at packet %d of %d)\n",
		           strm->flag, strm->pkts_seen, strm->lost_pkts, strm->bad_magic, strm->oversized, strm->short_pkts,
		           strm->resyncs_lost, strm->resyncs_order, strm->last_order_flag, strm->last_order_pos,
		           strm->pkts_per_frame);
	}

	if (hdr->magic[0] != 'R' || hdr->magic[1] != 'B') {
		// Dropped; the sequence gap it leaves marks the frame incomplete.
		strm->bad_magic++;
		if (++strm->bad_run == FN_STREAM_GARBLED_PKTS)
			strm->restart_wanted = 1;
		if (is_power_of_ten(strm->bad_magic))
			FN_LOG(l_notice, "[Stream %02x] Invalid magic %02x%02x in a %d-byte packet (%u so far)\n",
			       strm->flag, hdr->magic[0], hdr->magic[1], len, strm->bad_magic);
		return 0;
	}
	strm->bad_run = 0;

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
		strm->frame_bad = 0;
	}

	int got_frame_size = 0;

	// handle lost packets
	if (strm->seq != hdr->seq) {
		uint8_t lost = hdr->seq - strm->seq;
		strm->lost_pkts += lost;
		FN_LOG(l_info, "[Stream %02x] Lost %d packets\n", strm->flag, lost);

		FN_LOG(l_debug, "[Stream %02x] Lost %d total packets in %d frames (%f lppf)\n",
			strm->flag, strm->lost_pkts, strm->valid_frames, (float)strm->lost_pkts / strm->valid_frames);

		if (lost > 5 || strm->variable_length) {
			FN_LOG(l_notice, "[Stream %02x] Lost too many packets, resyncing...\n", strm->flag);
			strm->resyncs_lost++;
			strm->synced = 0;
			return 0;
		}
		strm->seq = hdr->seq;
		int left = strm->pkts_per_frame - strm->pkt_num;
		if (left <= lost) {
			// The rest of the open frame, its end included, was lost.
			strm->frame_bad = 1;
			strm->pkt_num = lost - left;
			strm->valid_pkts = strm->got_pkts;
			strm->got_pkts = 0;
			strm->timestamp = strm->last_timestamp;
			got_frame_size = frame_end(ctx, strm, strm->frame_size, l_notice);
			// The next frame is whole only if exactly that rest was lost.
			strm->frame_bad = strm->pkt_num != 0;
		} else {
			strm->pkt_num += lost;
			strm->frame_bad = 1;
		}
	}

	int expected_pkt_size = (strm->pkt_num == strm->pkts_per_frame-1) ? strm->last_pkt_size : strm->pkt_size;

	if (!strm->variable_length) {
		// check the header to make sure it's what we expect
		if (!(strm->pkt_num == 0 && hdr->flag == sof) &&
		    !(strm->pkt_num == strm->pkts_per_frame-1 && hdr->flag == eof) &&
		    !(strm->pkt_num > 0 && strm->pkt_num < strm->pkts_per_frame-1 && hdr->flag == mof)) {
			FN_LOG(l_notice, "[Stream %02x] Inconsistent flag %02x with %d packets in buf (%d total), resyncing...\n",
			       strm->flag, hdr->flag, strm->pkt_num, strm->pkts_per_frame);
			strm->resyncs_order++;
			strm->last_order_flag = hdr->flag;
			strm->last_order_pos = strm->pkt_num;
			strm->synced = 0;
			return got_frame_size;
		}
		// check data length
		if (datalen > expected_pkt_size) {
			FN_LOG(l_warning, "[Stream %02x] Expected max %d data bytes, but got %d. Dropping...\n",
			       strm->flag, expected_pkt_size, datalen);
			strm->oversized++;
			// Its place in the frame stays unfilled: the sequence gap the
			// next packet shows marks the frame incomplete.
			return got_frame_size;
		}
		if (datalen < expected_pkt_size) {
			FN_LOG(l_warning, "[Stream %02x] Expected %d data bytes, but got %d\n",
			       strm->flag, expected_pkt_size, datalen);
			strm->short_pkts++;
			strm->frame_bad = 1;
		}
	} else {
		// check the header to make sure it's what we expect
		if (!(strm->pkt_num == 0 && hdr->flag == sof) &&
		    !(strm->pkt_num < strm->pkts_per_frame && (hdr->flag == eof || hdr->flag == mof))) {
			FN_LOG(l_notice, "[Stream %02x] Inconsistent flag %02x with %d packets in buf (%d total), resyncing...\n",
			       strm->flag, hdr->flag, strm->pkt_num, strm->pkts_per_frame);
			strm->resyncs_order++;
			strm->last_order_flag = hdr->flag;
			strm->last_order_pos = strm->pkt_num;
			strm->synced = 0;
			return got_frame_size;
		}
		// check data length
		if (datalen > expected_pkt_size) {
			FN_LOG(l_warning, "[Stream %02x] Expected max %d data bytes, but got %d. Resyncng...\n",
			       strm->flag, expected_pkt_size, datalen);
			strm->synced = 0;
			return got_frame_size;
		}
		if (datalen < expected_pkt_size && hdr->flag != eof) {
			FN_LOG(l_warning, "[Stream %02x] Expected %d data bytes, but got %d. Resyncing...\n",
			       strm->flag, expected_pkt_size, datalen);
			strm->synced = 0;
			return got_frame_size;
		}
	}

	// copy or chunk process the data
	uint8_t *dbuf = strm->raw_buf + strm->pkt_num * strm->pkt_size;
	if(cb){
		cb(strm->raw_buf,data,strm->pkt_num,datalen,user_data);
	}else{
		memcpy(dbuf, data, datalen);
	}

	strm->pkt_num++;
	strm->seq++;
	strm->got_pkts++;

	strm->last_timestamp = fn_le32(hdr->timestamp);

	if (hdr->flag == sof)
		strm->sof_us = fn_now_us();

	if (hdr->flag == eof) {
		strm->eof_us = fn_now_us();
		int size;
		if (strm->variable_length)
			size = (int)(dbuf - strm->raw_buf) + datalen;
		else
			size = (int)(dbuf - strm->raw_buf) + strm->last_pkt_size;
		strm->pkt_num = 0;
		strm->valid_pkts = strm->got_pkts;
		strm->got_pkts = 0;
		strm->timestamp = strm->last_timestamp;
		got_frame_size = frame_end(ctx, strm, size, l_notice);
	}

	return got_frame_size;
}

FN_INTERNAL void stream_init(freenect_context *ctx, packet_stream *strm, int rlen, int plen)
{
	(void)ctx;
	const char *keep = getenv("LIBFREENECT_INCOMPLETE_FRAMES");

	strm->valid_frames = 0;
	strm->synced = 0;
	strm->frame_bad = 0;
	strm->dropped_frames = 0;
	strm->bad_magic = 0;
	strm->sof_us = strm->eof_us = strm->mid_us = 0;
	strm->delivered = 0;
	strm->pkts_seen = 0;
	strm->lost_pkts = 0;
	strm->oversized = strm->short_pkts = 0;
	strm->resyncs_lost = strm->resyncs_order = 0;
	strm->last_order_flag = 0;
	strm->last_order_pos = 0;
	strm->stall_reported = 0;
	strm->bad_run = 0;
	strm->restart_wanted = 0;
	strm->keep_incomplete = keep && strcmp(keep, "1") == 0;
	// strm->skip_frames is set by the caller after stream_init()

	if (strm->usr_buf) {
		strm->lib_buf = NULL;
		strm->proc_buf = strm->usr_buf;
	} else {
		strm->lib_buf = malloc(plen);
		strm->proc_buf = strm->lib_buf;
	}

	if (rlen == 0) {
		strm->split_bufs = 0;
		strm->raw_buf = (uint8_t*)strm->proc_buf;
		strm->frame_size = plen;
	} else {
		strm->split_bufs = 1;
		strm->raw_buf = (uint8_t*)malloc(rlen);
		strm->frame_size = rlen;
	}

	strm->last_pkt_size = strm->frame_size % strm->pkt_size;
	if (strm->last_pkt_size == 0)
		strm->last_pkt_size = strm->pkt_size;
	strm->pkts_per_frame = (strm->frame_size + strm->pkt_size - 1) / strm->pkt_size;
}

FN_INTERNAL void stream_freebufs(freenect_context *ctx, packet_stream *strm)
{
	if (strm->pkts_seen > 0)
		FN_INFO("[Stream %02x] %d frames, %u dropped as incomplete; packets: %u lost, %u with an invalid header, "
		        "%u too long, %u too short; %u resyncs\n",
		        strm->flag, strm->valid_frames, strm->dropped_frames, strm->lost_pkts, strm->bad_magic,
		        strm->oversized, strm->short_pkts, strm->resyncs_lost + strm->resyncs_order);

	if (strm->split_bufs)
		free(strm->raw_buf);
	if (strm->lib_buf)
		free(strm->lib_buf);

	strm->raw_buf = NULL;
	strm->proc_buf = NULL;
	strm->lib_buf = NULL;
}

FN_INTERNAL int stream_setbuf(freenect_context *ctx, packet_stream *strm, void *pbuf)
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
			strm->raw_buf = (uint8_t*)strm->proc_buf;
		return 0;
	}
}
