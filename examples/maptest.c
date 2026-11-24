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

// Usage: freenect-maptest [--hires] [--mic] [--out PREFIX]
//
// Checks the coordinate mapper on a real Kinect. It streams depth (mm) and
// colour for a moment, takes one frame of each, and writes:
//   PREFIX_color.bmp    the colour frame
//   PREFIX_overlay.bmp  the colour frame with depth edges drawn in red where
//                       the mapper puts them: they should lie on the outlines
//                       of the objects in the picture
//   PREFIX_depth.bmp    depth mapped onto the colour frame, near = bright
//   PREFIX_calibration.bin  the Kinect's calibration (freenect_mapper_get_calibration)
// and prints a few mapped points. PREFIX defaults to "maptest".
//
//   --hires  colour at 1280x1024 (1280x960 on the 1473/K4W)
//   --mic    also open the audio controller and print the microphone geometry
//
// It ends by itself after at most 15 s, and on Ctrl+C.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS // fopen
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libfreenect.h"
#include "libfreenect_audio.h"
#include "libfreenect_coordinate_mapper.h"
#include "console_exit.h"

#define DW 640
#define DH 480

static uint16_t* depth_copy;
static uint8_t* rgb_copy;
static int cw, ch;
static unsigned depth_frames, video_frames;

static void depth_cb(freenect_device* dev, void* data, uint32_t timestamp)
{
	(void)dev; (void)timestamp;
	memcpy(depth_copy, data, DW * DH * sizeof(uint16_t));
	depth_frames++;
}

static void video_cb(freenect_device* dev, void* data, uint32_t timestamp)
{
	(void)dev; (void)timestamp;
	memcpy(rgb_copy, data, (size_t)cw * ch * 3);
	video_frames++;
}

// 24-bit BMP, bottom-up, rows padded to 4 bytes; Windows opens it directly.
static int write_bmp(const char* path, const uint8_t* rgb, int w, int h)
{
	FILE* f = fopen(path, "wb");
	int row = (w * 3 + 3) & ~3, y, x;
	uint32_t size = 54 + (uint32_t)row * h;
	uint8_t hd[54], pad[3] = { 0, 0, 0 };
	if (!f)
		return -1;
	memset(hd, 0, sizeof(hd));
	hd[0] = 'B'; hd[1] = 'M';
	hd[2] = (uint8_t)size; hd[3] = (uint8_t)(size >> 8); hd[4] = (uint8_t)(size >> 16); hd[5] = (uint8_t)(size >> 24);
	hd[10] = 54; hd[14] = 40;
	hd[18] = (uint8_t)w; hd[19] = (uint8_t)(w >> 8);
	hd[22] = (uint8_t)h; hd[23] = (uint8_t)(h >> 8);
	hd[26] = 1; hd[28] = 24;
	fwrite(hd, 1, 54, f);
	for (y = h - 1; y >= 0; y--) {
		for (x = 0; x < w; x++) {
			const uint8_t* p = rgb + ((size_t)y * w + x) * 3;
			uint8_t bgr[3];
			bgr[0] = p[2]; bgr[1] = p[1]; bgr[2] = p[0];
			fwrite(bgr, 1, 3, f);
		}
		fwrite(pad, 1, (size_t)(row - w * 3), f);
	}
	fclose(f);
	return 0;
}

static void print_mic(freenect_device* dev)
{
	freenect_mic_geometry g;
	int r = freenect_get_mic_geometry(dev, &g), i;
	printf("Microphone array (%s): %d microphones, %s, %.0f..%.0f degrees, %u-%u Hz\n",
	       r == 1 ? "read from the Kinect" : "standard values; the Kinect did not answer",
	       g.num_mics, g.array_type == 0 ? "linear" : "not linear",
	       g.horizontal_angle_begin * 1e-4 * 57.29578, g.horizontal_angle_end * 1e-4 * 57.29578,
	       g.freq_low_hz, g.freq_high_hz);
	for (i = 0; i < g.num_mics; i++)
		printf("  mic %d: type %u at x %d, y %d, z %d mm\n", i + 1, g.mics[i].type,
		       g.mics[i].x_mm, g.mics[i].y_mm, g.mics[i].z_mm);
	printf("libfreenect audio streaming on this device: %s\n",
	       freenect_audio_streaming_available(dev) ? "available" : "not available");
}

int main(int argc, char** argv)
{
	int hires = 0, mic = 0, i, ret = 1;
	const char* prefix = "maptest";
	char path[512];
	freenect_context* ctx = NULL;
	freenect_device* dev = NULL;
	freenect_coordinate_mapper *m = NULL, *m2 = NULL;
	freenect_frame_mode vmode;
	freenect_resolution cres;
	time_t start;

	fn_install_exit_handlers();
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--hires") == 0) hires = 1;
		else if (strcmp(argv[i], "--mic") == 0) mic = 1;
		else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) prefix = argv[++i];
		else {
			printf("Usage: freenect-maptest [--hires] [--mic] [--out PREFIX]\n");
			return 2;
		}
	}

	if (freenect_init(&ctx, NULL) < 0)
		return 1;
	freenect_set_log_level(ctx, FREENECT_LOG_INFO);
	freenect_select_subdevices(ctx, (freenect_device_flags)(FREENECT_DEVICE_CAMERA | (mic ? FREENECT_DEVICE_AUDIO : 0)));
	if (freenect_num_devices(ctx) < 1) {
		printf("No device found!\n");
		freenect_shutdown(ctx);
		return 1;
	}
	if (freenect_open_device(ctx, &dev, 0) < 0) {
		printf("Could not open the Kinect\n");
		freenect_shutdown(ctx);
		return 1;
	}
	if (mic)
		print_mic(dev);

	vmode = freenect_find_video_mode(hires ? FREENECT_RESOLUTION_HIGH : FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
	if (!vmode.is_valid || freenect_set_video_mode(dev, vmode) < 0 ||
	    freenect_set_depth_mode(dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM)) < 0) {
		printf("Modes not available\n");
		goto out;
	}
	cw = vmode.width;
	ch = vmode.height;
	cres = hires ? FREENECT_RESOLUTION_HIGH : FREENECT_RESOLUTION_MEDIUM;
	depth_copy = (uint16_t*)calloc(DW * DH, sizeof(uint16_t));
	rgb_copy = (uint8_t*)calloc((size_t)cw * ch, 3);
	if (!depth_copy || !rgb_copy)
		goto out;
	freenect_set_depth_range(dev, FREENECT_DEPTH_RANGE_FULL);
	freenect_set_depth_callback(dev, depth_cb);
	freenect_set_video_callback(dev, video_cb);
	if (freenect_start_depth(dev) < 0 || freenect_start_video(dev) < 0) {
		printf("Could not start the streams\n");
		goto out;
	}
	printf("Depth 640x480 mm, colour %dx%d; waiting for frames...\n", cw, ch);
	fflush(stdout);

	// A few seconds, so that exposure and depth have settled; at most 15 s.
	start = time(NULL);
	while (!fn_exit_requested && time(NULL) - start < 15) {
		struct timeval tv = { 0, 100000 };
		if (freenect_process_events_timeout(ctx, &tv) < 0)
			break;
		if (depth_frames >= 60 && video_frames >= (hires ? 20u : 60u))
			break;
	}
	freenect_stop_depth(dev);
	freenect_stop_video(dev);
	printf("Frames: depth %u, colour %u\n", depth_frames, video_frames);
	if (fn_exit_requested || !depth_frames || !video_frames) {
		printf("No frames to map\n");
		goto out;
	}

	m = freenect_create_coordinate_mapper(dev);
	if (!m)
		goto out;

	// The calibration, and a mapper made from it that must agree
	{
		uint8_t cal[FREENECT_CALIBRATION_SIZE];
		FILE* f;
		freenect_mapper_get_calibration(m, cal, sizeof(cal));
		snprintf(path, sizeof(path), "%s_calibration.bin", prefix);
		f = fopen(path, "wb");
		if (f) {
			fwrite(cal, 1, sizeof(cal), f);
			fclose(f);
		}
		m2 = freenect_create_coordinate_mapper_from_calibration(cal, sizeof(cal));
	}

	// A few points: the centre and four others with depth
	{
		static const int px[5] = { 320, 160, 480, 160, 480 }, py[5] = { 240, 120, 120, 360, 360 };
		int same = 1;
		for (i = 0; i < 5; i++) {
			freenect_depth_point p, q;
			freenect_color_point c, c2;
			freenect_camera_point s;
			p.x = px[i]; p.y = py[i]; p.depth_mm = depth_copy[py[i] * DW + px[i]];
			freenect_map_depth_point_to_color(m, FREENECT_RESOLUTION_MEDIUM, cres, p, &c);
			freenect_map_depth_point_to_color(m2, FREENECT_RESOLUTION_MEDIUM, cres, p, &c2);
			freenect_map_depth_point_to_camera(m, FREENECT_RESOLUTION_MEDIUM, p, &s);
			freenect_map_camera_point_to_depth(m, FREENECT_RESOLUTION_MEDIUM, s, &q);
			same &= c.x == c2.x && c.y == c2.y;
			if (p.depth_mm == 0)
				printf("  depth (%3d,%3d): no depth\n", p.x, p.y);
			else
				printf("  depth (%3d,%3d) %4u mm -> colour (%4d,%4d); camera (%+.3f, %+.3f, %.3f) m -> depth (%3d,%3d) %4u mm\n",
				       p.x, p.y, p.depth_mm, c.x, c.y, s.x, s.y, s.z, q.x, q.y, q.depth_mm);
		}
		printf("Mapper from the saved calibration: %s\n", same ? "same results" : "DIFFERENT results");
	}

	// Depth onto the colour frame
	{
		size_t n = (size_t)cw * ch;
		int mh = hires ? 960 : 480; // the mapped grid; the 1414's 1280x1024 has 64 rows more
		freenect_depth_point* reg = (freenect_depth_point*)malloc(sizeof(*reg) * (size_t)cw * mh);
		uint8_t* over = (uint8_t*)malloc(n * 3);
		uint8_t* dimg = (uint8_t*)calloc(n, 3);
		unsigned valid = 0, edges = 0;
		int x, y;
		if (!reg || !over || !dimg) {
			free(reg); free(over); free(dimg);
			goto out;
		}
		freenect_map_color_frame_to_depth(m, FREENECT_RESOLUTION_MEDIUM, cres, depth_copy, reg);
		memcpy(over, rgb_copy, n * 3);
		for (y = 0; y < mh; y++)
			for (x = 0; x < cw; x++) {
				unsigned d = reg[(size_t)y * cw + x].depth_mm;
				uint8_t* o = over + ((size_t)y * cw + x) * 3;
				uint8_t* g = dimg + ((size_t)y * cw + x) * 3;
				if (d) {
					uint8_t v = (uint8_t)(d >= 4500 ? 30 : 255 - (d * 225) / 4500);
					valid++;
					g[0] = g[1] = g[2] = v;
				}
				// a depth step of more than 10 % to the right or below
				if (x + 1 < cw && y + 1 < mh) {
					unsigned r = reg[(size_t)y * cw + x + 1].depth_mm, b = reg[(size_t)(y + 1) * cw + x].depth_mm;
					unsigned lo = d, hi = d;
					if (r && r < lo) lo = r;
					if (b && b < lo) lo = b;
					if (r > hi) hi = r;
					if (b > hi) hi = b;
					if (d && lo && hi > lo + lo / 10) {
						o[0] = 255; o[1] = 0; o[2] = 0;
						edges++;
					}
				}
			}
		printf("Colour pixels with depth: %u of %d (%.0f %%); edge pixels drawn: %u\n",
		       valid, cw * mh, 100.0 * valid / (cw * mh), edges);
		snprintf(path, sizeof(path), "%s_color.bmp", prefix);
		write_bmp(path, rgb_copy, cw, ch);
		snprintf(path, sizeof(path), "%s_overlay.bmp", prefix);
		write_bmp(path, over, cw, ch);
		snprintf(path, sizeof(path), "%s_depth.bmp", prefix);
		write_bmp(path, dimg, cw, ch);
		printf("Wrote %s_color.bmp, %s_overlay.bmp, %s_depth.bmp and %s_calibration.bin\n", prefix, prefix, prefix, prefix);
		free(reg); free(over); free(dimg);
	}
	ret = 0;

out:
	freenect_destroy_coordinate_mapper(m);
	freenect_destroy_coordinate_mapper(m2);
	if (dev)
		freenect_close_device(dev);
	freenect_shutdown(ctx);
	free(depth_copy);
	free(rgb_copy);
	printf("Done!\n");
	return fn_exit_done(ret);
}
