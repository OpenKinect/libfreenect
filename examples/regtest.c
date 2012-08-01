/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2011 individual OpenKinect contributors. See the CONTRIB file
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

#include "libfreenect.h"
#include "libfreenect_sync.h"

FILE *open_dump(const char *filename)
{
	FILE* fp = fopen(filename, "w");
	if (fp == NULL) {
		fprintf(stderr, "Error: Cannot open file [%s]\n", filename);
		exit(1);
	}
	printf("%s\n", filename);
	return fp;
}

void dump_depth(FILE *fp, void *data, unsigned int width, unsigned int height)
{
	fprintf(fp, "P5 %d %d 65535\n", width, height);
	fwrite(data, width * height * 2, 1, fp);
}

void dump_rgb(FILE *fp, void *data, unsigned int width, unsigned int height)
{
	fprintf(fp, "P6 %d %d 255\n", width, height);
	fwrite(data, width * height * 3, 1, fp);
}

void no_kinect_quit(void)
{
	fprintf(stderr, "Error: Kinect not connected?\n");
	exit(1);
}

int main(void)
{
	short *depth = 0;
	char *rgb = 0;
	uint32_t ts;
	FILE *fp;
	int ret;

	ret = freenect_sync_get_video((void**)&rgb, &ts, 0, FREENECT_VIDEO_RGB);
	if (ret < 0)
		no_kinect_quit();

	fp = open_dump("registration_test_rgb.ppm");
	dump_rgb(fp, rgb, 640, 480);
	fclose(fp);

	ret = freenect_sync_get_depth((void**)&depth, &ts, 0, FREENECT_DEPTH_11BIT);
	if (ret < 0)
		no_kinect_quit();

	fp = open_dump("registration_test_depth_raw.pgm");
	dump_depth(fp, depth, 640, 480);
	fclose(fp);

	ret = freenect_sync_get_depth((void**)&depth, &ts, 0, FREENECT_DEPTH_REGISTERED);
	if (ret < 0)
		no_kinect_quit();

	fp = open_dump("registration_test_depth_registered.pgm");
	dump_depth(fp, depth, 640, 480);
	fclose(fp);

	ret = freenect_sync_get_depth((void**)&depth, &ts, 0, FREENECT_DEPTH_MM);
	if (ret < 0)
		no_kinect_quit();

	fp = open_dump("registration_test_depth_mm.pgm");
	dump_depth(fp, depth, 640, 480);
	fclose(fp);

	return 0;
}
