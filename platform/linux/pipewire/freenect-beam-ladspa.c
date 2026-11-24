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
 * freenect_beam: libfreenect's Kinect beamformer (libfreenect_audio_dsp.h)
 * as a LADSPA plugin, for PipeWire's filter graphs (kinect-mic.conf, loaded
 * by WirePlumber when the Kinect's microphones appear) and any other LADSPA
 * host.
 *
 * Ports: In 1..In 4 (the microphones, left to right as the Kinect sees
 * them), Out (mono). Controls: Mode (0 follow the talker among the fixed
 * beams, 1 steer continuously, 2 a fixed beam at Angle), Angle (-50..50
 * degrees, positive towards microphone 4), and, read when the filter starts: SDK weights (1: Microsoft's
 * beam weights, if fwfetcher.py --audio-weights installed them), Noise
 * suppression and AGC (1: on; need libfreenect built with SpeexDSP).
 * Read-only: Beam angle, Source angle, Source confidence.
 *
 * The beamformer works at 16 kHz. The plugin runs at 16 kHz or any whole
 * multiple of it (32, 48, 96 kHz): it then filters and decimates the input
 * and interpolates the output itself. The beam output is delayed by
 * FREENECT_DSP_LATENCY (32 ms), plus the resampling filters' delay.
 *
 * The LADSPA interface is declared here from its published specification
 * (the ABI of ladspa.h, version 1.1), so that no header is needed to build.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libfreenect_audio_dsp.h"

#if defined(__GNUC__)
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif

/* ------------------------------------------------------------ LADSPA 1.1 ABI */

typedef float LADSPA_Data;
typedef void* LADSPA_Handle;
typedef int LADSPA_Properties;
typedef int LADSPA_PortDescriptor;
typedef int LADSPA_PortRangeHintDescriptor;

#define LADSPA_PROPERTY_HARD_RT_CAPABLE 0x4
#define LADSPA_PORT_INPUT 0x1
#define LADSPA_PORT_OUTPUT 0x2
#define LADSPA_PORT_CONTROL 0x4
#define LADSPA_PORT_AUDIO 0x8
#define LADSPA_HINT_BOUNDED_BELOW 0x1
#define LADSPA_HINT_BOUNDED_ABOVE 0x2
#define LADSPA_HINT_TOGGLED 0x4
#define LADSPA_HINT_INTEGER 0x20
#define LADSPA_HINT_DEFAULT_MINIMUM 0x40
#define LADSPA_HINT_DEFAULT_0 0x200

typedef struct {
	LADSPA_PortRangeHintDescriptor HintDescriptor;
	LADSPA_Data LowerBound;
	LADSPA_Data UpperBound;
} LADSPA_PortRangeHint;

typedef struct LADSPA_Descriptor_s {
	unsigned long UniqueID;
	const char* Label;
	LADSPA_Properties Properties;
	const char* Name;
	const char* Maker;
	const char* Copyright;
	unsigned long PortCount;
	const LADSPA_PortDescriptor* PortDescriptors;
	const char* const* PortNames;
	const LADSPA_PortRangeHint* PortRangeHints;
	void* ImplementationData;
	LADSPA_Handle (*instantiate)(const struct LADSPA_Descriptor_s* Descriptor, unsigned long SampleRate);
	void (*connect_port)(LADSPA_Handle Instance, unsigned long Port, LADSPA_Data* DataLocation);
	void (*activate)(LADSPA_Handle Instance);
	void (*run)(LADSPA_Handle Instance, unsigned long SampleCount);
	void (*run_adding)(LADSPA_Handle Instance, unsigned long SampleCount);
	void (*set_run_adding_gain)(LADSPA_Handle Instance, LADSPA_Data Gain);
	void (*deactivate)(LADSPA_Handle Instance);
	void (*cleanup)(LADSPA_Handle Instance);
} LADSPA_Descriptor;

/* ------------------------------------------------------------ the plugin */

enum {
	P_IN1, P_IN2, P_IN3, P_IN4, P_OUT,
	P_MODE, P_ANGLE, P_SDK, P_NS, P_AGC,
	P_BEAM_ANGLE, P_SOURCE_ANGLE, P_SOURCE_CONF,
	P_COUNT
};

#define MAX_FACTOR 6       // up to 96 kHz
#define TAPS_PER_FACTOR 24 // resampling filter length: 24 taps per 16 kHz sample
#define MAX_TAPS (TAPS_PER_FACTOR * MAX_FACTOR + 1)
#define CHUNK 256          // 16 kHz samples per DSP call
#define OUT_FIFO 8192      // host-rate output samples (power of two)

typedef struct {
	LADSPA_Data* port[P_COUNT];
	freenect_dsp* dsp;
	int factor;              // host rate / 16000
	int taps;
	float h[MAX_TAPS];       // anti-alias / interpolation low-pass at the host rate
	// decimation: the last `taps` input samples of each microphone (doubled ring)
	float hist[4][2 * MAX_TAPS];
	int hpos, phase;
	// 16 kHz in and out
	float in16[CHUNK * 4];
	int n16;
	int16_t out16[CHUNK];
	// interpolation: the last 16 kHz outputs
	float ohist[2 * MAX_TAPS];
	int opos;
	// host-rate output FIFO
	float fifo[OUT_FIFO];
	unsigned fr, fw;
	int last_mode;
	float last_angle;
} beam;

static void design(beam* b)
{
	// windowed sinc (Blackman), cut-off 7.2 kHz at the host rate, unity DC gain
	int n = b->taps, i, c = n / 2;
	double fc = 7200.0 / (16000.0 * b->factor), sum = 0.0;
	for (i = 0; i < n; i++) {
		double x = i - c, w = 0.42 - 0.5 * cos(2 * M_PI * i / (n - 1)) + 0.08 * cos(4 * M_PI * i / (n - 1));
		double s = x == 0 ? 2 * fc : sin(2 * M_PI * fc * x) / (M_PI * x);
		b->h[i] = (float)(s * w);
		sum += s * w;
	}
	for (i = 0; i < n; i++)
		b->h[i] = (float)(b->h[i] / sum);
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor* d, unsigned long rate)
{
	beam* b;
	(void)d;
	if (rate < 16000 || rate % 16000 || rate / 16000 > MAX_FACTOR) {
		fprintf(stderr, "freenect_beam: needs 16 kHz or a whole multiple of it up to 96 kHz, not %lu Hz\n", rate);
		return NULL;
	}
	b = calloc(1, sizeof(*b));
	if (!b)
		return NULL;
	b->factor = (int)(rate / 16000);
	b->taps = b->factor > 1 ? TAPS_PER_FACTOR * b->factor + 1 : 1;
	if (b->factor > 1)
		design(b);
	b->dsp = freenect_dsp_create(NULL);
	if (!b->dsp) {
		free(b);
		return NULL;
	}
	if (getenv("FREENECT_BEAM_DEBUG"))
		fprintf(stderr, "freenect_beam: %lu Hz, factor %d, %d taps\n", rate, b->factor, b->taps);
	b->last_mode = -1;
	return b;
}

static void connect_port(LADSPA_Handle h, unsigned long port, LADSPA_Data* data)
{
	beam* b = h;
	if (port < P_COUNT)
		b->port[port] = data;
}

static void activate(LADSPA_Handle h)
{
	beam* b = h;
	memset(b->hist, 0, sizeof(b->hist));
	memset(b->ohist, 0, sizeof(b->ohist));
	b->hpos = b->phase = b->n16 = b->opos = 0;
	// prime the output FIFO with one 16 kHz sample's worth, so that every run
	// call has enough however the host's block falls on the 16 kHz grid
	memset(b->fifo, 0, sizeof(b->fifo));
	b->fr = 0;
	b->fw = (unsigned)b->factor;
	b->last_mode = -1;
	// the options that need memory or files: only here, never in run()
	if (b->port[P_SDK] && *b->port[P_SDK] > 0.5f && !freenect_dsp_using_sdk_weights(b->dsp) &&
	    freenect_dsp_load_sdk_weights(b->dsp, NULL) < 0)
		fprintf(stderr, "freenect_beam: Microsoft's beam weights are not installed; using libfreenect's\n");
	if (b->port[P_NS])
		freenect_dsp_set_noise_suppression(b->dsp, *b->port[P_NS] > 0.5f);
	if (b->port[P_AGC])
		freenect_dsp_set_agc(b->dsp, *b->port[P_AGC] > 0.5f);
}

static void controls(beam* b)
{
	int mode = b->port[P_MODE] ? (int)lrintf(*b->port[P_MODE]) : 0;
	float angle = b->port[P_ANGLE] ? *b->port[P_ANGLE] : 0.0f;
	if (mode < 0 || mode > 2)
		mode = 0;
	if (mode != b->last_mode || (mode == 2 && angle != b->last_angle)) {
		freenect_dsp_set_beam_mode(b->dsp, mode == 1 ? FREENECT_DSP_BEAM_ADAPTIVE
		                                   : mode == 2 ? FREENECT_DSP_BEAM_MANUAL
		                                               : FREENECT_DSP_BEAM_AUTOMATIC);
		if (mode == 2) {
			int beam_i = (int)lrintf((angle + 50.0f) / 10.0f);
			freenect_dsp_set_beam(b->dsp, beam_i < 0 ? 0 : beam_i > 10 ? 10 : beam_i);
		}
		b->last_mode = mode;
		b->last_angle = angle;
	}
}

// the 16 kHz chunk is full: beamform it and interpolate it into the FIFO
static void flush16(beam* b)
{
	int i, k, j;
	freenect_dsp_process_float(b->dsp, b->in16, b->n16, b->out16);
	for (i = 0; i < b->n16; i++) {
		float y = b->out16[i] * (1.0f / 32768.0f);
		if (b->factor == 1) {
			b->fifo[b->fw++ & (OUT_FIFO - 1)] = y;
			continue;
		}
		// zero-stuffed history at the host rate: y then factor - 1 zeros;
		// only every factor-th tap meets a sample (polyphase)
		b->opos = (b->opos + 1) % b->taps;
		b->ohist[b->opos] = b->ohist[b->opos + b->taps] = y;
		for (k = 0; k < b->factor; k++) {
			float acc = 0.0f;
			int newest = b->opos + b->taps; // index of y in the doubled ring
			for (j = k; j < b->taps; j += b->factor)
				acc += b->h[j] * b->ohist[newest - (j - k) / b->factor];
			b->fifo[b->fw++ & (OUT_FIFO - 1)] = acc * b->factor;
		}
	}
	b->n16 = 0;
}

static void run(LADSPA_Handle h, unsigned long n)
{
	beam* b = h;
	unsigned long i;
	int c, j;
	const LADSPA_Data* in[4];
	LADSPA_Data* out = b->port[P_OUT];
	for (c = 0; c < 4; c++)
		in[c] = b->port[P_IN1 + c];
	controls(b);
	for (i = 0; i < n; i++) {
		if (b->factor == 1) {
			for (c = 0; c < 4; c++)
				b->in16[b->n16 * 4 + c] = in[c] ? in[c][i] : 0.0f;
			if (++b->n16 == CHUNK)
				flush16(b);
			continue;
		}
		// decimate: keep the history, filter once every factor samples
		b->hpos = (b->hpos + 1) % b->taps;
		for (c = 0; c < 4; c++) {
			float x = in[c] ? in[c][i] : 0.0f;
			b->hist[c][b->hpos] = b->hist[c][b->hpos + b->taps] = x;
		}
		if (++b->phase == b->factor) {
			b->phase = 0;
			for (c = 0; c < 4; c++) {
				const float* x = &b->hist[c][b->hpos + b->taps];
				float acc = 0.0f;
				for (j = 0; j < b->taps; j++)
					acc += b->h[j] * x[-j];
				b->in16[b->n16 * 4 + c] = acc;
			}
			if (++b->n16 == CHUNK)
				flush16(b);
		}
	}
	// what is buffered at 16 kHz goes through now, so that the output keeps up
	if (b->n16)
		flush16(b);
	for (i = 0; i < n; i++)
		out[i] = (b->fw - b->fr) > 0 ? b->fifo[b->fr++ & (OUT_FIFO - 1)] : 0.0f;
	if (b->fw - b->fr > OUT_FIFO / 2) // never happens with a steady host; keep the delay bounded
		b->fr = b->fw - (unsigned)b->factor;

	if (b->port[P_BEAM_ANGLE])
		*b->port[P_BEAM_ANGLE] = (LADSPA_Data)(freenect_dsp_get_beam_angle(b->dsp) * 180.0 / M_PI);
	{
		double a, conf;
		if (freenect_dsp_get_source_angle(b->dsp, &a, &conf) == 0) {
			if (b->port[P_SOURCE_ANGLE])
				*b->port[P_SOURCE_ANGLE] = (LADSPA_Data)(a * 180.0 / M_PI);
			if (b->port[P_SOURCE_CONF])
				*b->port[P_SOURCE_CONF] = (LADSPA_Data)conf;
		}
	}
}

static void cleanup(LADSPA_Handle h)
{
	beam* b = h;
	freenect_dsp_destroy(b->dsp);
	free(b);
}

static const LADSPA_PortDescriptor port_desc[P_COUNT] = {
	LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO,    LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO,   LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO,
	LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO,    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,  LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,
	LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,  LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,   LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,
	LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,  LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL, LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL,
	LADSPA_PORT_OUTPUT | LADSPA_PORT_CONTROL,
};
static const char* const port_names[P_COUNT] = {
	"In 1", "In 2", "In 3", "In 4", "Out", "Mode", "Angle", "SDK weights", "Noise suppression", "AGC", "Beam angle", "Source angle", "Source confidence",
};
static const LADSPA_PortRangeHint port_hints[P_COUNT] = {
	{ 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
	{ LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_INTEGER | LADSPA_HINT_DEFAULT_MINIMUM, 0, 2 },
	{ LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0, -50, 50 },
	{ LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0, 0, 1 },
	{ LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0, 0, 1 },
	{ LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0, 0, 1 },
	{ LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE, -90, 90 },
	{ LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE, -90, 90 },
	{ LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE, 0, 1 },
};

static const LADSPA_Descriptor descriptor = {
	0x4b4e31, // "KN1"
	"freenect_beam",
	LADSPA_PROPERTY_HARD_RT_CAPABLE,
	"Kinect microphone array beamformer (libfreenect)",
	"OpenKinect",
	"Apache-2.0 OR GPL-2.0",
	P_COUNT,
	port_desc,
	port_names,
	port_hints,
	NULL,
	instantiate,
	connect_port,
	activate,
	run,
	NULL,
	NULL,
	NULL,
	cleanup,
};

EXPORT const LADSPA_Descriptor* ladspa_descriptor(unsigned long index)
{
	return index == 0 ? &descriptor : NULL;
}
