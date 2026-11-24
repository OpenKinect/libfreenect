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
 * freenect-pipewire-mic: the Kinect's microphone array as one beamformed
 * microphone for every PipeWire (and PulseAudio, JACK) application.
 *
 * With Microsoft's UAC firmware running, snd-usb-audio offers the array as a
 * 4-channel ALSA device, which PipeWire turns into a source node. This
 * program captures that node, runs libfreenect's beamformer
 * (libfreenect_audio_dsp.h) on it and publishes the result as a mono source
 * node, "Kinect beamformed microphone", which applications pick like any
 * other microphone. The source stays while the Kinect is unplugged (it gives
 * silence) and picks the Kinect up again when it comes back.
 *
 *   freenect-pipewire-mic [--beam auto|adaptive|ANGLE] [--sdk-weights]
 *                         [--noise-suppression] [--agc] [--target NODE]
 *                         [--name DESCRIPTION] [--verbose]
 *
 * --target names the source node to capture (node.name or object.serial);
 * by default the first source whose name or ALSA card names the Kinect
 * ("Xbox NUI Sensor", "Kinect").
 *
 * Run as the user, next to PipeWire: the systemd user unit
 * freenect-pipewire-mic.service starts it with the session.
 */

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/result.h>
#include <spa/utils/ringbuffer.h>

#include "libfreenect_audio_dsp.h"

#define RATE FREENECT_DSP_RATE
#define IN_CHANNELS 4
#define RING_SAMPLES 16384 // power of two
#define RING_MASK (RING_SAMPLES - 1)
#define RING_HIGH 4096     // more than this buffered (256 ms): drop back to RING_LOW
#define RING_LOW 1024

struct mic {
	struct pw_main_loop* loop;
	struct pw_context* context;
	struct pw_core* core;
	struct pw_registry* registry;
	struct spa_hook core_listener, registry_listener, capture_listener, source_listener;
	struct pw_stream* capture; // from the Kinect, NULL while it is away
	struct pw_stream* source;  // the beamformed microphone, always there
	uint32_t kinect_id;        // the captured node's global id, SPA_ID_INVALID if none
	char kinect_target[256];
	freenect_dsp* dsp;
	int16_t* outbuf;
	uint32_t outbuf_len;
	struct spa_ringbuffer ring;
	int16_t ring_data[RING_SAMPLES];
	const char* want_target; // --target
	const char* description; // --name
	int verbose;
	unsigned long frames_in, frames_out, underruns, drops;
};

static int contains_nocase(const char* s, const char* what)
{
	size_t n = strlen(what), i;
	if (!s)
		return 0;
	for (; *s; s++) {
		for (i = 0; i < n && s[i] && tolower((unsigned char)s[i]) == tolower((unsigned char)what[i]); i++)
			;
		if (i == n)
			return 1;
	}
	return 0;
}

// Whether a source node is the Kinect's microphone array.
static int is_kinect(const struct spa_dict* props)
{
	static const char* keys[] = { PW_KEY_NODE_NAME, PW_KEY_NODE_DESCRIPTION, PW_KEY_NODE_NICK, "alsa.card_name",
		                          "alsa.long_card_name", "api.alsa.card.name", "api.alsa.card.longname", "device.product.name" };
	unsigned i;
	for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
		const char* v = spa_dict_lookup(props, keys[i]);
		if (contains_nocase(v, "Xbox NUI Sensor") || contains_nocase(v, "Xbox_NUI_Sensor") || contains_nocase(v, "Kinect"))
			return 1;
	}
	return 0;
}

/* ------------------------------------------------------------ capture: the Kinect */

static void on_capture_process(void* data)
{
	struct mic* m = data;
	struct pw_buffer* b = pw_stream_dequeue_buffer(m->capture);
	struct spa_data* d;
	uint32_t frames, idx, done = 0;
	int32_t filled;
	if (!b)
		return;
	d = &b->buffer->datas[0];
	if (d->data && d->chunk) {
		const float* in = SPA_PTROFF(d->data, d->chunk->offset, const float);
		frames = d->chunk->size / (IN_CHANNELS * sizeof(float));
		while (done < frames) {
			uint32_t n = SPA_MIN(frames - done, m->outbuf_len);
			freenect_dsp_process_float(m->dsp, in + done * IN_CHANNELS, (int)n, m->outbuf);
			filled = spa_ringbuffer_get_write_index(&m->ring, &idx);
			if (filled + (int32_t)n > RING_SAMPLES)
				break; // nobody listens; the reader drops the backlog when one comes
			spa_ringbuffer_write_data(&m->ring, m->ring_data, sizeof(m->ring_data), (idx & RING_MASK) * sizeof(int16_t), m->outbuf,
			                          n * sizeof(int16_t));
			spa_ringbuffer_write_update(&m->ring, idx + n);
			done += n;
		}
		m->frames_in += frames;
	}
	pw_stream_queue_buffer(m->capture, b);
}

static void destroy_capture(struct mic* m);

static void on_capture_state(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error)
{
	struct mic* m = data;
	(void)old;
	if (m->verbose || state == PW_STREAM_STATE_ERROR)
		fprintf(stderr, "Kinect capture: %s%s%s\n", pw_stream_state_as_string(state), error ? ": " : "", error ? error : "");
	if (state == PW_STREAM_STATE_STREAMING)
		fprintf(stderr, "Capturing the Kinect's microphones (%s)\n", m->kinect_target);
}

static const struct pw_stream_events capture_events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = on_capture_state,
	.process = on_capture_process,
};

static void create_capture(struct mic* m, uint32_t id, const char* target)
{
	uint8_t buffer[1024];
	struct spa_pod_builder pb = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	const struct spa_pod* params[1];
	struct spa_audio_info_raw info;
	struct pw_properties* props;
	char latency[32];
	int i;

	snprintf(latency, sizeof(latency), "%u/%u", FREENECT_DSP_FRAME, RATE);
	props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "DSP",
	                          PW_KEY_NODE_NAME, "freenect_beam_mic_capture", PW_KEY_NODE_DESCRIPTION,
	                          "Kinect beamformer input", PW_KEY_TARGET_OBJECT, target, PW_KEY_NODE_DONT_RECONNECT, "true",
	                          PW_KEY_STREAM_DONT_REMIX, "true", PW_KEY_NODE_LATENCY, latency, NULL);
	m->capture = pw_stream_new(m->core, "Kinect beamformer input", props);
	if (!m->capture) {
		fprintf(stderr, "Could not create the capture stream\n");
		return;
	}
	pw_stream_add_listener(m->capture, &m->capture_listener, &capture_events, m);

	spa_zero(info);
	info.format = SPA_AUDIO_FORMAT_F32;
	info.channels = IN_CHANNELS;
	info.rate = RATE;
	for (i = 0; i < IN_CHANNELS; i++)
		info.position[i] = SPA_AUDIO_CHANNEL_AUX0 + i; // microphones 1..4, left to right
	params[0] = spa_format_audio_raw_build(&pb, SPA_PARAM_EnumFormat, &info);
	if (pw_stream_connect(m->capture, PW_DIRECTION_INPUT, PW_ID_ANY,
	                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS, params, 1) < 0) {
		fprintf(stderr, "Could not connect to %s\n", target);
		destroy_capture(m);
		return;
	}
	m->kinect_id = id;
	snprintf(m->kinect_target, sizeof(m->kinect_target), "%s", target);
}

static void destroy_capture(struct mic* m)
{
	if (m->capture) {
		spa_hook_remove(&m->capture_listener);
		pw_stream_destroy(m->capture);
		m->capture = NULL;
	}
	m->kinect_id = SPA_ID_INVALID;
}

/* ------------------------------------------------------------ the beamformed source */

static void on_source_process(void* data)
{
	struct mic* m = data;
	struct pw_buffer* b = pw_stream_dequeue_buffer(m->source);
	struct spa_data* d;
	uint32_t want, idx, have;
	int32_t avail;
	int16_t* dst;
	if (!b)
		return;
	d = &b->buffer->datas[0];
	if (!d->data) {
		pw_stream_queue_buffer(m->source, b);
		return;
	}
	dst = d->data;
	want = d->maxsize / sizeof(int16_t);
	if (b->requested && b->requested < want)
		want = (uint32_t)b->requested;

	avail = spa_ringbuffer_get_read_index(&m->ring, &idx);
	if (avail > RING_HIGH) {
		// the Kinect runs ahead of the reader's clock (or the reader paused):
		// keep the latency down
		idx += (uint32_t)(avail - RING_LOW);
		avail = RING_LOW;
		if (m->frames_out) // not the backlog from before anyone listened
			m->drops++;
	}
	have = avail > 0 ? SPA_MIN((uint32_t)avail, want) : 0;
	if (have)
		spa_ringbuffer_read_data(&m->ring, m->ring_data, sizeof(m->ring_data), (idx & RING_MASK) * sizeof(int16_t), dst,
		                         have * sizeof(int16_t));
	spa_ringbuffer_read_update(&m->ring, idx + have);
	if (have < want) {
		memset(dst + have, 0, (want - have) * sizeof(int16_t)); // the Kinect is away, or late
		if (m->capture)
			m->underruns++;
	}
	d->chunk->offset = 0;
	d->chunk->stride = sizeof(int16_t);
	d->chunk->size = want * sizeof(int16_t);
	m->frames_out += want;
	pw_stream_queue_buffer(m->source, b);
}

static void on_source_state(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error)
{
	struct mic* m = data;
	(void)old;
	if (m->verbose || state == PW_STREAM_STATE_ERROR)
		fprintf(stderr, "Beamformed source: %s%s%s\n", pw_stream_state_as_string(state), error ? ": " : "", error ? error : "");
}

static const struct pw_stream_events source_events = {
	PW_VERSION_STREAM_EVENTS,
	.state_changed = on_source_state,
	.process = on_source_process,
};

static int create_source(struct mic* m)
{
	uint8_t buffer[1024];
	struct spa_pod_builder pb = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
	const struct spa_pod* params[1];
	struct spa_audio_info_raw info;
	struct pw_properties* props;
	char latency[32];

	snprintf(latency, sizeof(latency), "%u/%u", FREENECT_DSP_FRAME, RATE);
	props = pw_properties_new(PW_KEY_MEDIA_CLASS, "Audio/Source", PW_KEY_NODE_NAME, "freenect_beam_mic", PW_KEY_NODE_DESCRIPTION,
	                          m->description, PW_KEY_NODE_NICK, "Kinect beam", PW_KEY_NODE_LATENCY, latency, PW_KEY_MEDIA_ROLE,
	                          "Communication", "device.icon-name", "audio-input-microphone", NULL);
	m->source = pw_stream_new(m->core, m->description, props);
	if (!m->source)
		return -1;
	pw_stream_add_listener(m->source, &m->source_listener, &source_events, m);
	spa_zero(info);
	info.format = SPA_AUDIO_FORMAT_S16;
	info.channels = 1;
	info.rate = RATE;
	info.position[0] = SPA_AUDIO_CHANNEL_MONO;
	params[0] = spa_format_audio_raw_build(&pb, SPA_PARAM_EnumFormat, &info);
	return pw_stream_connect(m->source, PW_DIRECTION_OUTPUT, PW_ID_ANY, PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
	                         params, 1);
}

/* ------------------------------------------------------------ finding the Kinect */

static void on_global(void* data, uint32_t id, uint32_t permissions, const char* type, uint32_t version, const struct spa_dict* props)
{
	struct mic* m = data;
	const char *klass, *name, *serial;
	(void)permissions;
	(void)version;
	if (m->capture || !props || strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
		return;
	klass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
	name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
	serial = spa_dict_lookup(props, PW_KEY_OBJECT_SERIAL);
	if (!klass || strcmp(klass, "Audio/Source") != 0 || (name && strcmp(name, "freenect_beam_mic") == 0))
		return;
	if (m->want_target ? !((name && strcmp(name, m->want_target) == 0) || (serial && strcmp(serial, m->want_target) == 0))
	                   : !is_kinect(props))
		return;
	if (m->verbose)
		fprintf(stderr, "Found the Kinect's microphones: node %u (%s)\n", id, name ? name : "?");
	create_capture(m, id, name ? name : (serial ? serial : ""));
}

static void on_global_remove(void* data, uint32_t id)
{
	struct mic* m = data;
	if (m->capture && id == m->kinect_id) {
		fprintf(stderr, "The Kinect's microphones went away; waiting for them\n");
		destroy_capture(m);
	}
}

static const struct pw_registry_events registry_events = {
	PW_VERSION_REGISTRY_EVENTS,
	.global = on_global,
	.global_remove = on_global_remove,
};

static void on_core_error(void* data, uint32_t id, int seq, int res, const char* message)
{
	struct mic* m = data;
	(void)seq;
	fprintf(stderr, "PipeWire error on object %u: %s (%s)\n", id, message, spa_strerror(res));
	if (id == PW_ID_CORE && res == -EPIPE)
		pw_main_loop_quit(m->loop); // PipeWire went away; systemd restarts us with it
}

static const struct pw_core_events core_events = {
	PW_VERSION_CORE_EVENTS,
	.error = on_core_error,
};

static void on_signal(void* data, int sig)
{
	struct mic* m = data;
	(void)sig;
	pw_main_loop_quit(m->loop);
}

static void on_report(void* data, uint64_t expirations)
{
	struct mic* m = data;
	double angle, conf;
	(void)expirations;
	if (!m->capture)
		return;
	if (freenect_dsp_get_source_angle(m->dsp, &angle, &conf) == 0)
		fprintf(stderr, "beam %+.0f deg, source %+.0f deg (confidence %.2f); %lu frames in, %lu underruns, %lu drops\n",
		        freenect_dsp_get_beam_angle(m->dsp) * 180.0 / M_PI, angle * 180.0 / M_PI, conf, m->frames_in, m->underruns, m->drops);
	else
		fprintf(stderr, "beam %+.0f deg, no source yet; %lu frames in\n", freenect_dsp_get_beam_angle(m->dsp) * 180.0 / M_PI, m->frames_in);
}

static void usage(const char* argv0)
{
	printf("Usage: %s [--beam auto|adaptive|ANGLE] [--sdk-weights] [--noise-suppression] [--agc]\n"
	       "          [--target NODE] [--name DESCRIPTION] [--verbose]\n"
	       "Publishes the Kinect's microphone array, beamformed, as one PipeWire microphone.\n"
	       "  --beam              auto (default): follow the talker among the 11 fixed beams;\n"
	       "                      adaptive: steer continuously; ANGLE: a fixed beam, -50..50 degrees\n"
	       "  --sdk-weights       use Microsoft's beam weights (fetched with fwfetcher.py --audio-weights)\n"
	       "  --noise-suppression, --agc   (need libfreenect built with SpeexDSP)\n"
	       "  --target NODE       the source to capture (node.name or object.serial); default: the Kinect\n"
	       "  --name DESCRIPTION  the microphone's name (default \"Kinect beamformed microphone\")\n"
	       "  --verbose           report the beam and the talker's direction every 5 s\n",
	       argv0);
}

int main(int argc, char** argv)
{
	struct mic m;
	const char* beam = "auto";
	int sdk = 0, ns = 0, agc = 0, i, ret = 1;
	struct spa_source* timer = NULL;

	memset(&m, 0, sizeof(m));
	m.kinect_id = SPA_ID_INVALID;
	m.description = "Kinect beamformed microphone";
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--beam") == 0 && i + 1 < argc) beam = argv[++i];
		else if (strcmp(argv[i], "--sdk-weights") == 0) sdk = 1;
		else if (strcmp(argv[i], "--noise-suppression") == 0) ns = 1;
		else if (strcmp(argv[i], "--agc") == 0) agc = 1;
		else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) m.want_target = argv[++i];
		else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) m.description = argv[++i];
		else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) m.verbose = 1;
		else {
			usage(argv[0]);
			return strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ? 0 : 2;
		}
	}

	m.dsp = freenect_dsp_create(NULL);
	m.outbuf_len = 1024;
	m.outbuf = calloc(m.outbuf_len, sizeof(int16_t));
	if (!m.dsp || !m.outbuf) {
		fprintf(stderr, "Out of memory\n");
		return 1;
	}
	if (strcmp(beam, "auto") == 0) {
		freenect_dsp_set_beam_mode(m.dsp, FREENECT_DSP_BEAM_AUTOMATIC);
	} else if (strcmp(beam, "adaptive") == 0) {
		freenect_dsp_set_beam_mode(m.dsp, FREENECT_DSP_BEAM_ADAPTIVE);
	} else {
		char* end;
		double a = strtod(beam, &end);
		if (*end || a < -50.0 || a > 50.0) {
			fprintf(stderr, "--beam: auto, adaptive or an angle from -50 to 50 degrees\n");
			return 2;
		}
		freenect_dsp_set_beam_mode(m.dsp, FREENECT_DSP_BEAM_MANUAL);
		freenect_dsp_set_beam(m.dsp, (int)lround((a + 50.0) / 10.0));
	}
	if (sdk && freenect_dsp_load_sdk_weights(m.dsp, NULL) < 0)
		fprintf(stderr, "Microsoft's beam weights were not found; using libfreenect's own\n");
	if (ns && freenect_dsp_set_noise_suppression(m.dsp, 1) < 0)
		fprintf(stderr, "Noise suppression needs libfreenect built with SpeexDSP\n");
	if (agc && freenect_dsp_set_agc(m.dsp, 1) < 0)
		fprintf(stderr, "Automatic gain control needs libfreenect built with SpeexDSP\n");
	spa_ringbuffer_init(&m.ring);

	pw_init(&argc, &argv);
	m.loop = pw_main_loop_new(NULL);
	if (!m.loop)
		goto out;
	pw_loop_add_signal(pw_main_loop_get_loop(m.loop), SIGINT, on_signal, &m);
	pw_loop_add_signal(pw_main_loop_get_loop(m.loop), SIGTERM, on_signal, &m);
	m.context = pw_context_new(pw_main_loop_get_loop(m.loop), NULL, 0);
	if (!m.context)
		goto out;
	m.core = pw_context_connect(m.context, NULL, 0);
	if (!m.core) {
		fprintf(stderr, "Could not connect to PipeWire: %s\n", strerror(errno));
		goto out;
	}
	pw_core_add_listener(m.core, &m.core_listener, &core_events, &m);
	if (create_source(&m) < 0) {
		fprintf(stderr, "Could not create the beamformed source\n");
		goto out;
	}
	m.registry = pw_core_get_registry(m.core, PW_VERSION_REGISTRY, 0);
	pw_registry_add_listener(m.registry, &m.registry_listener, &registry_events, &m);
	fprintf(stderr, "\"%s\" is available; waiting for the Kinect's microphones\n", m.description);
	if (m.verbose) {
		struct timespec value = { 5, 0 }, interval = { 5, 0 };
		timer = pw_loop_add_timer(pw_main_loop_get_loop(m.loop), on_report, &m);
		pw_loop_update_timer(pw_main_loop_get_loop(m.loop), timer, &value, &interval, false);
	}
	pw_main_loop_run(m.loop);
	ret = 0;

out:
	destroy_capture(&m);
	if (timer)
		pw_loop_destroy_source(pw_main_loop_get_loop(m.loop), timer);
	if (m.source)
		pw_stream_destroy(m.source);
	if (m.registry)
		pw_proxy_destroy((struct pw_proxy*)m.registry);
	if (m.core)
		pw_core_disconnect(m.core);
	if (m.context)
		pw_context_destroy(m.context);
	if (m.loop)
		pw_main_loop_destroy(m.loop);
	pw_deinit();
	freenect_dsp_destroy(m.dsp);
	free(m.outbuf);
	return ret;
}
