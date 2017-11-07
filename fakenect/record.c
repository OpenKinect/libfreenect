/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 Brandyn White (bwhite@dappervision.com)
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

#include "libfreenect.h"
#include "freenect_internal.h"
#include "platform.h"
#include "parson.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

char *out_dir=0;
volatile sig_atomic_t running = 1;
uint32_t last_timestamp = 0;
FILE *index_fp = NULL;

#define FREENECT_FRAME_W 640
#define FREENECT_FRAME_H 480

int use_ffmpeg = 0;
char *ffmpeg_opts = 0;
char *depth_name = 0;
char *rgb_name = 0;

FILE *depth_stream=0;
FILE *rgb_stream=0;

void dump_depth(FILE *fp, void *data, int data_size)
{
	fprintf(fp, "P5 %d %d 65535\n", FREENECT_FRAME_W, FREENECT_FRAME_H);
	fwrite(data, data_size, 1, fp);
}

void dump_rgb(FILE *fp, void *data, int data_size)
{
	fprintf(fp, "P6 %d %d 255\n", FREENECT_FRAME_W, FREENECT_FRAME_H);
	fwrite(data, data_size, 1, fp);
}

FILE *open_dump(char type, double cur_time, uint32_t timestamp, int data_size, const char *extension)
{
	char *fn = malloc(strlen(out_dir) + 50);
	sprintf(fn, "%c-%f-%u.%s", type, cur_time, timestamp, extension);
	fprintf(index_fp, "%s\n", fn);
	sprintf(fn, "%s/%c-%f-%u.%s", out_dir, type, cur_time, timestamp, extension);
	FILE* fp = fopen(fn, "wb");
	if (!fp) {
		printf("Error: Cannot open file [%s]\n", fn);
		exit(1);
	}
	printf("%s\n", fn);
	free(fn);
	return fp;
}

FILE *open_ffmpeg(char *output_filename)
{
	char cmd[1024];

	if (ffmpeg_opts==0)
		ffmpeg_opts = "-aspect 4:3 -r 20 -vcodec msmpeg4 -b 30000k";

	snprintf(cmd, 1024, "ffmpeg -pix_fmt rgb24 -s %dx%d -f rawvideo "
			 "-i /dev/stdin %s %s",
             FREENECT_FRAME_W, FREENECT_FRAME_H,
             ffmpeg_opts, output_filename);

	fprintf(stderr, "%s\n", cmd);

	FILE* proc = popen(cmd, "w");
	if (!proc) {
		printf("Error: Cannot run ffmpeg\n");
		exit(1);
	}

	return proc;
}

void dump(char type, uint32_t timestamp, void *data, int data_size)
{
	// timestamp can be at most 10 characters, we have a few extra
	double cur_time = get_time();
	FILE *fp;
	last_timestamp = timestamp;
	switch (type) {
		case 'd':
			fp = open_dump(type, cur_time, timestamp, data_size, "pgm");
			dump_depth(fp, data, data_size);
			fclose(fp);
			break;
		case 'r':
			fp = open_dump(type, cur_time, timestamp, data_size, "ppm");
			dump_rgb(fp, data, data_size);
			fclose(fp);
			break;
		case 'a':
			fp = open_dump(type, cur_time, timestamp, data_size, "dump");
			fwrite(data, data_size, 1, fp);
			fclose(fp);
			break;
	}
}

void dump_ffmpeg_24(FILE *stream, uint32_t timestamp, void *data,
					int data_size)
{
	fwrite(data, data_size, 1, stream);
}

void dump_ffmpeg_pad16(FILE *stream, uint32_t timestamp, void *data,
					   int data_size)
{
	unsigned int z = 0;
	uint16_t* data_ptr = (uint16_t*)data;
	uint16_t* end = data_ptr + data_size;
	while (data_ptr < end) {
		z = *data_ptr;
		fwrite(((char*)(&z)), 3, 1, stream);
		data_ptr += 2;
	}
}

void snapshot_accel(freenect_device *dev)
{
	freenect_raw_tilt_state* state;
	if (!last_timestamp)
		return;
	freenect_update_tilt_state(dev);
	state = freenect_get_tilt_state(dev);
	dump('a', last_timestamp, state, sizeof *state);
}


void depth_cb(freenect_device *dev, void *depth, uint32_t timestamp)
{
	dump('d', timestamp, depth, freenect_get_current_depth_mode(dev).bytes);
}


void rgb_cb(freenect_device *dev, void *rgb, uint32_t timestamp)
{
	dump('r', timestamp, rgb, freenect_get_current_video_mode(dev).bytes);
}

void depth_cb_ffmpeg(freenect_device *dev, void *depth, uint32_t timestamp)
{
	double cur_time = get_time();
	fprintf(index_fp, "d-%f-%u\n", cur_time, timestamp);

	dump_ffmpeg_pad16(depth_stream, timestamp, depth,
                      freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM,
                                               FREENECT_DEPTH_11BIT).bytes);
}

void rgb_cb_ffmpeg(freenect_device *dev, void *rgb, uint32_t timestamp)
{
	double cur_time = get_time();
	fprintf(index_fp, "d-%f-%u\n", cur_time, timestamp);

	dump_ffmpeg_24(rgb_stream, timestamp, rgb,
                   freenect_get_current_video_mode(dev).bytes);
}

void init_ffmpeg_streams()
{
	depth_stream = open_ffmpeg(depth_name);
	rgb_stream = open_ffmpeg(rgb_name);
}

void print_mode(const char *name, freenect_frame_mode mode) {
    /* This is just a courtesy function to let the user know the mode
       if it becomes a bother for maintainability just comment out the
       code in its body.  It will only break if struct entries go missing.
     */
    printf("%s Mode: {%d, %d, {%d}, %d, %d, %d, %d, %d, %d, %d}\n", name,
	   mode.reserved, (int)mode.resolution, (int)mode.video_format, mode.bytes, mode.width,
	   mode.height, mode.data_bits_per_pixel, mode.padding_bits_per_pixel,
	   mode.framerate, mode.is_valid);
}

static void write_device_info(freenect_device *dev)
{
	JSON_Value *js = json_value_init_object();
	JSON_Object *dev_js = json_object(js);

	JSON_Value *reg_info_val = json_value_init_object();
	JSON_Object *reg_info = json_object(reg_info_val);

	json_object_set_value(dev_js, "reg_info", reg_info_val);

	json_object_set_number(reg_info, "ax", dev->registration.reg_info.ax);
	json_object_set_number(reg_info, "bx", dev->registration.reg_info.bx);
	json_object_set_number(reg_info, "cx", dev->registration.reg_info.cx);
	json_object_set_number(reg_info, "dx", dev->registration.reg_info.dx);
	json_object_set_number(reg_info, "ay", dev->registration.reg_info.ay);
	json_object_set_number(reg_info, "by", dev->registration.reg_info.by);
	json_object_set_number(reg_info, "cy", dev->registration.reg_info.cy);
	json_object_set_number(reg_info, "dy", dev->registration.reg_info.dy);
	json_object_set_number(reg_info, "dx_start", dev->registration.reg_info.dx_start);
	json_object_set_number(reg_info, "dy_start", dev->registration.reg_info.dy_start);
	json_object_set_number(reg_info, "dx_beta_start", dev->registration.reg_info.dx_beta_start);
	json_object_set_number(reg_info, "dy_beta_start", dev->registration.reg_info.dy_beta_start);
	json_object_set_number(reg_info, "dx_beta_inc", dev->registration.reg_info.dx_beta_inc);
	json_object_set_number(reg_info, "dy_beta_inc", dev->registration.reg_info.dy_beta_inc);
	json_object_set_number(reg_info, "dxdx_start", dev->registration.reg_info.dxdx_start);
	json_object_set_number(reg_info, "dxdy_start", dev->registration.reg_info.dxdy_start);
	json_object_set_number(reg_info, "dydx_start", dev->registration.reg_info.dydx_start);
	json_object_set_number(reg_info, "dydy_start", dev->registration.reg_info.dydy_start);
	json_object_set_number(reg_info, "dxdxdx_start", dev->registration.reg_info.dxdxdx_start);
	json_object_set_number(reg_info, "dydxdx_start", dev->registration.reg_info.dydxdx_start);
	json_object_set_number(reg_info, "dxdxdy_start", dev->registration.reg_info.dxdxdy_start);
	json_object_set_number(reg_info, "dydxdy_start", dev->registration.reg_info.dydxdy_start);
	json_object_set_number(reg_info, "dydydx_start", dev->registration.reg_info.dydydx_start);
	json_object_set_number(reg_info, "dydydy_start", dev->registration.reg_info.dydydy_start);

	JSON_Value *pad_info_val = json_value_init_object();
	JSON_Object *pad_info = json_object(pad_info_val);

	json_object_set_value(reg_info, "pad_info", pad_info_val);

	json_object_set_number(pad_info, "start_lines", dev->registration.reg_pad_info.start_lines);
	json_object_set_number(pad_info, "end_lines", dev->registration.reg_pad_info.end_lines);
	json_object_set_number(pad_info, "cropping_lines", dev->registration.reg_pad_info.cropping_lines);

	JSON_Value *zp_info_val = json_value_init_object();
	JSON_Object *zp_info = json_object(zp_info_val);

	json_object_set_value(reg_info, "zero_plane_info", zp_info_val);

	json_object_set_number(zp_info, "dcmos_emitter_distance", dev->registration.zero_plane_info.dcmos_emitter_dist);
	json_object_set_number(zp_info, "dcmos_rcmos_distance", dev->registration.zero_plane_info.dcmos_rcmos_dist);
	json_object_set_number(zp_info, "reference_distance", dev->registration.zero_plane_info.reference_distance);
	json_object_set_number(zp_info, "reference_pixel_size", dev->registration.zero_plane_info.reference_pixel_size);

	json_object_set_number(dev_js, "const_shift", dev->registration.const_shift);

	char fn[512];
	snprintf(fn, sizeof(fn), "%s/device.json", out_dir);

	json_serialize_to_file_pretty(js, fn);

	json_value_free(js);
}

void init()
{
	freenect_context *ctx;
	freenect_device *dev;
	if (freenect_init(&ctx, 0)) {
		printf("Error: Cannot get context\n");
		return;
	}

	// fakenect doesn't support audio yet, so don't bother claiming the device
	freenect_select_subdevices(ctx, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));

	if (freenect_open_device(ctx, &dev, 0)) {
		printf("Error: Cannot get device\n");
		return;
	}
	print_mode("Depth", freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));
	print_mode("Video", freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
	freenect_set_depth_mode(dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));
	freenect_start_depth(dev);
	freenect_set_video_mode(dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
	freenect_start_video(dev);

	write_device_info(dev);

	if (use_ffmpeg) {
		init_ffmpeg_streams();
		freenect_set_depth_callback(dev, depth_cb_ffmpeg);
		freenect_set_video_callback(dev, rgb_cb_ffmpeg);
	} else {
		freenect_set_depth_callback(dev, depth_cb);
		freenect_set_video_callback(dev, rgb_cb);
	}
	while (running && freenect_process_events(ctx) >= 0)
		snapshot_accel(dev);
	freenect_stop_depth(dev);
	freenect_stop_video(dev);
	freenect_close_device(dev);
	freenect_shutdown(ctx);
}

FILE *open_index(const char *fn)
{
    FILE *fp = fopen(fn, "r");
    if (fp) {
        fclose(fp);
        printf("Error: Index already exists, to avoid overwriting "
               "use a different directory.\n");
        return 0;
    }
    fp = fopen(fn, "wb");
    if (!fp) {
        printf("Error: Cannot open file [%s]\n", fn);
        return 0;
    }
    return fp;
}

void signal_cleanup(int num)
{
	running = 0;
	printf("Caught signal, cleaning up\n");
	signal(SIGINT, signal_cleanup);
}

void usage()
{
	printf("Records the Kinect sensor data to a directory\nResult can be used as input to Fakenect\nUsage:\n");
	printf("  record [-h] [-ffmpeg] [-ffmpeg-opts <options>] "
		   "<target basename>\n");
	exit(0);
}

int main(int argc, char **argv)
{
	int c=1;
	while (c < argc) {
		if (strcmp(argv[c],"-ffmpeg")==0)
			use_ffmpeg = 1;
		else if (strcmp(argv[c],"-ffmpeg-opts")==0) {
			if (++c < argc)
				ffmpeg_opts = argv[c];
		} else if (strcmp(argv[c],"-h")==0)
			usage();
		else
			out_dir = argv[c];
		c++;
	}

	if (!out_dir)
		usage();

	signal(SIGINT, signal_cleanup);

	if (use_ffmpeg) {
		FILE *f;

		char *index_fn = malloc(strlen(out_dir) + 50);
		sprintf(index_fn, "%s-index.txt", out_dir);
		index_fp = open_index(index_fn);
		free(index_fn);
		if (!index_fp)
			return 1;

		depth_name = malloc(strlen(out_dir) + 50);
		rgb_name = malloc(strlen(out_dir) + 50);
		sprintf(depth_name, "%s-depth.avi", out_dir);
		sprintf(rgb_name, "%s-rgb.avi", out_dir);

		f = fopen(depth_name, "r");
		if (f) {
			printf("Error: %s already exists, to avoid overwriting "
				   "use a different name.\n", depth_name);
			fclose(f);
			exit(1);
		}
		f = fopen(rgb_name, "r");
		if (f) {
			printf("Error: %s already exists, to avoid overwriting "
				   "use a different name.\n", depth_name);
			fclose(f);
			exit(1);
		}
		init();
		free(depth_name);
		free(rgb_name);
		if (depth_stream) fclose(depth_stream);
		if (rgb_stream) fclose(rgb_stream);
		fclose(index_fp);
	} else {
#ifdef _WIN32
		_mkdir(out_dir);
#else
		mkdir(out_dir, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
		char *fn = malloc(strlen(out_dir) + 50);
		sprintf(fn, "%s/INDEX.txt", out_dir);
		index_fp = open_index(fn);
		free(fn);
		if (!index_fp) {
			fclose(index_fp);
			return 1;
		}

		init();
		fclose(index_fp);
	}
	return 0;
}
