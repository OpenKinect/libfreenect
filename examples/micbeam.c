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

// freenect-micbeam: the Kinect's microphone array through libfreenect's
// audio DSP (beamformer and source direction).
//
// Usage: freenect-micbeam [source] [options]
// Sources:
//   --wasapi [N]   (Windows, default) the Kinect's microphone array as a
//                  Windows audio device (Microsoft's UAC firmware), through
//                  WASAPI; N picks another capture device (see --list)
//   --list         (Windows) list the capture devices
//   --kinect       libfreenect's own audio stream (Xbox firmware audios.bin;
//                  default on Linux and macOS)
//   --wav FILE     a 4-channel 16 kHz WAV file ("-": standard input), e.g.
//                  arecord -D hw:Kinect -c4 -f S32_LE -r16000 -t wav - | freenect-micbeam --wav -
// Options:
//   --seconds N    stop after N seconds (default 15; 0: until Ctrl+C)
//   --beam auto|adaptive|N   beam choice (default auto; N = 0..10 for -50..+50 degrees)
//   --sdk-weights  use Microsoft's weights (installed by freenect-driver-install
//                  or fwfetcher.py --audio-weights)
//   --ns, --agc    noise suppression, gain control (builds with SpeexDSP)
//   --out PREFIX   output files (default "micbeam"): PREFIX_beam.wav (mono,
//                  processed) and PREFIX_raw.wav (the 4 microphones)
//
// Once a second it prints the input level, the source direction and its
// confidence, and the beam in use.

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS // fopen, strncpy
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libfreenect.h"
#include "libfreenect_audio.h"
#include "libfreenect_audio_dsp.h"
#include "console_exit.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static freenect_dsp* dsp;
static FILE *beam_wav, *raw_wav;
static uint32_t beam_samples, raw_samples;
static double level_acc;
static uint32_t level_n, total_n;

/* ------------------------------------------------------------ WAV files */

static void wav_header(FILE* f, int channels, int bits, int fmt, uint32_t frames)
{
	uint32_t data = frames * (uint32_t)channels * (uint32_t)(bits / 8), v;
	uint16_t s;
	fseek(f, 0, SEEK_SET);
	fwrite("RIFF", 1, 4, f);
	v = 36 + data; fwrite(&v, 4, 1, f);
	fwrite("WAVEfmt ", 1, 8, f);
	v = 16; fwrite(&v, 4, 1, f);
	s = (uint16_t)fmt; fwrite(&s, 2, 1, f);
	s = (uint16_t)channels; fwrite(&s, 2, 1, f);
	v = FREENECT_DSP_RATE; fwrite(&v, 4, 1, f);
	v = FREENECT_DSP_RATE * (uint32_t)channels * (uint32_t)(bits / 8); fwrite(&v, 4, 1, f);
	s = (uint16_t)(channels * bits / 8); fwrite(&s, 2, 1, f);
	s = (uint16_t)bits; fwrite(&s, 2, 1, f);
	fwrite("data", 1, 4, f);
	v = data; fwrite(&v, 4, 1, f);
	fseek(f, 0, SEEK_END);
}

// A 4-channel float buffer (full scale 1): through the DSP and into the files.
static void consume(const float* frames, int n)
{
	int16_t* out = (int16_t*)malloc(sizeof(int16_t) * (size_t)n);
	int i;
	if (!out)
		return;
	freenect_dsp_process_float(dsp, frames, n, out);
	if (beam_wav) {
		fwrite(out, sizeof(int16_t), (size_t)n, beam_wav);
		beam_samples += (uint32_t)n;
	}
	if (raw_wav) {
		fwrite(frames, sizeof(float) * 4, (size_t)n, raw_wav);
		raw_samples += (uint32_t)n;
	}
	for (i = 0; i < 4 * n; i++)
		level_acc += (double)frames[i] * frames[i];
	level_n += (uint32_t)(4 * n);
	total_n += (uint32_t)n;
	free(out);
}

static void report(unsigned sec)
{
	double a, c;
	double db = level_n ? 10.0 * log10(level_acc / level_n + 1e-20) : -200.0;
	if (freenect_dsp_get_source_angle(dsp, &a, &c) == 0)
		printf("%3u s: level %6.1f dBFS; source %+5.1f deg, confidence %.2f; beam %+5.1f deg\n",
		       sec, db, a * 180.0 / M_PI, c, freenect_dsp_get_beam_angle(dsp) * 180.0 / M_PI);
	else
		printf("%3u s: level %6.1f dBFS; no source yet; beam %+5.1f deg\n",
		       sec, db, freenect_dsp_get_beam_angle(dsp) * 180.0 / M_PI);
	fflush(stdout);
	level_acc = 0.0;
	level_n = 0;
}

/* Reads a WAV file: 4 or more channels at 16 kHz, 16/32-bit PCM or 32-bit
 * float (also WAVE_FORMAT_EXTENSIBLE). */
static int run_wav(const char* path, unsigned seconds)
{
	FILE* f = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
	uint8_t hdr[12], ck[8];
	int channels = 0, bits = 0, fmt = 0, rate = 0;
	unsigned next = 1;
	if (!f) {
		printf("Cannot open %s\n", path);
		return 1;
	}
	if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) || memcmp(hdr + 8, "WAVE", 4)) {
		printf("%s is not a WAV file\n", path);
		return 1;
	}
	while (fread(ck, 1, 8, f) == 8) {
		uint32_t len = (uint32_t)ck[4] | ((uint32_t)ck[5] << 8) | ((uint32_t)ck[6] << 16) | ((uint32_t)ck[7] << 24);
		if (memcmp(ck, "fmt ", 4) == 0) {
			uint8_t b[40];
			uint32_t take = len < sizeof(b) ? len : sizeof(b), i;
			if (fread(b, 1, take, f) != take)
				return 1;
			for (i = take; i < len; i++)
				fgetc(f);
			fmt = b[0] | (b[1] << 8);
			channels = b[2] | (b[3] << 8);
			rate = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24);
			bits = b[14] | (b[15] << 8);
			if (fmt == 0xFFFE && take >= 26)
				fmt = b[24] | (b[25] << 8); // sub-format: 1 PCM, 3 float
			if (len & 1)
				fgetc(f);
		} else if (memcmp(ck, "data", 4) == 0) {
			break;
		} else {
			uint32_t i;
			for (i = 0; i < len + (len & 1); i++)
				fgetc(f);
		}
	}
	if (channels < 4 || rate != FREENECT_DSP_RATE ||
	    !((fmt == 1 && (bits == 16 || bits == 32)) || (fmt == 3 && bits == 32))) {
		printf("Need 4 or more channels at 16 kHz, 16/32-bit PCM or 32-bit float (got %d channels, %d Hz, format %d, %d bits)\n",
		       channels, rate, fmt, bits);
		return 1;
	}
	printf("Reading %s: %d channels, 16 kHz, %s %d-bit\n", path, channels, fmt == 3 ? "float" : "PCM", bits);
	{
		int bps = bits / 8, fsz = channels * bps, i, m;
		uint8_t* in = (uint8_t*)malloc((size_t)fsz * 1600);
		float* fr = (float*)malloc(sizeof(float) * 4 * 1600);
		size_t got;
		while (in && fr && !fn_exit_requested && (got = fread(in, (size_t)fsz, 1600, f)) > 0) {
			for (i = 0; i < (int)got; i++)
				for (m = 0; m < 4; m++) {
					const uint8_t* p = in + (size_t)i * fsz + (size_t)m * bps;
					if (fmt == 3) {
						float v;
						memcpy(&v, p, 4);
						fr[4 * i + m] = v;
					} else if (bits == 16) {
						fr[4 * i + m] = (int16_t)(p[0] | (p[1] << 8)) / 32768.0f;
					} else {
						int32_t v = (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
						fr[4 * i + m] = (float)v / 2147483648.0f;
					}
				}
			consume(fr, (int)got);
			while (total_n >= next * FREENECT_DSP_RATE) {
				report(next);
				next++;
			}
			if (seconds && total_n >= seconds * FREENECT_DSP_RATE)
				break;
		}
		free(in);
		free(fr);
	}
	if (f != stdin)
		fclose(f);
	return 0;
}

/* ------------------------------------------------ libfreenect's stream */

static void kinect_in(freenect_device* dev, int n, int32_t* m1, int32_t* m2, int32_t* m3, int32_t* m4,
                      int16_t* cancelled, void* unknown)
{
	float* fr = (float*)malloc(sizeof(float) * 4 * (size_t)n);
	int i;
	(void)dev; (void)cancelled; (void)unknown;
	if (!fr)
		return;
	for (i = 0; i < n; i++) {
		fr[4 * i + 0] = (float)m1[i] / 2147483648.0f;
		fr[4 * i + 1] = (float)m2[i] / 2147483648.0f;
		fr[4 * i + 2] = (float)m3[i] / 2147483648.0f;
		fr[4 * i + 3] = (float)m4[i] / 2147483648.0f;
	}
	consume(fr, n);
	free(fr);
}

static int run_kinect(unsigned seconds)
{
	freenect_context* ctx;
	freenect_device* dev;
	unsigned next = 1;
	int ret = 1;
	if (freenect_init(&ctx, NULL) < 0)
		return 1;
	freenect_select_subdevices(ctx, FREENECT_DEVICE_AUDIO);
	if (freenect_num_devices(ctx) < 1 || freenect_open_device(ctx, &dev, 0) < 0) {
		printf("No Kinect audio controller found\n");
		freenect_shutdown(ctx);
		return 1;
	}
	{
		freenect_mic_geometry g;
		if (freenect_get_mic_geometry(dev, &g) == 1) {
			freenect_dsp_destroy(dsp);
			dsp = freenect_dsp_create(&g);
		}
	}
	if (!freenect_audio_streaming_available(dev)) {
		printf("libfreenect cannot stream audio from this Kinect (it runs Microsoft's UAC firmware, or this is\n"
		       "Windows). Record from the operating system's audio device instead (see --help).\n");
		goto out;
	}
	freenect_set_audio_in_callback(dev, kinect_in);
	if (freenect_start_audio(dev) < 0)
		goto out;
	while (!fn_exit_requested) {
		struct timeval tv = { 0, 100000 };
		if (freenect_process_events_timeout(ctx, &tv) < 0)
			break;
		while (total_n >= next * FREENECT_DSP_RATE) {
			report(next);
			next++;
		}
		if (seconds && total_n >= seconds * FREENECT_DSP_RATE)
			break;
	}
	freenect_stop_audio(dev);
	ret = 0;
out:
	freenect_close_device(dev);
	freenect_shutdown(ctx);
	return ret;
}

/* ------------------------------------------------ WASAPI (Windows) */

#ifdef _WIN32
#define COBJMACROS
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>

// The GUIDs this needs, spelled out (in C they would otherwise need extra
// import libraries).
static const GUID fn_CLSID_MMDeviceEnumerator = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const GUID fn_IID_IMMDeviceEnumerator = { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
static const GUID fn_IID_IAudioClient = { 0x1CB9AD4C, 0xDBFA, 0x4C32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
static const GUID fn_IID_IAudioCaptureClient = { 0xC8ADBD64, 0xE71E, 0x48A0, { 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17 } };
static const GUID fn_SUBTYPE_IEEE_FLOAT = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

static IMMDevice* pick_device(IMMDeviceEnumerator* en, int index, int list)
{
	IMMDeviceCollection* col = NULL;
	IMMDevice* found = NULL;
	UINT n = 0, i;
	if (FAILED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eCapture, DEVICE_STATE_ACTIVE, &col)))
		return NULL;
	IMMDeviceCollection_GetCount(col, &n);
	for (i = 0; i < n; i++) {
		IMMDevice* d = NULL;
		IPropertyStore* ps = NULL;
		PROPVARIANT name;
		int kinect = 0;
		PropVariantInit(&name);
		if (FAILED(IMMDeviceCollection_Item(col, i, &d)))
			continue;
		if (SUCCEEDED(IMMDevice_OpenPropertyStore(d, STGM_READ, &ps)) &&
		    SUCCEEDED(IPropertyStore_GetValue(ps, &PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR) {
			kinect = wcsstr(name.pwszVal, L"Kinect") || wcsstr(name.pwszVal, L"Xbox NUI") ||
			         wcsstr(name.pwszVal, L"NUI Sensor");
			if (list)
				printf("  %u: %ls%s\n", i, name.pwszVal, kinect ? "  (Kinect)" : "");
		}
		PropVariantClear(&name);
		if (ps)
			IPropertyStore_Release(ps);
		if (!found && !list && ((index >= 0 && (UINT)index == i) || (index < 0 && kinect))) {
			found = d;
			continue;
		}
		IMMDevice_Release(d);
	}
	IMMDeviceCollection_Release(col);
	return found;
}

static int run_wasapi(int index, int list, unsigned seconds)
{
	IMMDeviceEnumerator* en = NULL;
	IMMDevice* dev = NULL;
	IAudioClient* ac = NULL;
	IAudioCaptureClient* cap = NULL;
	WAVEFORMATEXTENSIBLE wf;
	unsigned next = 1;
	int ret = 1;
	HRESULT hr;

	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(CoCreateInstance(&fn_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &fn_IID_IMMDeviceEnumerator, (void**)&en))) {
		printf("No Windows audio\n");
		goto out;
	}
	if (list) {
		printf("Capture devices:\n");
		pick_device(en, -1, 1);
		ret = 0;
		goto out;
	}
	dev = pick_device(en, index, 0);
	if (!dev) {
		printf(index < 0 ? "No Kinect microphone array among the Windows capture devices (--list shows them).\n"
		                 : "No capture device %d (--list shows them).\n", index);
		goto out;
	}
	if (FAILED(IMMDevice_Activate(dev, &fn_IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&ac)))
		goto out;
	// Ask for 4 channels, float, 16 kHz; Windows converts if its mix format differs.
	memset(&wf, 0, sizeof(wf));
	wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	wf.Format.nChannels = 4;
	wf.Format.nSamplesPerSec = FREENECT_DSP_RATE;
	wf.Format.wBitsPerSample = 32;
	wf.Format.nBlockAlign = 16;
	wf.Format.nAvgBytesPerSec = FREENECT_DSP_RATE * 16;
	wf.Format.cbSize = 22;
	wf.Samples.wValidBitsPerSample = 32;
	wf.dwChannelMask = 0;
	wf.SubFormat = fn_SUBTYPE_IEEE_FLOAT;
	hr = IAudioClient_Initialize(ac, AUDCLNT_SHAREMODE_SHARED,
	                             AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
	                             1000000 /* 100 ms */, 0, (WAVEFORMATEX*)&wf, NULL);
	if (FAILED(hr)) {
		printf("The device does not take 4 channels at 16 kHz (0x%08lx); is it the Kinect's array?\n", (unsigned long)hr);
		goto out;
	}
	if (FAILED(IAudioClient_GetService(ac, &fn_IID_IAudioCaptureClient, (void**)&cap)) || FAILED(IAudioClient_Start(ac)))
		goto out;
	printf("Capturing from the Kinect's microphone array through WASAPI (4 channels, 16 kHz)\n");
	fflush(stdout);
	while (!fn_exit_requested) {
		UINT32 packet = 0;
		Sleep(10);
		while (SUCCEEDED(IAudioCaptureClient_GetNextPacketSize(cap, &packet)) && packet > 0) {
			BYTE* data;
			UINT32 frames;
			DWORD flags;
			if (FAILED(IAudioCaptureClient_GetBuffer(cap, &data, &frames, &flags, NULL, NULL)))
				break;
			if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
				float* z = (float*)calloc((size_t)frames * 4, sizeof(float));
				if (z) {
					consume(z, (int)frames);
					free(z);
				}
			} else {
				consume((const float*)data, (int)frames);
			}
			IAudioCaptureClient_ReleaseBuffer(cap, frames);
		}
		while (total_n >= next * FREENECT_DSP_RATE) {
			report(next);
			next++;
		}
		if (seconds && total_n >= seconds * FREENECT_DSP_RATE)
			break;
	}
	IAudioClient_Stop(ac);
	ret = 0;
out:
	if (cap) IAudioCaptureClient_Release(cap);
	if (ac) IAudioClient_Release(ac);
	if (dev) IMMDevice_Release(dev);
	if (en) IMMDeviceEnumerator_Release(en);
	CoUninitialize();
	return ret;
}
#endif

/* ------------------------------------------------------------ main */

static void usage(void)
{
	printf("Usage: freenect-micbeam [--wasapi [N] | --list | --kinect | --wav FILE] [--seconds N]\n"
	       "                        [--beam auto|adaptive|0..10] [--sdk-weights] [--ns] [--agc] [--out PREFIX]\n");
}

int main(int argc, char** argv)
{
	enum { SRC_WASAPI, SRC_KINECT, SRC_WAV } src;
	const char *wav = NULL, *prefix = "micbeam";
	unsigned seconds = 15;
	int i, ret, dev_index = -1, list = 0, sdk = 0, ns = 0, agc = 0;
	const char* beam = "auto";
	char path[512];

	fn_install_exit_handlers();
#ifdef _WIN32
	src = SRC_WASAPI;
#else
	src = SRC_KINECT;
#endif
	for (i = 1; i < argc; i++) {
		const char* a = argv[i];
		if (strcmp(a, "--wasapi") == 0) {
			src = SRC_WASAPI;
			if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
				dev_index = atoi(argv[++i]);
		} else if (strcmp(a, "--list") == 0) { src = SRC_WASAPI; list = 1; }
		else if (strcmp(a, "--kinect") == 0) src = SRC_KINECT;
		else if (strcmp(a, "--wav") == 0 && i + 1 < argc) { src = SRC_WAV; wav = argv[++i]; }
		else if (strcmp(a, "--seconds") == 0 && i + 1 < argc) seconds = (unsigned)atoi(argv[++i]);
		else if (strcmp(a, "--beam") == 0 && i + 1 < argc) beam = argv[++i];
		else if (strcmp(a, "--sdk-weights") == 0) sdk = 1;
		else if (strcmp(a, "--ns") == 0) ns = 1;
		else if (strcmp(a, "--agc") == 0) agc = 1;
		else if (strcmp(a, "--out") == 0 && i + 1 < argc) prefix = argv[++i];
		else { usage(); return strcmp(a, "--help") == 0 ? 0 : 2; }
	}

	dsp = freenect_dsp_create(NULL);
	if (!dsp)
		return 1;
	if (strcmp(beam, "adaptive") == 0)
		freenect_dsp_set_beam_mode(dsp, FREENECT_DSP_BEAM_ADAPTIVE);
	else if (strcmp(beam, "auto") != 0) {
		freenect_dsp_set_beam_mode(dsp, FREENECT_DSP_BEAM_MANUAL);
		if (freenect_dsp_set_beam(dsp, atoi(beam)) < 0) {
			usage();
			return 2;
		}
	}
	if (sdk) {
		int r = freenect_dsp_load_sdk_weights(dsp, NULL);
		printf(r == 0 ? "Using Microsoft's beam weights\n"
		              : "Microsoft's beam weights are not installed (freenect-driver-install --fetch-firmware,\n"
		                "or fwfetcher.py --audio-weights); using libfreenect's\n");
	}
	if ((ns && freenect_dsp_set_noise_suppression(dsp, 1) < 0) || (agc && freenect_dsp_set_agc(dsp, 1) < 0))
		printf("Noise suppression and gain control need a libfreenect built with SpeexDSP; continuing without\n");

	if (!list) {
		snprintf(path, sizeof(path), "%s_beam.wav", prefix);
		beam_wav = fopen(path, "wb");
		snprintf(path, sizeof(path), "%s_raw.wav", prefix);
		raw_wav = fopen(path, "wb");
		if (beam_wav) wav_header(beam_wav, 1, 16, 1, 0);
		if (raw_wav) wav_header(raw_wav, 4, 32, 3, 0);
	}

	switch (src) {
	case SRC_WAV: ret = run_wav(wav, seconds); break;
	case SRC_KINECT: ret = run_kinect(seconds); break;
	default:
#ifdef _WIN32
		ret = run_wasapi(dev_index, list, seconds);
#else
		(void)dev_index;
		printf("WASAPI is Windows only; use --kinect or --wav\n");
		ret = 2;
#endif
		break;
	}

	if (beam_wav) {
		wav_header(beam_wav, 1, 16, 1, beam_samples);
		fclose(beam_wav);
	}
	if (raw_wav) {
		wav_header(raw_wav, 4, 32, 3, raw_samples);
		fclose(raw_wav);
	}
	if (!list)
		printf("Wrote %s_beam.wav (%.1f s, processed) and %s_raw.wav (the 4 microphones)\n",
		       prefix, beam_samples / (double)FREENECT_DSP_RATE, prefix);
	freenect_dsp_destroy(dsp);
	printf("Done!\n");
	return fn_exit_done(ret);
}
