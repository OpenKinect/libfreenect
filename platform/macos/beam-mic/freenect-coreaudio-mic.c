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
 * freenect-coreaudio-mic: the helper of KinectBeamMic.driver. It captures the
 * Kinect's 4 microphones (a Core Audio input once the UAC firmware runs),
 * beamforms them with libfreenect (libfreenect_audio_dsp.h) and plays the
 * result into the plug-in's hidden sink, so that "Kinect Beamformed
 * Microphone" carries it to every application. It follows the Kinect being
 * unplugged and plugged in again.
 *
 *   freenect-coreaudio-mic [--beam auto|adaptive|ANGLE] [--sdk-weights]
 *                          [--noise-suppression] [--agc] [--verbose]
 *   freenect-coreaudio-mic --list        the audio devices, as it sees them
 *   freenect-coreaudio-mic --self-test   no Kinect needed: plays a tone into
 *                                        the sink, records the microphone
 *
 * It runs from "Kinect Beam Mic.app" (a background app, so that macOS asks
 * for microphone access under that name), started at login by the launchd
 * agent org.openkinect.beam-mic.plist.
 */

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <math.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libfreenect_audio_dsp.h"

#define SINK_UID "org.openkinect.beam-sink"
// kAudioObjectPropertyElementMain (macOS 12; kAudioObjectPropertyElementMaster before), both 0
#define ELEMENT_MAIN 0
#define MIC_UID "org.openkinect.beam-mic"
#define RING 16384u // power of two
#define RING_HIGH 4096
#define RING_LOW 1024

static struct {
	freenect_dsp* dsp;
	AudioObjectID kinect, sink;
	AudioDeviceIOProcID in_proc, out_proc;
	int verbose, running;
	// DSP output -> sink: one producer (the Kinect's IO thread), one consumer
	float ring[RING];
	_Atomic unsigned wpos, rpos;
	float* inter;            // interleaving scratch (non-interleaved input)
	unsigned inter_len;
	int16_t* out16;
	unsigned long frames_in, underruns, drops;
} g;

/* ------------------------------------------------------------ devices */

static CFStringRef copy_string(AudioObjectID id, AudioObjectPropertySelector sel)
{
	AudioObjectPropertyAddress a = { sel, kAudioObjectPropertyScopeGlobal, ELEMENT_MAIN };
	CFStringRef s = NULL;
	UInt32 size = sizeof(s);
	if (AudioObjectGetPropertyData(id, &a, 0, NULL, &size, &s) != noErr)
		return NULL;
	return s;
}

static void cstring(CFStringRef s, char* out, size_t n)
{
	out[0] = 0;
	if (s)
		CFStringGetCString(s, out, (CFIndex)n, kCFStringEncodingUTF8);
}

static UInt32 channels(AudioObjectID id, AudioObjectPropertyScope scope)
{
	AudioObjectPropertyAddress a = { kAudioDevicePropertyStreamConfiguration, scope, ELEMENT_MAIN };
	UInt32 size = 0, n = 0, i;
	AudioBufferList* l;
	if (AudioObjectGetPropertyDataSize(id, &a, 0, NULL, &size) != noErr || size == 0)
		return 0;
	l = malloc(size);
	if (!l)
		return 0;
	if (AudioObjectGetPropertyData(id, &a, 0, NULL, &size, l) == noErr)
		for (i = 0; i < l->mNumberBuffers; i++)
			n += l->mBuffers[i].mNumberChannels;
	free(l);
	return n;
}

static AudioObjectID device_by_uid(const char* uid)
{
	AudioObjectPropertyAddress a = { kAudioHardwarePropertyTranslateUIDToDevice, kAudioObjectPropertyScopeGlobal,
		                             ELEMENT_MAIN };
	CFStringRef s = CFStringCreateWithCString(NULL, uid, kCFStringEncodingUTF8);
	AudioObjectID id = kAudioObjectUnknown;
	UInt32 size = sizeof(id);
	if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, sizeof(s), &s, &size, &id) != noErr)
		id = kAudioObjectUnknown;
	CFRelease(s);
	return id;
}

static int contains_nocase(const char* s, const char* what)
{
	size_t n = strlen(what), i;
	for (; *s; s++) {
		for (i = 0; i < n && s[i] && (s[i] | 32) == (what[i] | 32); i++)
			;
		if (i == n)
			return 1;
	}
	return 0;
}

// The Kinect's microphone array: an input with 4 channels whose name says so.
static AudioObjectID find_kinect(int list)
{
	AudioObjectPropertyAddress a = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, ELEMENT_MAIN };
	UInt32 size = 0, n, i;
	AudioObjectID *ids, found = kAudioObjectUnknown;
	if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, NULL, &size) != noErr)
		return kAudioObjectUnknown;
	ids = malloc(size);
	if (!ids)
		return kAudioObjectUnknown;
	if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, ids) != noErr)
		size = 0;
	n = size / sizeof(AudioObjectID);
	for (i = 0; i < n; i++) {
		char name[256], uid[256];
		CFStringRef s = copy_string(ids[i], kAudioObjectPropertyName), u = copy_string(ids[i], kAudioDevicePropertyDeviceUID);
		UInt32 in = channels(ids[i], kAudioObjectPropertyScopeInput), out = channels(ids[i], kAudioObjectPropertyScopeOutput);
		int kinect;
		cstring(s, name, sizeof(name));
		cstring(u, uid, sizeof(uid));
		if (s) CFRelease(s);
		if (u) CFRelease(u);
		kinect = in >= 4 && (contains_nocase(name, "Xbox NUI Sensor") || contains_nocase(name, "Kinect")) &&
		         strcmp(uid, MIC_UID) != 0 && strcmp(uid, SINK_UID) != 0;
		if (list)
			printf("  %3u: %-40s %2u in, %2u out  %s%s\n", (unsigned)ids[i], name, (unsigned)in, (unsigned)out, uid,
			       kinect ? "  (Kinect)" : "");
		if (kinect && found == kAudioObjectUnknown)
			found = ids[i];
	}
	free(ids);
	return found;
}

static void set_rate(AudioObjectID id, Float64 rate)
{
	AudioObjectPropertyAddress a = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
		                             ELEMENT_MAIN };
	Float64 now = 0;
	UInt32 size = sizeof(now);
	if (AudioObjectGetPropertyData(id, &a, 0, NULL, &size, &now) == noErr && now != rate)
		AudioObjectSetPropertyData(id, &a, 0, NULL, sizeof(rate), &rate);
}

/* ------------------------------------------------------------ IO */

static void ring_push(const int16_t* s, unsigned n)
{
	unsigned w = atomic_load_explicit(&g.wpos, memory_order_relaxed), r = atomic_load_explicit(&g.rpos, memory_order_acquire), i;
	if (w - r + n > RING)
		return; // nobody plays the sink; drop
	for (i = 0; i < n; i++)
		g.ring[(w + i) & (RING - 1)] = s[i] * (1.0f / 32768.0f);
	atomic_store_explicit(&g.wpos, w + n, memory_order_release);
}

// The Kinect's 4 channels in (interleaved, or one buffer per channel).
static OSStatus kinect_proc(AudioObjectID dev, const AudioTimeStamp* now, const AudioBufferList* in, const AudioTimeStamp* in_time,
                            AudioBufferList* out, const AudioTimeStamp* out_time, void* ctx)
{
	unsigned frames, i, c;
	const float* src;
	(void)dev; (void)now; (void)in_time; (void)out; (void)out_time; (void)ctx;
	if (!in || in->mNumberBuffers == 0)
		return noErr;
	if (in->mNumberBuffers == 1 && in->mBuffers[0].mNumberChannels >= 4) {
		unsigned ch = in->mBuffers[0].mNumberChannels;
		frames = in->mBuffers[0].mDataByteSize / (ch * sizeof(float));
		if (ch == 4) {
			src = in->mBuffers[0].mData;
		} else {
			if (frames * 4 > g.inter_len)
				frames = g.inter_len / 4;
			for (i = 0; i < frames; i++)
				for (c = 0; c < 4; c++)
					g.inter[i * 4 + c] = ((const float*)in->mBuffers[0].mData)[i * ch + c];
			src = g.inter;
		}
	} else if (in->mNumberBuffers >= 4) {
		frames = in->mBuffers[0].mDataByteSize / sizeof(float);
		if (frames * 4 > g.inter_len)
			frames = g.inter_len / 4;
		for (c = 0; c < 4; c++) {
			const float* p = in->mBuffers[c].mData;
			for (i = 0; i < frames; i++)
				g.inter[i * 4 + c] = p ? p[i] : 0.0f;
		}
		src = g.inter;
	} else {
		return noErr; // not the 4-channel layout we know
	}
	while (frames > 0) {
		unsigned n = frames > 1024 ? 1024 : frames;
		freenect_dsp_process_float(g.dsp, src, (int)n, g.out16);
		ring_push(g.out16, n);
		src += n * 4;
		frames -= n;
		g.frames_in += n;
	}
	return noErr;
}

// The sink's output: the beam.
static OSStatus sink_proc(AudioObjectID dev, const AudioTimeStamp* now, const AudioBufferList* in, const AudioTimeStamp* in_time,
                          AudioBufferList* out, const AudioTimeStamp* out_time, void* ctx)
{
	unsigned b;
	(void)dev; (void)now; (void)in; (void)in_time; (void)out_time; (void)ctx;
	if (!out)
		return noErr;
	for (b = 0; b < out->mNumberBuffers; b++) {
		float* dst = out->mBuffers[b].mData;
		unsigned ch = out->mBuffers[b].mNumberChannels ? out->mBuffers[b].mNumberChannels : 1;
		unsigned frames = out->mBuffers[b].mDataByteSize / (ch * sizeof(float)), i, c;
		unsigned w = atomic_load_explicit(&g.wpos, memory_order_acquire), r = atomic_load_explicit(&g.rpos, memory_order_relaxed);
		if (!dst)
			continue;
		if (b > 0) { // only one mono stream is expected; keep others silent
			memset(dst, 0, out->mBuffers[b].mDataByteSize);
			continue;
		}
		if (w - r > RING_HIGH) {
			r = w - RING_LOW;
			g.drops++;
		}
		for (i = 0; i < frames; i++) {
			float v = 0.0f;
			if (r != w)
				v = g.ring[r++ & (RING - 1)];
			else if (g.kinect != kAudioObjectUnknown)
				g.underruns++;
			for (c = 0; c < ch; c++)
				dst[i * ch + c] = v;
		}
		atomic_store_explicit(&g.rpos, r, memory_order_release);
	}
	return noErr;
}

static void stop_kinect(void)
{
	if (g.kinect != kAudioObjectUnknown && g.in_proc) {
		AudioDeviceStop(g.kinect, g.in_proc);
		AudioDeviceDestroyIOProcID(g.kinect, g.in_proc);
	}
	g.in_proc = NULL;
	g.kinect = kAudioObjectUnknown;
}

static void start_kinect(AudioObjectID id)
{
	char name[256];
	CFStringRef s;
	set_rate(id, FREENECT_DSP_RATE);
	if (AudioDeviceCreateIOProcID(id, kinect_proc, NULL, &g.in_proc) != noErr) {
		fprintf(stderr, "Could not open the Kinect's microphones\n");
		g.in_proc = NULL;
		return;
	}
	if (AudioDeviceStart(id, g.in_proc) != noErr) {
		fprintf(stderr, "Could not start the Kinect's microphones (is microphone access allowed in System Settings > Privacy & "
		                "Security > Microphone?)\n");
		AudioDeviceDestroyIOProcID(id, g.in_proc);
		g.in_proc = NULL;
		return;
	}
	g.kinect = id;
	s = copy_string(id, kAudioObjectPropertyName);
	cstring(s, name, sizeof(name));
	if (s)
		CFRelease(s);
	fprintf(stderr, "Capturing the Kinect's microphones (%s)\n", name);
}

// Device list changes: the Kinect came or went.
static void rescan(void)
{
	AudioObjectID k = find_kinect(0);
	if (g.kinect != kAudioObjectUnknown && k != g.kinect) {
		fprintf(stderr, "The Kinect's microphones went away; waiting for them\n");
		stop_kinect();
	}
	if (g.kinect == kAudioObjectUnknown && k != kAudioObjectUnknown)
		start_kinect(k);
}

static OSStatus devices_changed(AudioObjectID id, UInt32 n, const AudioObjectPropertyAddress* a, void* ctx)
{
	(void)id; (void)n; (void)a; (void)ctx;
	dispatch_async(dispatch_get_main_queue(), ^{ rescan(); });
	return noErr;
}

/* ------------------------------------------------------------ self-test */

static struct {
	_Atomic unsigned long played, recorded;
	double phase, sum2;
	unsigned long n;
} st;

static OSStatus tone_proc(AudioObjectID dev, const AudioTimeStamp* now, const AudioBufferList* in, const AudioTimeStamp* in_time,
                          AudioBufferList* out, const AudioTimeStamp* out_time, void* ctx)
{
	unsigned i;
	(void)dev; (void)now; (void)in; (void)in_time; (void)out_time; (void)ctx;
	if (!out || out->mNumberBuffers < 1 || !out->mBuffers[0].mData)
		return noErr;
	for (i = 0; i < out->mBuffers[0].mDataByteSize / sizeof(float); i++) {
		((float*)out->mBuffers[0].mData)[i] = 0.5f * (float)sin(st.phase);
		st.phase += 2 * M_PI * 1000.0 / 16000.0;
	}
	atomic_fetch_add(&st.played, out->mBuffers[0].mDataByteSize / sizeof(float));
	return noErr;
}

static OSStatus record_proc(AudioObjectID dev, const AudioTimeStamp* now, const AudioBufferList* in, const AudioTimeStamp* in_time,
                            AudioBufferList* out, const AudioTimeStamp* out_time, void* ctx)
{
	unsigned i, n;
	(void)dev; (void)now; (void)in_time; (void)out; (void)out_time; (void)ctx;
	if (!in || in->mNumberBuffers < 1 || !in->mBuffers[0].mData)
		return noErr;
	n = in->mBuffers[0].mDataByteSize / sizeof(float);
	// after half a second, when the loop has filled
	if (atomic_load(&st.recorded) > 8000)
		for (i = 0; i < n; i++) {
			double v = ((const float*)in->mBuffers[0].mData)[i];
			st.sum2 += v * v;
			st.n++;
		}
	atomic_fetch_add(&st.recorded, n);
	return noErr;
}

static int self_test(void)
{
	AudioObjectID sink = device_by_uid(SINK_UID), mic = device_by_uid(MIC_UID);
	AudioDeviceIOProcID tp = NULL, rp = NULL;
	double rms;
	printf("Sink %s, microphone %s\n", sink != kAudioObjectUnknown ? "found" : "MISSING", mic != kAudioObjectUnknown ? "found" : "MISSING");
	if (sink == kAudioObjectUnknown || mic == kAudioObjectUnknown) {
		printf("KinectBeamMic.driver is not loaded (copy it to /Library/Audio/Plug-Ins/HAL and restart coreaudiod)\n");
		return 1;
	}
	printf("Microphone: %u input channel(s); sink: %u output channel(s)\n", (unsigned)channels(mic, kAudioObjectPropertyScopeInput),
	       (unsigned)channels(sink, kAudioObjectPropertyScopeOutput));
	if (AudioDeviceCreateIOProcID(sink, tone_proc, NULL, &tp) != noErr || AudioDeviceCreateIOProcID(mic, record_proc, NULL, &rp) != noErr ||
	    AudioDeviceStart(sink, tp) != noErr || AudioDeviceStart(mic, rp) != noErr) {
		printf("Could not run the devices\n");
		return 1;
	}
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 2.5, false);
	AudioDeviceStop(mic, rp);
	AudioDeviceStop(sink, tp);
	AudioDeviceDestroyIOProcID(mic, rp);
	AudioDeviceDestroyIOProcID(sink, tp);
	rms = st.n ? sqrt(st.sum2 / st.n) : 0.0;
	printf("Played %lu frames into the sink, recorded %lu from the microphone, level %.3f (the tone: %.3f)\n",
	       (unsigned long)atomic_load(&st.played), (unsigned long)atomic_load(&st.recorded), rms, 0.5 / sqrt(2.0));
	if (atomic_load(&st.recorded) == 0) {
		printf("Nothing recorded\n");
		return 1;
	}
	if (rms < 0.25) {
		printf("The tone did not come through%s\n", rms == 0.0 ? " (all silence: no microphone access for this program?)" : "");
		return rms == 0.0 ? 3 : 1;
	}
	printf("The tone came through the loop\n");
	return 0;
}

/* ------------------------------------------------------------ main */

static void usage(const char* argv0)
{
	printf("Usage: %s [--beam auto|adaptive|ANGLE] [--sdk-weights] [--noise-suppression] [--agc] [--verbose]\n"
	       "       %s --list | --self-test\n"
	       "The Kinect's microphones, beamformed, into \"Kinect Beamformed Microphone\" (KinectBeamMic.driver).\n",
	       argv0, argv0);
}

int main(int argc, char** argv)
{
	const char* beam = "auto";
	int sdk = 0, ns = 0, agc = 0, i;
	AudioObjectPropertyAddress devs = { kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, ELEMENT_MAIN };
	dispatch_source_t sigs[2];
	int signo[2] = { SIGINT, SIGTERM };

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--beam") == 0 && i + 1 < argc) beam = argv[++i];
		else if (strcmp(argv[i], "--sdk-weights") == 0) sdk = 1;
		else if (strcmp(argv[i], "--noise-suppression") == 0) ns = 1;
		else if (strcmp(argv[i], "--agc") == 0) agc = 1;
		else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) g.verbose = 1;
		else if (strcmp(argv[i], "--list") == 0) {
			printf("Audio devices:\n");
			find_kinect(1);
			return 0;
		} else if (strcmp(argv[i], "--self-test") == 0) {
			return self_test();
		} else {
			usage(argv[0]);
			return strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ? 0 : 2;
		}
	}

	g.dsp = freenect_dsp_create(NULL);
	g.inter_len = 4 * 8192;
	g.inter = calloc(g.inter_len, sizeof(float));
	g.out16 = calloc(1024, sizeof(int16_t));
	g.kinect = kAudioObjectUnknown;
	if (!g.dsp || !g.inter || !g.out16) {
		fprintf(stderr, "Out of memory\n");
		return 1;
	}
	if (strcmp(beam, "adaptive") == 0) {
		freenect_dsp_set_beam_mode(g.dsp, FREENECT_DSP_BEAM_ADAPTIVE);
	} else if (strcmp(beam, "auto") != 0) {
		char* end;
		double a = strtod(beam, &end);
		if (*end || a < -50 || a > 50) {
			fprintf(stderr, "--beam: auto, adaptive or an angle from -50 to 50 degrees\n");
			return 2;
		}
		freenect_dsp_set_beam_mode(g.dsp, FREENECT_DSP_BEAM_MANUAL);
		freenect_dsp_set_beam(g.dsp, (int)lround((a + 50) / 10));
	}
	if (sdk && freenect_dsp_load_sdk_weights(g.dsp, NULL) < 0)
		fprintf(stderr, "Microsoft's beam weights were not found; using libfreenect's own\n");
	if (ns && freenect_dsp_set_noise_suppression(g.dsp, 1) < 0)
		fprintf(stderr, "Noise suppression needs libfreenect built with SpeexDSP\n");
	if (agc && freenect_dsp_set_agc(g.dsp, 1) < 0)
		fprintf(stderr, "Automatic gain control needs libfreenect built with SpeexDSP\n");

	g.sink = device_by_uid(SINK_UID);
	if (g.sink == kAudioObjectUnknown) {
		fprintf(stderr, "KinectBeamMic.driver is not loaded: copy it to /Library/Audio/Plug-Ins/HAL, then run "
		                "sudo killall coreaudiod\n");
		return 1;
	}
	if (AudioDeviceCreateIOProcID(g.sink, sink_proc, NULL, &g.out_proc) != noErr || AudioDeviceStart(g.sink, g.out_proc) != noErr) {
		fprintf(stderr, "Could not start the beamformed microphone's sink\n");
		return 1;
	}
	fprintf(stderr, "\"Kinect Beamformed Microphone\" is fed; waiting for the Kinect's microphones\n");
	AudioObjectAddPropertyListener(kAudioObjectSystemObject, &devs, devices_changed, NULL);
	rescan();

	for (i = 0; i < 2; i++) {
		signal(signo[i], SIG_IGN);
		sigs[i] = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, (uintptr_t)signo[i], 0, dispatch_get_main_queue());
		dispatch_source_set_event_handler(sigs[i], ^{ CFRunLoopStop(CFRunLoopGetMain()); });
		dispatch_resume(sigs[i]);
	}
	if (g.verbose) {
		dispatch_source_t t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
		dispatch_source_set_timer(t, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC), 5 * NSEC_PER_SEC, NSEC_PER_SEC / 10);
		dispatch_source_set_event_handler(t, ^{
			double a, c;
			if (g.kinect == kAudioObjectUnknown)
				return;
			if (freenect_dsp_get_source_angle(g.dsp, &a, &c) == 0)
				fprintf(stderr, "beam %+.0f deg, source %+.0f deg (confidence %.2f); %lu frames in, %lu underruns, %lu drops\n",
				        freenect_dsp_get_beam_angle(g.dsp) * 180 / M_PI, a * 180 / M_PI, c, g.frames_in, g.underruns, g.drops);
			else
				fprintf(stderr, "beam %+.0f deg, no source yet; %lu frames in\n", freenect_dsp_get_beam_angle(g.dsp) * 180 / M_PI,
				        g.frames_in);
		});
		dispatch_resume(t);
	}
	CFRunLoopRun();

	AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &devs, devices_changed, NULL);
	stop_kinect();
	AudioDeviceStop(g.sink, g.out_proc);
	AudioDeviceDestroyIOProcID(g.sink, g.out_proc);
	freenect_dsp_destroy(g.dsp);
	free(g.inter);
	free(g.out16);
	return 0;
}
