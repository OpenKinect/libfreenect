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
#include "registration.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <ctype.h>

#define GRAVITY 9.80665

// The dev and ctx are just faked with these numbers

static freenect_device fake_dev_singleton;
static freenect_device *fake_dev = &fake_dev_singleton;
static freenect_context *fake_ctx = (freenect_context *)5678;
static freenect_depth_cb cur_depth_cb = NULL;
static freenect_video_cb cur_video_cb = NULL;
static char *input_path = NULL;
static FILE *index_fp = NULL;
static freenect_raw_tilt_state state = { 0 };
static uint16_t ir_brightness = 25;
static int already_warned = 0;
static double playback_prev_time = 0.;
static double record_prev_time = 0.;
static void *user_depth_buf = NULL;
static void *user_video_buf = NULL;
static int depth_running = 0;
static int rgb_running = 0;
static void *user_ptr = NULL;
static bool loop_playback = true;

#define MAKE_RESERVED(res, fmt) (uint32_t)(((res & 0xff) << 8) | (((fmt & 0xff))))
#define RESERVED_TO_RESOLUTION(reserved) (freenect_resolution)((reserved >> 8) & 0xff)
#define RESERVED_TO_FORMAT(reserved) ((reserved) & 0xff)

static freenect_frame_mode rgb_video_mode =
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB),
	    FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_RGB},
	    640*480*3, 640,  480, 24, 0, 30, 1 };
static freenect_frame_mode yuv_video_mode =
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_YUV_RAW),
	    FREENECT_RESOLUTION_MEDIUM, {FREENECT_VIDEO_YUV_RAW},
	    640*480*2, 640, 480, 16, 0, 15, 1 };

static freenect_frame_mode video_mode;

static freenect_frame_mode depth_11_mode =
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT),
	    FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_11BIT},
	    640*480*2, 640, 480, 11, 5, 30, 1};
static freenect_frame_mode depth_registered_mode =
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_REGISTERED),
	    FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_REGISTERED},
	    640*480*2, 640, 480, 16, 0, 30, 1};
static freenect_frame_mode depth_mm_mode =
	{MAKE_RESERVED(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM),
	    FREENECT_RESOLUTION_MEDIUM, {FREENECT_DEPTH_MM},
	    640*480*2, 640, 480, 16, 0, 30, 1};

static freenect_frame_mode depth_mode;

static void *default_video_back;
static void *default_depth_back;


static char *one_line(FILE *fp)
{
	int c;
	int pos = 0;
	char *out = NULL;
	for (c = fgetc(fp); !(c == '\n' || c == EOF); c = fgetc(fp))
	{
		out = realloc(out, pos + 1);
		out[pos++] = c;
	}
	if (out) {
		out = realloc(out, pos + 1);
		out[pos] = '\0';
	}
	return out;
}

static int get_data_size(FILE *fp)
{
	int orig = ftell(fp);
	fseek(fp, 0L, SEEK_END);
	int out = ftell(fp);
	fseek(fp, orig, SEEK_SET);
	return out;
}

static int parse_line(char *type, double *cur_time, unsigned int *timestamp, unsigned int *data_size, char **data)
{
	char *line = one_line(index_fp);
	if (!line) {
		printf("Warning: No more lines in [%s]\n", input_path);
		return -1;
	}
	int file_path_size = strlen(input_path) + strlen(line) + 50;
	char *file_path = malloc(file_path_size);
	snprintf(file_path, file_path_size, "%s/%s", input_path, line);
	// Open file
	FILE *cur_fp = fopen(file_path, "rb");
	if (!cur_fp) {
		printf("Error: Cannot open file [%s]\n", file_path);
		exit(1);
	}
	// Parse data from file name
	int ret = 0;
	*data_size = get_data_size(cur_fp);
	sscanf(line, "%c-%lf-%u-%*s", type, cur_time, timestamp);
	*data = malloc(*data_size);
	if (fread(*data, *data_size, 1, cur_fp) != 1) {
		printf("Error: Couldn't read entire file.\n");
		ret = -1;
	}
	fclose(cur_fp);
	free(line);
	free(file_path);
	return ret;
}

static void open_index()
{
	int index_path_size = strlen(input_path) + 50;
	char *index_path = malloc(index_path_size);
	snprintf(index_path, index_path_size, "%s/INDEX.txt", input_path);
	index_fp = fopen(index_path, "rb");
	if (!index_fp) {
		printf("Error: Cannot open file [%s]\n", index_path);
		exit(1);
	}
	free(index_path);
}

static void close_index()
{
	fclose(index_fp);
	index_fp = NULL;
	record_prev_time = 0;
	playback_prev_time = 0;
}

static char *skip_line(char *str)
{
	char *out = strchr(str, '\n');
	if (!out) {
		printf("Error: PGM/PPM has incorrect formatting, expected a header on one line followed by a newline\n");
		exit(1);
	}
	return out + 1;
}

static void convert_rgb_to_uyvy(uint8_t *rgb_buffer, uint8_t *yuv_buffer,
				freenect_frame_mode mode)
{
	int x,y;
	for (y = 0; y < mode.height; y++) {
		for (x = 0; x < mode.width; x+=2) {
			int pos = y * mode.width + x;
			uint8_t *rgb0 = rgb_buffer + pos * 3;
			uint8_t *rgb1 = rgb_buffer + (pos + 1) * 3;
			float y0 = (0.257f * rgb0[0]) + (0.504f * rgb0[1]) + (0.098f * rgb0[2]) + 16;
			float u0 = -(0.148f * rgb0[0]) - (0.291f * rgb0[1]) + (0.439f * rgb0[2]) + 128;
			float v0 = (0.439f * rgb0[0]) - (0.368f * rgb0[1]) - (0.071f * rgb0[2]) + 128;

			float y1 = (0.257f * rgb1[0]) + (0.504f * rgb1[1]) + (0.098f * rgb1[2]) + 16;
			float u1 = -(0.148f * rgb1[0]) - (0.291f * rgb1[1]) + (0.439f * rgb1[2]) + 128;
			float v1 = (0.439f * rgb1[0]) - (0.368f * rgb1[1]) - (0.071f * rgb1[2]) + 128;

			uint8_t *uyvy = yuv_buffer + pos * 2;

			uyvy[0] = (u0+u1)/2.f;
			uyvy[1] = y0;
			uyvy[2] = (v0+v1)/2.f;
			uyvy[3] = y1;
		}
	}
}

int freenect_process_events(freenect_context *ctx)
{
	/* This is where the magic happens. We read 1 update from the index
	   per call, so this needs to be called in a loop like usual.  If the
	   index line is a Depth/RGB image the provided callback is called.  If
	   the index line is accelerometer data, then it is used to update our
	   internal state.  If you query for the accelerometer data you get the
	   last sensor reading that we have.  The time delays are compensated as
	   best as we can to match those from the original data and current run
	   conditions (e.g., if it takes longer to run this code then we wait less).
	 */
	if (!index_fp)
		open_index();
	char type;
	double record_cur_time;
	unsigned int timestamp, data_size;
	char *data = NULL;
	if (parse_line(&type, &record_cur_time, &timestamp, &data_size, &data)) {
                if (loop_playback) {
			close_index();
			return 0;
                } else
		    return -1;
	}
	// Sleep an amount that compensates for the original and current delays
	// playback_ is w.r.t. the current time
	// record_ is w.r.t. the original time period during the recording
	if (record_prev_time != 0. && playback_prev_time != 0.)
		sleep_highres((record_cur_time - record_prev_time) - (get_time() - playback_prev_time));
	record_prev_time = record_cur_time;
	switch (type) {
		case 'd':
			if (cur_depth_cb && depth_running) {
				freenect_frame_mode mode = freenect_get_current_depth_mode(fake_dev);
				void *cur_depth = skip_line(data);
				void *depth_buffer = user_depth_buf ? user_depth_buf : default_depth_back;

				switch (mode.depth_format) {
				case FREENECT_DEPTH_11BIT:
				    memcpy(depth_buffer, cur_depth, mode.bytes);
				    break;
                                case FREENECT_DEPTH_REGISTERED:
                                    freenect_apply_registration(fake_dev, cur_depth, depth_buffer, true);
                                    break;
				case FREENECT_DEPTH_MM:
				    freenect_apply_depth_unpacked_to_mm(fake_dev, cur_depth, depth_buffer);
				    break;
				default:
				    assert(0);
				    break;
				}

				cur_depth_cb(fake_dev, depth_buffer, timestamp);
			}
			break;
		case 'r':
			if (cur_video_cb && rgb_running) {
				void *cur_video = skip_line(data);
				void *video_buffer = user_video_buf ? user_video_buf : default_video_back;

				freenect_frame_mode mode = freenect_get_current_video_mode(fake_dev);

				switch (mode.video_format) {
				case FREENECT_VIDEO_RGB:
					memcpy(video_buffer, cur_video, mode.bytes);
					break;
				case FREENECT_VIDEO_YUV_RAW:
					convert_rgb_to_uyvy(cur_video, video_buffer, mode);
					break;
				default:
					assert(0);
					break;
				}

				cur_video_cb(fake_dev, video_buffer, timestamp);
			}
			break;
		case 'a':
			if (data_size == sizeof(state)) {
				memcpy(&state, data, sizeof(state));
			} else if (!already_warned) {
				already_warned = 1;
				printf("\n\nWarning: Accelerometer data has an unexpected"
				       " size [%u] instead of [%u].  The acceleration "
				       "and tilt data will be substituted for dummy "
				       "values.  This data was probably made with an "
				       "older version of record (the upstream interface "
				       "changed).\n\n",
				       data_size, (unsigned int)sizeof state);
			}
			break;
	}
	free(data);
	playback_prev_time = get_time();
	return 0;
}

int freenect_process_events_timeout(freenect_context *ctx, struct timeval *timeout)
{
	return freenect_process_events(ctx);
}

double freenect_get_tilt_degs(freenect_raw_tilt_state *state)
{
	// NOTE: This is duped from tilt.c, this is the only function we need from there
	return ((double)state->tilt_angle) / 2.;
}

freenect_raw_tilt_state* freenect_get_tilt_state(freenect_device *dev)
{
	return &state;
}

freenect_tilt_status_code freenect_get_tilt_status(freenect_raw_tilt_state *state)
{
    return state->tilt_status;
}

void freenect_get_mks_accel(freenect_raw_tilt_state *state, double* x, double* y, double* z)
{
	//the documentation for the accelerometer (http://www.kionix.com/Product%20Sheets/KXSD9%20Product%20Brief.pdf)
	//states there are 819 counts/g
	*x = (double)state->accelerometer_x/FREENECT_COUNTS_PER_G*GRAVITY;
	*y = (double)state->accelerometer_y/FREENECT_COUNTS_PER_G*GRAVITY;
	*z = (double)state->accelerometer_z/FREENECT_COUNTS_PER_G*GRAVITY;
}

void freenect_set_depth_callback(freenect_device *dev, freenect_depth_cb cb)
{
	cur_depth_cb = cb;
}

void freenect_set_video_callback(freenect_device *dev, freenect_video_cb cb)
{
	cur_video_cb = cb;
}

int freenect_set_video_mode(freenect_device* dev, const freenect_frame_mode mode)
{
        // Always say it was successful but continue to pass through the
        // underlying data.  Would be better to check for conflict.
	video_mode = mode;
        return 0;
}

int freenect_set_depth_mode(freenect_device* dev, const freenect_frame_mode mode)
{
        // Always say it was successful but continue to pass through the
        // underlying data.  Would be better to check for conflict.
	depth_mode = mode;

	if ((mode.depth_format == FREENECT_DEPTH_MM ||
             mode.depth_format == FREENECT_DEPTH_REGISTERED) &&
	    dev->registration.zero_plane_info.reference_distance == 0) {
		printf("Warning: older fakenect recording doesn't contain "
		       "registration info for mapping depth to MM units\n");
	}

        return 0;
}

freenect_frame_mode freenect_find_video_mode(freenect_resolution res, freenect_video_format fmt) {
    assert(FREENECT_RESOLUTION_MEDIUM == res);

    switch(fmt) {
    case FREENECT_VIDEO_RGB:
	    return rgb_video_mode;
    case FREENECT_VIDEO_YUV_RAW:
	    return yuv_video_mode;
    default:
	    assert(0);
	    break;
    }

    freenect_frame_mode invalid = { 0 };
    return invalid;
}

int freenect_get_video_mode_count()
{
    return 1;
}

freenect_frame_mode freenect_get_video_mode(int mode_num)
{
    if (mode_num == 0)
	return rgb_video_mode;
    else if (mode_num == 1)
	return yuv_video_mode;
    else {
	freenect_frame_mode invalid = { 0 };
	return invalid;
    }
}

freenect_frame_mode freenect_get_current_video_mode(freenect_device *dev)
{
    return video_mode;
}

freenect_frame_mode freenect_find_depth_mode(freenect_resolution res, freenect_depth_format fmt) {
    assert(FREENECT_RESOLUTION_MEDIUM == res);

    switch (fmt) {
    case FREENECT_DEPTH_11BIT:
	    return depth_11_mode;
    case FREENECT_DEPTH_REGISTERED:
            return depth_registered_mode;
    case FREENECT_DEPTH_MM:
	    return depth_mm_mode;
    default:
	    assert(0);
	    break;
    }

    freenect_frame_mode invalid = { 0 };
    return invalid;
}

int freenect_get_depth_mode_count()
{
    return 3;
}

freenect_frame_mode freenect_get_depth_mode(int mode_num)
{
    if (mode_num == 0)
	return depth_11_mode;
    else if (mode_num == 1)
	return depth_mm_mode;
    else if (mode_num == 2)
        return depth_registered_mode;
    else {
	freenect_frame_mode invalid = { 0 };
	return invalid;
    }
}

freenect_frame_mode freenect_get_current_depth_mode(freenect_device *dev)
{
    return depth_mode;
}

int freenect_num_devices(freenect_context *ctx)
{
	// Always 1 device
	return 1;
}

int freenect_open_device(freenect_context *ctx, freenect_device **dev, int index)
{
	// Set it to some number to allow for NULL checks
	*dev = fake_dev;
	return 0;
}

int freenect_open_device_by_camera_serial(freenect_context *ctx, freenect_device **dev, const char* camera_serial)
{
    *dev = fake_dev;
    return 0;
}

static void read_device_info(freenect_device *dev)
{
	char fn[512];
	snprintf(fn, sizeof(fn), "%s/device.json", input_path);

	/* We silently return if this file is missing for compatibility with
	 * older recordings and applications that don't depend on registration
	 * info.
	 */
	JSON_Value *js = json_parse_file(fn);
        if (!js)
		return;

	JSON_Object *reg_info = json_object_get_object(json_object(js), "reg_info");
	JSON_Object *pad_info = json_object_get_object(reg_info, "pad_info");
	JSON_Object *zp_info = json_object_get_object(reg_info, "zero_plane_info");

	dev->registration.reg_info.ax = json_object_get_number(reg_info, "ax");
	dev->registration.reg_info.bx = json_object_get_number(reg_info, "bx");
	dev->registration.reg_info.cx = json_object_get_number(reg_info, "cx");
	dev->registration.reg_info.dx = json_object_get_number(reg_info, "dx");
	dev->registration.reg_info.ay = json_object_get_number(reg_info, "ay");
	dev->registration.reg_info.by = json_object_get_number(reg_info, "by");
	dev->registration.reg_info.cy = json_object_get_number(reg_info, "cy");
	dev->registration.reg_info.dy = json_object_get_number(reg_info, "dy");
	dev->registration.reg_info.dx_start = json_object_get_number(reg_info, "dx_start");
	dev->registration.reg_info.dy_start = json_object_get_number(reg_info, "dy_start");
	dev->registration.reg_info.dx_beta_start = json_object_get_number(reg_info, "dx_beta_start");
	dev->registration.reg_info.dy_beta_start = json_object_get_number(reg_info, "dy_beta_start");
	dev->registration.reg_info.dx_beta_inc = json_object_get_number(reg_info, "dx_beta_inc");
	dev->registration.reg_info.dy_beta_inc = json_object_get_number(reg_info, "dy_beta_inc");
	dev->registration.reg_info.dxdx_start = json_object_get_number(reg_info, "dxdx_start");
	dev->registration.reg_info.dxdy_start = json_object_get_number(reg_info, "dxdy_start");
	dev->registration.reg_info.dydx_start = json_object_get_number(reg_info, "dydx_start");
	dev->registration.reg_info.dydy_start = json_object_get_number(reg_info, "dydy_start");
	dev->registration.reg_info.dxdxdx_start = json_object_get_number(reg_info, "dxdxdx_start");
	dev->registration.reg_info.dydxdx_start = json_object_get_number(reg_info, "dydxdx_start");
	dev->registration.reg_info.dxdxdy_start = json_object_get_number(reg_info, "dxdxdy_start");
	dev->registration.reg_info.dydxdy_start = json_object_get_number(reg_info, "dydxdy_start");
	dev->registration.reg_info.dydydx_start = json_object_get_number(reg_info, "dydydx_start");
	dev->registration.reg_info.dydydy_start = json_object_get_number(reg_info, "dydydy_start");

	dev->registration.reg_pad_info.start_lines = json_object_get_number(pad_info, "start_lines");
	dev->registration.reg_pad_info.end_lines = json_object_get_number(pad_info, "end_lines");
	dev->registration.reg_pad_info.cropping_lines = json_object_get_number(pad_info, "cropping_lines");

	dev->registration.zero_plane_info.dcmos_emitter_dist = json_object_get_number(zp_info, "dcmos_emitter_distance");
	dev->registration.zero_plane_info.dcmos_rcmos_dist = json_object_get_number(zp_info, "dcmos_rcmos_distance");
	dev->registration.zero_plane_info.reference_distance = json_object_get_number(zp_info, "reference_distance");
	dev->registration.zero_plane_info.reference_pixel_size = json_object_get_number(zp_info, "reference_pixel_size");

	dev->registration.const_shift = json_object_get_number(json_object(js), "const_shift");

	json_value_free(js);

	freenect_init_registration(fake_dev);
}

int freenect_init(freenect_context **ctx, freenect_usb_context *usb_ctx)
{
	input_path = getenv("FAKENECT_PATH");
	if (!input_path) {
		printf("Error: Environmental variable FAKENECT_PATH is not set.  Set it to a path that was created using the 'record' utility.\n");
		exit(1);
	}

	char *var = getenv("FAKENECT_LOOP");
	if (var) {
		const int len = strlen(var);
		char* tmp = malloc((len + 1) * sizeof(*tmp));
		int i;
		for (i = 0; i < len; i++)
			tmp[i] = tolower(var[i]);
		tmp[len] = '\0';
		if (strcmp(tmp, "0") == 0 ||
		    strcmp(tmp, "false") == 0 ||
		    strcmp(tmp, "no") == 0 ||
		    strcmp(tmp, "off") == 0) {
			loop_playback = false;
		}
		free (tmp);
	}

	*ctx = fake_ctx;

	read_device_info(fake_dev);

	video_mode = rgb_video_mode;
	depth_mode = depth_11_mode;

	default_video_back = malloc(640*480*3);
	default_depth_back = malloc(640*480*2);

	return 0;
}

int freenect_supported_subdevices(void)
{
    return FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA;
}

void freenect_select_subdevices(freenect_context *ctx, freenect_device_flags subdevs) {
	// Ideally, we'd actually check for MOTOR and CAMERA and AUDIO, but for now
	// we just ignore them and provide all captured data.
}

int freenect_set_depth_buffer(freenect_device *dev, void *buf)
{
	user_depth_buf = buf;
	return 0;
}

int freenect_set_video_buffer(freenect_device *dev, void *buf)
{
	user_video_buf = buf;
	return 0;
}

void freenect_set_user(freenect_device *dev, void *user)
{
	user_ptr = user;
}

void *freenect_get_user(freenect_device *dev)
{
	return user_ptr;
}

int freenect_start_depth(freenect_device *dev)
{
	depth_running = 1;
	return 0;
}

int freenect_start_video(freenect_device *dev)
{
	rgb_running = 1;
	return 0;
}

int freenect_stop_depth(freenect_device *dev)
{
	depth_running = 0;
	return 0;
}

int freenect_stop_video(freenect_device *dev)
{
	rgb_running = 0;
	return 0;
}

int freenect_set_video_format(freenect_device *dev, freenect_video_format fmt)
{
	assert(fmt == FREENECT_VIDEO_RGB || fmt == FREENECT_VIDEO_YUV_RAW);
	return 0;
}
int freenect_set_depth_format(freenect_device *dev, freenect_depth_format fmt)
{
	assert(fmt == FREENECT_DEPTH_11BIT);
	return 0;
}

void freenect_set_log_callback(freenect_context *ctx, freenect_log_cb cb) {}
void freenect_set_log_level(freenect_context *ctx, freenect_loglevel level) {}
int freenect_shutdown(freenect_context *ctx)
{
	free(default_video_back);
	free(default_depth_back);
	return 0;
}
int freenect_close_device(freenect_device *dev)
{
	return 0;
}
int freenect_set_tilt_degs(freenect_device *dev, double angle)
{
	return 0;
}
int freenect_set_led(freenect_device *dev, freenect_led_options option)
{
	return 0;
}
int freenect_update_tilt_state(freenect_device *dev)
{
	return 0;
}
int freenect_get_ir_brightness(freenect_device *dev)
{
	return ir_brightness;
}
int freenect_set_ir_brightness(freenect_device *dev, uint16_t brightness)
{
	ir_brightness = (brightness % 50);
	return 0;
}

