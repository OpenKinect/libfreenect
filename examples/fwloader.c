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
 * freenect-fwloader: upload the audio firmware to every connected Kinect
 * whose audio controller is still in its bootloader, then exit.
 *
 * Opening a device with only FREENECT_DEVICE_AUDIO selected makes libfreenect
 * upload the firmware and wait for the audio controller to re-enumerate, so
 * this tool only drives that path for each sensor. Run it at login or from a
 * device-arrival trigger, so that the audio device (and, on Kinect for
 * Windows and model 1473, the motor and LED) is ready before an application
 * starts.
 *
 * Usage: freenect-fwloader [--firmware FILE] [--verbose]
 *   Without --firmware, audios.bin is searched for in the usual places (see
 *   upload_firmware() in src/loader.c), or in $LIBFREENECT_FIRMWARE_PATH.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libfreenect.h"

static unsigned char *read_file(const char *path, unsigned int *size)
{
	FILE *f = fopen(path, "rb");
	unsigned char *data = NULL;
	long n;
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0) {
		data = (unsigned char *)malloc((size_t)n);
		if (data && fread(data, 1, (size_t)n, f) == (size_t)n) {
			*size = (unsigned int)n;
		} else {
			free(data);
			data = NULL;
		}
	}
	fclose(f);
	return data;
}

int main(int argc, char **argv)
{
	freenect_context *ctx;
	unsigned char *fw = NULL;
	unsigned int fw_size = 0;
	const char *fw_path = NULL;
	int verbose = 0, i, n, failed = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--firmware") == 0 && i + 1 < argc) {
			fw_path = argv[++i];
		} else if (strcmp(argv[i], "--verbose") == 0) {
			verbose = 1;
		} else {
			printf("Usage: %s [--firmware FILE] [--verbose]\n"
			       "Uploads the audio firmware to every connected Kinect that needs it.\n", argv[0]);
			return strcmp(argv[i], "--help") == 0 ? 0 : 1;
		}
	}

	if (freenect_init(&ctx, NULL) < 0) {
		fprintf(stderr, "freenect_init() failed\n");
		return 1;
	}
	freenect_set_log_level(ctx, verbose ? FREENECT_LOG_INFO : FREENECT_LOG_NOTICE);
	freenect_select_subdevices(ctx, FREENECT_DEVICE_AUDIO);

	if (fw_path) {
		fw = read_file(fw_path, &fw_size);
		if (!fw) {
			fprintf(stderr, "Cannot read firmware file %s\n", fw_path);
			freenect_shutdown(ctx);
			return 1;
		}
		/* The same image is offered for both Xbox and Kinect for Windows sensors. */
		freenect_set_fw_address_nui(ctx, fw, fw_size);
		freenect_set_fw_address_k4w(ctx, fw, fw_size);
	}

	n = freenect_num_devices(ctx);
	if (n <= 0) {
		printf("No Kinect found.\n");
		freenect_shutdown(ctx);
		free(fw);
		return 2;
	}

	for (i = 0; i < n; i++) {
		freenect_device *dev;
		if (freenect_open_device(ctx, &dev, i) < 0) {
			fprintf(stderr, "Kinect %d: could not open the audio controller or upload its firmware.\n", i);
			failed++;
			continue;
		}
		printf("Kinect %d: audio controller is running its firmware.\n", i);
		freenect_close_device(dev);
	}

	freenect_shutdown(ctx);
	free(fw);
	return failed ? 1 : 0;
}
