/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2026 individual OpenKinect contributors. See the CONTRIB file
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
 * Host-only test of frame reassembly (src/stream.c) on synthetic "RB"
 * packets: only complete frames are delivered (§10A.4), and the first video
 * frames after a start are skipped (§10D.1.3).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freenect_internal.h"

static int pass = 1;
#define CHECK(cond, what) do { int ok_ = (cond); printf("  %-62s %s\n", what, ok_ ? "PASS" : "FAIL"); pass &= ok_; } while (0)

uint64_t fn_now_us(void) { static uint64_t t; return t += 1000; }

void fn_log(freenect_context *ctx, freenect_loglevel level, const char *fmt, ...)
{
	(void)ctx; (void)level; (void)fmt;
}

/* A small frame: 5 packets of up to 100 data bytes, the last one 50. */
#define PKT_DATA 100
#define FRAME 450
#define NPKTS 5
#define FLAG 0x70

enum { NONE = -1 };
/* What to do to one packet of a frame. */
typedef struct {
	int lose;      /* packet index never sent (NONE: all sent) */
	int lose2;     /* a second lost packet index */
	int bad_magic; /* packet index sent with a broken header */
	int shorten;   /* packet index sent 10 bytes short */
} damage;
static const damage intact = { NONE, NONE, NONE, NONE };

static freenect_context ctx_storage;
static freenect_context *ctx = &ctx_storage;
static uint8_t seq;
static int delivered; /* frames stream_process() delivered so far */

static int send_pkt(packet_stream *s, int idx, int frame_no, int bad, int shortby)
{
	uint8_t pkt[12 + PKT_DATA];
	int n = idx == NPKTS - 1 ? FRAME - PKT_DATA * (NPKTS - 1) : PKT_DATA;
	uint8_t type = idx == 0 ? FLAG | 1 : idx == NPKTS - 1 ? FLAG | 5 : FLAG | 2;
	memset(pkt, 0, 12);
	pkt[0] = bad ? 0xff : 'R';
	pkt[1] = bad ? 0xff : 'B';
	pkt[3] = type;
	pkt[5] = seq++;
	n -= shortby;
	memset(pkt + 12, (uint8_t)(frame_no * 16 + idx), n);
	n = stream_process(ctx, s, pkt, 12 + n, NULL, NULL);
	if (n)
		delivered++;
	return n;
}

/* Sends one frame; returns the size stream_process() delivered (0: none). */
static int send_frame(packet_stream *s, int frame_no, damage d)
{
	int i, got = 0;
	for (i = 0; i < NPKTS; i++) {
		int r;
		if (i == d.lose || i == d.lose2) {
			seq++; /* the device sent it; it never arrived */
			continue;
		}
		r = send_pkt(s, i, frame_no, i == d.bad_magic, i == d.shorten ? 10 : 0);
		if (r)
			got = r;
	}
	return got;
}

static int frame_is(packet_stream *s, int frame_no)
{
	int i;
	for (i = 0; i < FRAME; i++)
		if (s->raw_buf[i] != (uint8_t)(frame_no * 16 + i / PKT_DATA))
			return 0;
	return 1;
}

static void start(packet_stream *s, int skip)
{
	memset(s, 0, sizeof(*s));
	s->flag = FLAG;
	s->pkt_size = PKT_DATA;
	stream_init(ctx, s, 0, FRAME);
	s->skip_frames = skip;
	s->running = 1;
}

static void stop(packet_stream *s)
{
	stream_freebufs(ctx, s);
}

int main(void)
{
	packet_stream s;
	damage d;
	int r1, r2, r3;

	printf("Frame reassembly (src/stream.c)\n");

	start(&s, 0);
	r1 = send_frame(&s, 1, intact);
	CHECK(r1 == FRAME && frame_is(&s, 1), "a complete frame is delivered, with its data");
	r1 = send_frame(&s, 2, intact);
	CHECK(r1 == FRAME && frame_is(&s, 2), "and the next one");
	CHECK(s.delivered == 2 && s.eof_us > s.sof_us && s.mid_us == s.sof_us + (s.eof_us - s.sof_us) / 2,
	      "delivered frames are counted and get a midpoint time");

	d = intact; d.lose = 2;
	r1 = send_frame(&s, 3, d);
	r2 = send_frame(&s, 4, intact);
	CHECK(r1 == 0, "a frame with a lost middle packet is dropped");
	CHECK(r2 == FRAME && frame_is(&s, 4), "the next complete frame is delivered");

	d = intact; d.lose = NPKTS - 1;
	r1 = send_frame(&s, 5, d);
	r2 = send_frame(&s, 6, intact);
	CHECK(r1 == 0 && r2 == FRAME && frame_is(&s, 6), "lost end of frame: that frame dropped, the next delivered");

	/* end of frame 7 and start of frame 8 lost together */
	d = intact; d.lose = NPKTS - 1;
	r1 = send_frame(&s, 7, d);
	d = intact; d.lose = 0;
	r2 = send_frame(&s, 8, d);
	r3 = send_frame(&s, 9, intact);
	CHECK(r1 == 0 && r2 == 0, "lost end and next start: both frames dropped");
	CHECK(r3 == FRAME && frame_is(&s, 9), "the frame after them is delivered");

	d = intact; d.bad_magic = 1;
	r1 = send_frame(&s, 10, d);
	r2 = send_frame(&s, 11, intact);
	CHECK(r1 == 0 && r2 == FRAME, "a packet with an invalid header drops its frame");
	CHECK(s.bad_magic == 1, "and is counted");

	d = intact; d.shorten = 2;
	r1 = send_frame(&s, 12, d);
	r2 = send_frame(&s, 13, intact);
	CHECK(r1 == 0 && r2 == FRAME, "a short middle packet drops its frame");
	CHECK(s.dropped_frames == 6, "every dropped frame is counted");
	stop(&s);

	/* a stream whose frames never end where expected: start, two middles,
	 * then a start again where the third middle should be */
	start(&s, 0);
	{
		int k;
		for (k = 0; k < FN_STREAM_STALL_PKTS + 10; k++)
			send_pkt(&s, k % 3 == 0 ? 0 : 1, 30, 0, 0);
	}
	CHECK(s.valid_frames == 0 && s.stall_reported == 1, "no complete frame for a few seconds: reported once");
	CHECK(s.resyncs_order > 0 && s.last_order_flag == (FLAG | 1) && s.last_order_pos == 3,
	      "  with the out-of-order packet's type and position");
	CHECK(!s.restart_wanted, "  (valid headers: no restart)");
	stop(&s);

	/* a garbled stream: nothing but packets without the "RB" header */
	start(&s, 0);
	{
		int k;
		for (k = 0; k < FN_STREAM_GARBLED_PKTS - 1; k++)
			send_pkt(&s, 1, 40, 1, 0);
		CHECK(!s.restart_wanted, "a run of packets without a header, short of the limit: no restart");
		send_pkt(&s, 1, 40, 0, 0);
		for (k = 0; k < FN_STREAM_GARBLED_PKTS - 1; k++)
			send_pkt(&s, 1, 40, 1, 0);
		CHECK(!s.restart_wanted, "  one valid packet starts the count again");
		send_pkt(&s, 1, 40, 1, 0);
		CHECK(s.restart_wanted, "FN_STREAM_GARBLED_PKTS in a row: the stream asks for a restart");
	}
	stop(&s);

	/* a stream that starts mid-frame syncs on the next start of frame */
	start(&s, 0);
	seq = 40;
	send_pkt(&s, 3, 20, 0, 0);
	send_pkt(&s, 4, 20, 0, 0);
	r1 = send_frame(&s, 21, intact);
	CHECK(r1 == FRAME && frame_is(&s, 21), "joining mid-frame: the first whole frame is delivered");
	stop(&s);

	start(&s, 2);
	r1 = send_frame(&s, 1, intact);
	r2 = send_frame(&s, 2, intact);
	r3 = send_frame(&s, 3, intact);
	CHECK(r1 == 0 && r2 == 0 && r3 == FRAME && frame_is(&s, 3), "skip_frames = 2: the third frame is the first delivered");
	stop(&s);

	start(&s, 2);
	d = intact; d.lose = 1;
	send_frame(&s, 1, d);
	r1 = send_frame(&s, 2, intact);
	r2 = send_frame(&s, 3, intact);
	r3 = send_frame(&s, 4, intact);
	CHECK(r1 == 0 && r2 == 0 && r3 == FRAME, "a dropped frame does not count as a skipped one");
	stop(&s);

#ifdef _WIN32
	_putenv("LIBFREENECT_INCOMPLETE_FRAMES=1");
#else
	setenv("LIBFREENECT_INCOMPLETE_FRAMES", "1", 1);
#endif
	start(&s, 0);
	send_frame(&s, 1, intact);
	d = intact; d.lose = 2;
	r1 = send_frame(&s, 2, d);
	d = intact; d.lose = NPKTS - 1;
	r2 = delivered;
	send_frame(&s, 3, d);           /* delivered when frame 4 starts */
	send_frame(&s, 4, intact);
	CHECK(r1 == FRAME, "LIBFREENECT_INCOMPLETE_FRAMES=1: a frame with a hole is delivered");
	CHECK(delivered - r2 == 2, "  and one whose end was lost, as before");
	CHECK(s.dropped_frames == 0, "  and none is dropped");
	stop(&s);

	printf(pass ? "All passed\n" : "FAILURES\n");
	return pass ? 0 : 1;
}
