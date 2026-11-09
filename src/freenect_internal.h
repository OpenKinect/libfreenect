/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010-2011 individual OpenKinect contributors. See the CONTRIB
 * file for details.
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
#pragma once

#include <stdint.h>

#include "libfreenect.h"
#include "libfreenect_registration.h"
#include "libfreenect_audio.h"
#include "stream_timing.h"

#ifdef __ELF__
  #define FN_INTERNAL	__attribute__ ((visibility ("hidden")))
#else
  #define FN_INTERNAL
#endif

// A function kept although nothing calls it yet.
#if defined(__GNUC__) || defined(__clang__)
  #define FN_UNUSED __attribute__ ((unused))
#else
  #define FN_UNUSED
#endif


typedef void (*fnusb_iso_cb)(freenect_device *dev, uint8_t *buf, int len);

#include "usb_libusb10.h"

// needed to set the led state for non 1414 devices
FN_INTERNAL int fnusb_set_led_alt(libusb_device_handle * dev, freenect_context * ctx, freenect_led_options state);

// Hotplug (hotplug.c): the Kinect cameras seen by the last scan. A camera is
// identified by its USB bus and address, which change whenever it
// re-enumerates, so a Kinect that is unplugged and plugged in again is a new
// arrival.
#define FN_HOTPLUG_MAX 16
#define FN_HOTPLUG_POLL_MS 1000
// A camera whose serial cannot be read yet (on Windows, for a moment after it
// appears, before its driver is ready) is reported only once it can, or after
// this long with an unknown serial.
#define FN_HOTPLUG_SERIAL_WAIT_MS 5000
typedef struct {
	uint8_t bus;
	uint8_t addr;
	char serial[64];
} fn_hotplug_entry;

typedef struct {
	freenect_hotplug_event event;
	fn_hotplug_entry dev;
} fn_hotplug_change;

typedef struct {
	freenect_hotplug_cb cb;
	void *user;
	fn_hotplug_entry known[FN_HOTPLUG_MAX];
	int n_known;
	int dirty;           // rescan at the next freenect_process_events*()
	int native;          // libusb hotplug is registered (no polling needed)
	int native_handle;   // libusb_hotplug_callback_handle
	uint64_t last_scan_ms;
	struct {
		uint8_t bus, addr;
		uint64_t since_ms;
	} pending[FN_HOTPLUG_MAX]; // new cameras whose serial could not be read yet
	int n_pending;
} fn_hotplug_state;

// The entry for bus/addr in the last scan, or NULL.
const fn_hotplug_entry *fn_hotplug_find(const fn_hotplug_state *st, uint8_t bus, uint8_t addr);
// Make `now` (n_now cameras) the known set. Writes the differences to
// `changes` (departures first, then arrivals, at most max_changes) and
// returns how many there are.
int fn_hotplug_update(fn_hotplug_state *st, const fn_hotplug_entry *now, int n_now,
                      fn_hotplug_change *changes, int max_changes);
// Hold back from `now` (n_now cameras, scanned at now_ms) the new cameras
// whose serial could not be read, for up to FN_HOTPLUG_SERIAL_WAIT_MS after
// they were first seen. Returns how many cameras remain in `now`.
int fn_hotplug_hold_unready(fn_hotplug_state *st, fn_hotplug_entry *now, int n_now, uint64_t now_ms);
// USB side (usb_libusb10.c)
int fnusb_scan_cameras(freenect_context *ctx, fn_hotplug_entry *out, int max);
void fnusb_hotplug_register(freenect_context *ctx);
void fnusb_hotplug_deregister(freenect_context *ctx);

struct _freenect_context {
	freenect_loglevel log_level;
	int log_from_env;      // LIBFREENECT_LOG set the level (and adds time stamps)
	uint64_t log_start_ms; // time base of those time stamps
	freenect_log_cb log_cb;
	fnusb_ctx usb;
	freenect_device_flags enabled_subdevices;
	freenect_device *first;
	int zero_plane_res;
    
    // if you want to load firmware from memory rather than disk
    unsigned char *     fn_fw_nui_ptr;
    unsigned int        fn_fw_nui_size;

    unsigned char *     fn_fw_k4w_ptr;
    unsigned int        fn_fw_k4w_size;

	fn_hotplug_state hotplug;
};

#define LL_FATAL FREENECT_LOG_FATAL
#define LL_ERROR FREENECT_LOG_ERROR
#define LL_WARNING FREENECT_LOG_WARNING
#define LL_NOTICE FREENECT_LOG_NOTICE
#define LL_INFO FREENECT_LOG_INFO
#define LL_DEBUG FREENECT_LOG_DEBUG
#define LL_SPEW FREENECT_LOG_SPEW
#define LL_FLOOD FREENECT_LOG_FLOOD


#ifdef _WIN32
#include <stdarg.h>
#include <stdio.h>
void fn_log(freenect_context *ctx, freenect_loglevel level, const char *fmt, ...);
#else
void fn_log(freenect_context *ctx, freenect_loglevel level, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
#endif

#define FN_LOG(level, ...) fn_log(ctx, level, __VA_ARGS__)

#define FN_FATAL(...) FN_LOG(LL_FATAL, __VA_ARGS__)
#define FN_ERROR(...) FN_LOG(LL_ERROR, __VA_ARGS__)
#define FN_WARNING(...) FN_LOG(LL_WARNING, __VA_ARGS__)
#define FN_NOTICE(...) FN_LOG(LL_NOTICE, __VA_ARGS__)
#define FN_INFO(...) FN_LOG(LL_INFO, __VA_ARGS__)
#define FN_DEBUG(...) FN_LOG(LL_DEBUG, __VA_ARGS__)
#define FN_SPEW(...) FN_LOG(LL_SPEW, __VA_ARGS__)
#define FN_FLOOD(...) FN_LOG(LL_FLOOD, __VA_ARGS__)

#ifdef FN_BIGENDIAN
static inline uint16_t fn_le16(uint16_t d)
{
	return (d<<8) | (d>>8);
}
static inline uint32_t fn_le32(uint32_t d)
{
	return (d<<24) | ((d<<8)&0xFF0000) | ((d>>8)&0xFF00) | (d>>24);
}
static inline int16_t fn_le16s(int16_t s)
{
	// reinterpret cast to unsigned, use the normal fn_le16, and then reinterpret cast back
	union {
		int16_t s;
		uint16_t u;
	} conversion_union;
	conversion_union.s = s;
	conversion_union.u = fn_le16(conversion_union.u);
	return conversion_union.s;
}
static inline int32_t fn_le32s(int32_t s)
{
	// reinterpret cast to unsigned, use the normal fn_le32, and then reinterpret cast back
	union {
		int32_t s;
		uint32_t u;
	} conversion_union;
	conversion_union.s = s;
	conversion_union.u = fn_le32(conversion_union.u);
	return conversion_union.s;
}
#else
#define fn_le16(x) (x)
#define fn_le32(x) (x)
#define fn_le16s(x) (x)
#define fn_le32s(x) (x)
#endif

#define DEPTH_PKTSIZE 1760
#define VIDEO_PKTSIZE 1920

#define DEPTH_PKTDSIZE (DEPTH_PKTSIZE-12)
#define VIDEO_PKTDSIZE (VIDEO_PKTSIZE-12)

#define VID_MICROSOFT 0x45e
#define PID_NUI_AUDIO 0x02ad
#define PID_NUI_CAMERA 0x02ae
#define PID_NUI_MOTOR 0x02b0
#define PID_K4W_CAMERA 0x02bf
#define PID_KV2_CAMERA 0x02d9 // This identifies the Kinect v2 (XBox One). Use https://github.com/OpenKinect/libfreenect2 instead.

// For K4W: first pid is what it starts out as,
// second is how it appears with lastest firmware from SDK,
// third is from beta SDK firmware ( which is what is unpacked by the fw script and doesn't support motor control )
#define PID_K4W_AUDIO 0x02be
#define PID_K4W_AUDIO_ALT_1 0x02c3
#define PID_K4W_AUDIO_ALT_2 0x02bb
#define PID_NUI_MOTOR_ALT 0x02af // listed next to 02B0 in Microsoft's INF; role unknown

// The Kinect's internal hub, which groups its USB functions into one sensor.
#define VID_NEC 0x0409
#define PID_NEC_HUB 0x005a       // Xbox 360 model 1414
#define PID_KINECT_HUB 0x02c2    // Xbox 360 model 1473 and Kinect for Windows

// Kinect v1 model, decided as Microsoft's runtime does: from the internal hub
// and the Kinect function on its lowest port.
typedef enum {
	FN_KINECT_UNKNOWN = 0,
	FN_KINECT_XBOX_1414 = 1, // NEC hub; motor/LED on 02B0
	FN_KINECT_XBOX_1473 = 2, // Microsoft hub; motor/LED through the audio controller
	FN_KINECT_WINDOWS = 3,   // Microsoft hub, 02BF camera; motor/LED through the audio controller, near mode
} fn_kinect_type;

fn_kinect_type fnusb_kinect_type(int have_hub, uint16_t hub_vid, uint16_t hub_pid, uint16_t first_child_pid,
                                 uint16_t camera_pid, uint16_t camera_bcd);
const char *fnusb_kinect_type_name(fn_kinect_type type);

// Conversion from shutter width to exposure in microseconds
// Measured using camtest.c with various very long exposure times
// TODO: calculate this from camera registers instead of magic numbers
#define SHUTTER_WIDTH_TO_EXP_RGB 54.21
#define SHUTTER_WIDTH_TO_EXP_YUV 63.25

typedef struct {
	int running;
	uint8_t flag;
	int synced;
	uint8_t seq;
	int got_pkts;
	int pkt_num;
	int pkts_per_frame;
	int pkt_size;
	int frame_size;
	int last_pkt_size;
	int valid_pkts;
	unsigned int lost_pkts;
	int valid_frames;
	int variable_length;
	uint32_t last_timestamp;
	uint32_t timestamp;
	int split_bufs;
	void *lib_buf;
	void *usr_buf;
	uint8_t *raw_buf;
	void *proc_buf;
	int frame_bad;               // a packet of the open frame was lost or short
	int keep_incomplete;         // LIBFREENECT_INCOMPLETE_FRAMES=1: deliver such frames anyway
	int skip_frames;             // complete frames still to skip after the stream start
	unsigned int dropped_frames; // incomplete frames dropped since the stream start
	unsigned int bad_magic;      // packets without the "RB" header
	uint64_t sof_us, eof_us;     // host time of the open frame's first and last packet
	uint64_t mid_us;             // host time midway through the last delivered frame
	unsigned int delivered;      // frames delivered since the stream start
	// Diagnostics since the stream start (logged when it stops, or when no
	// frame completes at all)
	unsigned int pkts_seen;      // non-empty packets
	unsigned int oversized, short_pkts;
	unsigned int resyncs_lost, resyncs_order;
	uint8_t last_order_flag;     // type of the last out-of-order packet
	int last_order_pos;          // and where in the frame it came
	int stall_reported;
} packet_stream;

// Stream start: stay quiet about lost packets for about a second (one
// packet per 125 µs microframe); report a stream with no complete frame
// after about three.
#define FN_STREAM_QUIET_PKTS 8000
#define FN_STREAM_STALL_PKTS 24000

// Frame reassembly from the camera's isochronous packets (stream.c).
// stream_process() returns the size of a completed frame, or 0.
FN_INTERNAL int stream_process(freenect_context *ctx, packet_stream *strm, uint8_t *pkt, int len, freenect_chunk_cb cb, void *user_data);
FN_INTERNAL void stream_init(freenect_context *ctx, packet_stream *strm, int rlen, int plen);
FN_INTERNAL void stream_freebufs(freenect_context *ctx, packet_stream *strm);
FN_INTERNAL int stream_setbuf(freenect_context *ctx, packet_stream *strm, void *pbuf);

// Kinect for Windows colour settings (color.c)
FN_INTERNAL void fn_color_init(freenect_device *dev);
FN_INTERNAL void fn_color_on_video_start(freenect_device *dev);
FN_INTERNAL void fn_color_apply_pending(freenect_device *dev);

// Depth/colour frame phase alignment (timing.c)
FN_INTERNAL int fn_fetch_timing_params(freenect_device *dev);
FN_INTERNAL void fn_timing_process(freenect_device *dev);

typedef struct {
	int running;

	freenect_sample_51* audio_out_ring; // TODO: implement sending user-provided data in callbacks
	int ring_reader_idx; // Index in audio_out_ring of the last sent sample
	int ring_writer_idx; // Index in audio_out_ring of the next sample we haven't received from the client yet

	uint16_t out_window;
	uint8_t out_seq;
	uint8_t out_counter_within_window;
	uint16_t out_weird_timestamp;
	uint8_t out_window_parity;

	uint16_t in_window;
	uint16_t last_seen_window[10];
	uint8_t in_counter;
	int32_t* mic_buffer[4];
	int16_t* cancelled_buffer;
	void* in_unknown;

	// TODO: timestamps
} audio_stream;

typedef struct {
	uint32_t magic;    // 0x80000080
	uint16_t channel;  // Values between 0x1 and 0xa indicate audio channel
	uint16_t len;      // packet length
	uint16_t window;   // timestamp
	uint16_t unknown;  // ???
	int32_t samples[]; // Size depends on len
} audio_in_block;

typedef struct {
	uint16_t window;       // Kinda like a timestamp.
	uint8_t seq;           // Values from 0x00 to 0x7f
	uint8_t weird;         // Has an odd cyclic behavior.
	freenect_sample_51 samples[6];  // Audio samples - 6 samples per transfer
} audio_out_block;

struct _freenect_device {
	freenect_context *parent;
	freenect_device *next;
	void *user_data;

	// Cameras
	fnusb_dev usb_cam;
	fnusb_isoc_stream depth_isoc;
	fnusb_isoc_stream video_isoc;

	freenect_depth_cb depth_cb;
	freenect_video_cb video_cb;
	freenect_chunk_cb depth_chunk_cb;
	freenect_chunk_cb video_chunk_cb;
	freenect_depth_range depth_range; // distances kept in millimetre depth (default: FREENECT_DEPTH_RANGE_DEFAULT, 800-4000 mm)
	double color_value[FREENECT_COLOR_SETTING_COUNT]; // Kinect for Windows colour settings (color.c)
	uint32_t color_set_mask; // bit per setting the application set
	uint32_t color_pending;  // bit per setting still to be sent to the camera
	uint16_t ir_brightness; // IR brightness the application set (register 0x15), 0 if none
	int near_mode;          // FREENECT_NEAR_MODE is on (it owns register 0x15 then)
	int32_t timing_k2, timing_k3; // stream timing parameters (cmd 0x16 param 6); k2 0 if unavailable
	int timing_auto;              // freenect_set_stream_timing_auto() is on
	fn_phase_state timing_phase;  // its phase estimate
	fn_timing_policy timing_policy; // and when it acts on it
	unsigned int timing_seen_depth, timing_seen_video; // frames it has seen
	uint16_t *depth_full; // 640x480 work buffer for the 320x240 and 80x60 depth modes
	int depth_mirrored;    // FREENECT_MIRROR_DEPTH is on (the camera mirrors depth)
	freenect_video_format video_format;
	freenect_depth_format depth_format;
	freenect_resolution video_resolution;
	freenect_resolution depth_resolution;

	int cam_inited;
	uint16_t cam_tag;

	packet_stream depth;
	packet_stream video;

	// Registration
	freenect_registration registration;

	// Audio
	fnusb_dev usb_audio;
	fnusb_isoc_stream audio_out_isoc;
	fnusb_isoc_stream audio_in_isoc;

	freenect_audio_in_cb audio_in_cb;
	freenect_audio_out_cb audio_out_cb;

	audio_stream audio;
	uint32_t audio_tag;

	// Motor
	fnusb_dev usb_motor;
	freenect_raw_tilt_state raw_state;
    
	int device_does_motor_control_with_audio;
	int motor_control_with_audio_enabled;

	fn_kinect_type device_type;

	// freenect_set_ir_emitter(dev, 0): keep the IR projector off while depth runs.
	int ir_emitter_forced_off;

	// Motor pacing (as in Microsoft's runtime): spacing of 1414 control
	// transfers, and the tilt rate limit (>= 1 s apart, <= 15 moves per 20 s).
	uint64_t motor_last_ctrl_ms;
	uint64_t tilt_last_move_ms;
	uint64_t tilt_window_start_ms;
	int tilt_window_count;

	// USB bus and address of this sensor's camera when it was opened (also when
	// the camera subdevice itself is not open), to match hotplug departures.
	uint8_t cam_bus;
	uint8_t cam_addr;
	int disconnect_handled; // streams already stopped after the device left

	// Serial that identifies this Kinect (see freenect_get_device_serial()).
	char serial[64];

	// Build of the UAC firmware running on the audio controller (cmd 0x09),
	// 0 if unknown; and whether the missing LED command was reported.
	int audio_fw_build;
	int led_unsupported_noted;
	// The LED was set; freenect_close_device() returns it to the idle state.
	int led_changed;
	// Times the audio controller was reopened after it rebooted.
	int audio_recover_attempts;
};

// The first UAC firmware build with the LED command 0x10 (v1.8 = 709; the
// SDK v1.0 beta 2 image, build 390, lacks it).
#define FN_UAC_LED_MIN_BUILD 709
// Register 0x15 (IR brightness) as Microsoft's driver sets it when near mode
// is switched off (§10C.3)
#define FN_IR_BRIGHTNESS_DEFAULT 30
#define FN_AUDIO_RECOVER_MAX 3

// Build number of the running UAC audio firmware (cmd 0x09), or < 0.
int fnusb_audio_fw_build(libusb_device_handle *audio, freenect_context *ctx);
// Reopen the audio controller after it rebooted, uploading its firmware again.
int fnusb_recover_audio(freenect_device *dev);

// LIBFREENECT_LED=off: the LED stays off (see freenect_set_led()).
int fn_led_forced_off(void);

// Switch the IR projector (laser) on or off: camera command 0x0086.
int fn_set_ir_emitter(freenect_device *dev, int on);

// Monotonic milliseconds and sleep, shared by the device code.
uint64_t fn_now_ms(void);
uint64_t fn_now_us(void);
void fn_sleep_ms(unsigned ms);
