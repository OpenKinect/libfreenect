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

// freenect-nuitest: skeleton, hand and finger tracking (libfreenect_nui) on a
// real Kinect, without a window.
//
// Usage: freenect-nuitest [seconds] [--seated] [--out PREFIX]
//
// Once a second it prints the users, how many joints each has tracked, the
// head position, the hands' states and fingers, and the time per frame.
// At the end (default after 30 s, and on Ctrl+C) it writes PREFIX.bmp
// (default "nuitest"): depth in grey, each user in its own colour, the
// skeletons in white (inferred joints in grey), fingertips as red dots.
//
// Needs Microsoft's models: freenect-driver-install --fetch-nui-models, or
// fwfetcher.py --nui, or LIBFREENECT_NUI_MODELS=DIR.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS // fopen
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libfreenect.h"
#include "libfreenect_nui.h"
#include "console_exit.h"

#ifdef _WIN32
static double now_ms(void) { return (double)GetTickCount64(); }
#else
static double now_ms(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
#endif

static uint16_t depth[640 * 480];
static unsigned depth_frames;
static int new_frame;

static void depth_cb(freenect_device* dev, void* data, uint32_t timestamp)
{
	(void)dev; (void)timestamp;
	memcpy(depth, data, sizeof(depth));
	depth_frames++;
	new_frame = 1;
}

/* ------------------------------------------------------------ picture */

static uint8_t img[640 * 480 * 3];

static void put(int x, int y, int r, int g, int b)
{
	if (x < 0 || x >= 640 || y < 0 || y >= 480)
		return;
	img[(y * 640 + x) * 3] = (uint8_t)r;
	img[(y * 640 + x) * 3 + 1] = (uint8_t)g;
	img[(y * 640 + x) * 3 + 2] = (uint8_t)b;
}

static int to_pixel(freenect_nui_point p, int* X, int* Y)
{
	if (p.z <= 0.1f)
		return 0;
	*X = 639 - (int)(p.x * 571.26f / p.z + 319.5f);
	*Y = (int)(-p.y * 571.26f / p.z + 239.5f);
	return 1;
}

static void line(int x0, int y0, int x1, int y1, int c)
{
	int dx = abs(x1 - x0), dy = -abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;
	for (;;) {
		put(x0, y0, c, c, c);
		put(x0 + 1, y0, c, c, c);
		if (x0 == x1 && y0 == y1)
			break;
		if (2 * e >= dy) { e += dy; x0 += sx; }
		if (2 * e <= dx) { e += dx; y0 += sy; }
	}
}

static void dot(int x, int y, int r, int g, int b, int rad)
{
	int i, j;
	for (j = -rad; j <= rad; j++)
		for (i = -rad; i <= rad; i++)
			if (i * i + j * j <= rad * rad)
				put(x + i, y + j, r, g, b);
}

static void write_bmp(const char* path)
{
	FILE* f = fopen(path, "wb");
	uint8_t h[54];
	uint32_t size = 54 + 640 * 480 * 3;
	int y, x;
	if (!f)
		return;
	memset(h, 0, sizeof(h));
	h[0] = 'B'; h[1] = 'M';
	h[2] = (uint8_t)size; h[3] = (uint8_t)(size >> 8); h[4] = (uint8_t)(size >> 16); h[5] = (uint8_t)(size >> 24);
	h[10] = 54; h[14] = 40; h[18] = 640 & 0xff; h[19] = 640 >> 8; h[22] = 480 & 0xff; h[23] = 480 >> 8; h[26] = 1; h[28] = 24;
	fwrite(h, 1, 54, f);
	for (y = 479; y >= 0; y--)
		for (x = 0; x < 640; x++) {
			uint8_t bgr[3];
			bgr[0] = img[(y * 640 + x) * 3 + 2];
			bgr[1] = img[(y * 640 + x) * 3 + 1];
			bgr[2] = img[(y * 640 + x) * 3];
			fwrite(bgr, 1, 3, f);
		}
	fclose(f);
}

static void draw(freenect_nui* nui, const freenect_nui_frame* fr)
{
	static uint8_t pidx[640 * 480];
	static const int col[7][3] = { { 0, 0, 0 }, { 230, 90, 60 }, { 70, 170, 240 }, { 90, 220, 90 }, { 230, 200, 60 }, { 200, 90, 220 }, { 90, 220, 210 } };
	int i, k, j;
	freenect_nui_get_player_index(nui, pidx);
	for (i = 0; i < 640 * 480; i++) {
		int d = depth[i], g = d ? 255 - (d > 6000 ? 6000 : d) * 200 / 6000 : 0, p = pidx[i];
		if (p) {
			img[i * 3] = (uint8_t)(col[p][0] * g / 255);
			img[i * 3 + 1] = (uint8_t)(col[p][1] * g / 255);
			img[i * 3 + 2] = (uint8_t)(col[p][2] * g / 255);
		} else {
			img[i * 3] = img[i * 3 + 1] = img[i * 3 + 2] = (uint8_t)g;
		}
	}
	for (k = 0; k < FREENECT_NUI_MAX_USERS; k++) {
		static const int parent[20] = { 0, 0, 1, 2, 2, 4, 5, 6, 2, 8, 9, 10, 0, 12, 13, 14, 0, 16, 17, 18 };
		const freenect_nui_skeleton* S = &fr->users[k];
		if (S->state != FREENECT_NUI_USER_TRACKED)
			continue;
		for (j = 1; j < 20; j++) {
			int x0, y0, x1, y1;
			if (!S->joint_state[j] || !S->joint_state[parent[j]])
				continue;
			if (to_pixel(S->joints[j], &x0, &y0) && to_pixel(S->joints[parent[j]], &x1, &y1))
				line(x0, y0, x1, y1, S->joint_state[j] == FREENECT_NUI_JOINT_TRACKED ? 255 : 140);
		}
		for (j = 0; j < 20; j++) {
			int x, y;
			if (S->joint_state[j] && to_pixel(S->joints[j], &x, &y))
				dot(x, y, 255, 255, 255, 3);
		}
		// the hand joints: palm to each fingertip, the thumb in yellow
		for (i = 0; i < 2; i++) {
			int palm = i ? FREENECT_NUI_PALM_RIGHT : FREENECT_NUI_PALM_LEFT, px, py, f;
			if (!S->joint_state[palm] || !to_pixel(S->joints[palm], &px, &py))
				continue;
			dot(px, py, 255, 255, 255, 2);
			for (f = 1; f <= 5; f++) {
				int x, y;
				if (S->joint_state[palm + f] && to_pixel(S->joints[palm + f], &x, &y)) {
					line(px, py, x, y, 200);
					dot(x, y, 255, f == 1 ? 220 : 30, 30, 2);
				}
			}
		}
	}
}

static const char* hand_name(int s)
{
	return s == 1 ? "open" : s == 2 ? "closed" : s == 3 ? "pointing" : s == 4 ? "gun" : "?";
}

static void print_hand(const char* side, const freenect_nui_skeleton* S, int which)
{
	static const char* fn[5] = { "thumb", "index", "middle", "ring", "little" };
	int palm = which ? FREENECT_NUI_PALM_RIGHT : FREENECT_NUI_PALM_LEFT, f, any = 0;
	printf("%s %s%s", side, hand_name(S->hand_state[which]), S->hand_holding[which] ? " (holding)" : "");
	for (f = 0; f < 5; f++)
		if (S->joint_state[palm + 1 + f] == FREENECT_NUI_JOINT_TRACKED) {
			printf("%s%s", any ? "," : " [", fn[f]);
			any = 1;
		}
	printf(any ? "]" : "");
	if (S->hand_state[which] != FREENECT_NUI_HAND_UNKNOWN && !S->hands_clasped) {
		// the palm's normal, in words (skeleton space: +x is towards the user's
		// right as they face the Kinect, +y up, +z away from the Kinect)
		const float* n = S->hand_orientation[which].matrix[2];
		float ax = fabsf(n[0]), ay = fabsf(n[1]), az = fabsf(n[2]);
		printf(", palm towards %s", az >= ax && az >= ay ? (n[2] < 0 ? "the Kinect" : "away")
		                             : ax >= ay ? (n[0] < 0 ? "the user's left" : "the user's right")
		                                        : (n[1] > 0 ? "up" : "down"));
	}
}

// a quaternion as "N degrees about (x y z)"
static void print_turn(const float q[4])
{
	float w = q[3] > 1.0f ? 1.0f : q[3] < -1.0f ? -1.0f : q[3], a = 2.0f * acosf(w), s = sinf(a / 2.0f);
	if (s < 1e-4f)
		printf("not turned");
	else
		printf("turned %.0f degrees about (%+.2f %+.2f %+.2f)", a * 57.29578f, q[0] / s, q[1] / s, q[2] / s);
}

static void print_manipulation(const freenect_nui_manipulation* m)
{
	print_turn(m->total_rotation);
	printf(", zoom x%.2f, moved (%+.0f %+.0f %+.0f) cm", m->total_scale, 100 * m->total_translation.x,
	       100 * m->total_translation.y, 100 * m->total_translation.z);
}

static const char* grip_name(freenect_nui_grip g)
{
	return g == FREENECT_NUI_GRIP_CLOSED     ? "fists"
	       : g == FREENECT_NUI_GRIP_HOLD     ? "palms facing"
	       : g == FREENECT_NUI_GRIP_TRIANGLE ? "the thumb and index triangle"
	                                         : "?";
}

static void print_events(int user, int hand, uint32_t ev)
{
	static const struct {
		uint32_t flag;
		const char* name;
	} names[] = {
		{ FREENECT_NUI_EVENT_SWIPE_LEFT, "swipe left" }, { FREENECT_NUI_EVENT_SWIPE_RIGHT, "swipe right" },
		{ FREENECT_NUI_EVENT_SWIPE_UP, "swipe up" },     { FREENECT_NUI_EVENT_SWIPE_DOWN, "swipe down" },
		{ FREENECT_NUI_EVENT_PRESS, "press" },           { FREENECT_NUI_EVENT_PRESS_END, "press end" },
		{ FREENECT_NUI_EVENT_GRIP, "grip" },             { FREENECT_NUI_EVENT_GRIP_END, "grip end (drop)" },
	};
	unsigned i;
	for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if (ev & names[i].flag)
			printf("    [user %d %s hand: %s]\n", user + 1, hand ? "right" : "left", names[i].name);
}

/* ------------------------------------------------------------ main */

int main(int argc, char** argv)
{
	int seconds = 30, seated = 0, i, ret = 1;
	const char* prefix = "nuitest";
	char path[512];
	freenect_context* ctx = NULL;
	freenect_device* dev = NULL;
	freenect_nui* nui;
	freenect_nui_frame fr;
	time_t start, last;
	double busy = 0.0;
	unsigned tracked_frames = 0, nframes = 0, sec_frames = 0;
	static freenect_nui_manipulation last_manip[FREENECT_NUI_MAX_USERS];

	fn_install_exit_handlers();
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--seated") == 0) seated = 1;
		else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) prefix = argv[++i];
		else if (argv[i][0] >= '0' && argv[i][0] <= '9') seconds = atoi(argv[i]);
		else {
			printf("Usage: freenect-nuitest [seconds] [--seated] [--out PREFIX]\n");
			return 2;
		}
	}
	nui = freenect_nui_create(NULL, seated ? FREENECT_NUI_SEATED : 0);
	if (!nui) {
		printf("The skeleton tracking model is not installed. Fetch it with\n"
		       "  freenect-driver-install --fetch-nui-models   (Windows)\n"
		       "  fwfetcher.py --nui --user                     (Linux, macOS)\n");
		return 1;
	}
	memset(&fr, 0, sizeof(fr));
	if (freenect_init(&ctx, NULL) < 0)
		goto out;
	freenect_select_subdevices(ctx, FREENECT_DEVICE_CAMERA);
	if (freenect_num_devices(ctx) < 1 || freenect_open_device(ctx, &dev, 0) < 0) {
		printf("No Kinect found\n");
		goto out;
	}
	freenect_set_depth_mode(dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM));
	freenect_set_depth_range(dev, FREENECT_DEPTH_RANGE_FULL);
	freenect_set_depth_callback(dev, depth_cb);
	if (freenect_start_depth(dev) < 0) {
		printf("Could not start depth\n");
		goto out;
	}
	printf("Tracking%s for %d s; stand 1.5-3.5 m in front of the Kinect\n", seated ? " (seated)" : "", seconds);
	fflush(stdout);

	start = last = time(NULL);
	while (!fn_exit_requested && time(NULL) - start < seconds) {
		struct timeval tv = { 0, 50000 };
		time_t t;
		if (freenect_process_events_timeout(ctx, &tv) < 0)
			break;
		if (new_frame) {
			double t0 = now_ms();
			int k;
			new_frame = 0;
			freenect_nui_process(nui, depth, depth_frames, &fr);
			busy += now_ms() - t0;
			nframes++;
			sec_frames++;
			for (k = 0; k < FREENECT_NUI_MAX_USERS; k++)
				if (fr.users[k].state == FREENECT_NUI_USER_TRACKED) {
					tracked_frames++;
					break;
				}
			// two-hand manipulation: report when an object is grabbed and let go
			for (k = 0; k < FREENECT_NUI_MAX_USERS; k++) {
				freenect_nui_manipulation m;
				if (freenect_nui_get_manipulation(nui, k, &m) < 0)
					continue;
				if (m.began)
					printf("    [user %d grabs with %s, hands %.0f cm apart]\n", k + 1, grip_name(m.grip), 100 * m.distance);
				if (m.active)
					last_manip[k] = m;
				if (m.ended) {
					printf("    [user %d lets go: ", k + 1);
					print_manipulation(&last_manip[k]);
					printf("]\n");
				}
			}
			// one-hand events: swipes, presses, grips
			for (k = 0; k < FREENECT_NUI_MAX_USERS; k++) {
				freenect_nui_hand_pointer hp[2];
				int h;
				if (freenect_nui_get_hand_pointers(nui, k, hp) < 0)
					continue;
				for (h = 0; h < 2; h++)
					print_events(k, h, hp[h].events);
			}
		}
		t = time(NULL);
		if (t != last) {
			int k, any = 0;
			printf("%3ld s: %u frames, %.1f ms per frame; floor %s\n", (long)(t - start), sec_frames,
			       nframes ? busy / nframes : 0.0, fr.floor_plane[1] != 0.0f ? "found" : "not found");
			for (k = 0; k < FREENECT_NUI_MAX_USERS; k++) {
				const freenect_nui_skeleton* S = &fr.users[k];
				int j, nt = 0;
				if (S->state == FREENECT_NUI_USER_NOT_TRACKED)
					continue;
				any = 1;
				for (j = 0; j < 20; j++)
					nt += S->joint_state[j] == FREENECT_NUI_JOINT_TRACKED;
				printf("    user %d (id %u) at %.2f m: %s", k + 1, S->tracking_id, S->position.z,
				       S->state == FREENECT_NUI_USER_TRACKED ? "skeleton" : "position only");
				if (S->state == FREENECT_NUI_USER_TRACKED) {
					printf(", %d joints tracked, head (%+.2f %+.2f %.2f)", nt, S->joints[3].x, S->joints[3].y, S->joints[3].z);
					if (S->quality)
						printf(", clipped%s%s%s%s", S->quality & 1 ? " right" : "", S->quality & 2 ? " left" : "",
						       S->quality & 4 ? " top" : "", S->quality & 8 ? " bottom" : "");
					printf("\n        hands: ");
					print_hand("left", S, 0);
					print_hand("; right", S, 1);
					if (S->hands_clasped) {
						freenect_nui_clasp c;
						freenect_nui_get_clasp(nui, k, &c);
						printf("\n        hands clasped: %s", c.state == FREENECT_NUI_HAND_GUN || c.state == FREENECT_NUI_HAND_POINTING
						                                      ? (c.towards_kinect ? "two-hand gun, at the Kinect" : "two-hand gun")
						                                      : "nothing out");
						if (c.state == FREENECT_NUI_HAND_GUN || c.state == FREENECT_NUI_HAND_POINTING)
							printf(", pointing (%+.2f %+.2f %+.2f)", c.direction.x, c.direction.y, c.direction.z);
					}
					{
						freenect_nui_manipulation m;
						if (freenect_nui_get_manipulation(nui, k, &m) == 0 && m.active) {
							printf("\n        holding (%s, %.0f cm apart): ", grip_name(m.grip), 100 * m.distance);
							print_manipulation(&m);
						}
					}
					{
						freenect_nui_hand_pointer hp[2];
						int h;
						if (freenect_nui_get_hand_pointers(nui, k, hp) == 0)
							for (h = 0; h < 2; h++)
								if (hp[h].primary)
									printf("\n        pointer: %s hand, cursor (%.2f %.2f), press %.2f%s%s", h ? "right" : "left",
									       hp[h].x, hp[h].y, hp[h].press, hp[h].pressed ? ", pressed" : "",
									       hp[h].gripped ? ", gripped" : "");
					}
				}
				printf("\n");
			}
			if (!any)
				printf("    no user\n");
			fflush(stdout);
			sec_frames = 0;
			last = t;
		}
	}
	freenect_stop_depth(dev);
	draw(nui, &fr);
	snprintf(path, sizeof(path), "%s.bmp", prefix);
	write_bmp(path);
	printf("Frames: %u, with a skeleton: %u; %.1f ms per frame. Wrote %s\n",
	       nframes, tracked_frames, nframes ? busy / nframes : 0.0, path);
	ret = 0;
out:
	if (dev)
		freenect_close_device(dev);
	if (ctx)
		freenect_shutdown(ctx);
	freenect_nui_destroy(nui);
	printf("Done!\n");
	return fn_exit_done(ret);
}
