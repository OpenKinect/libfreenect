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
#pragma once

#include "libfreenect_audio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Audio processing for the Kinect's 4-microphone array (library
 * freenect_audio_dsp): a beamformer, sound source localisation, and, when
 * built with SpeexDSP, echo cancellation, noise suppression and automatic
 * gain control. It follows the structure of Microsoft's Kinect audio DSP:
 * a 256-band MCLT filterbank (16 ms frames), 11 fixed beams from -50 to +50
 * degrees, beam selection by the source direction, and the runtime's
 * direction estimator and tracker.
 *
 * Input is 4-channel audio at 16 kHz from any source: libfreenect's audio
 * callback (Xbox firmware), or the operating system's capture of the USB
 * audio device (Microsoft's firmware). Output is mono 16 kHz.
 *
 * The beam weights are designed from the microphone positions (superdirective
 * beams with a limit on noise amplification), so nothing of Microsoft's is
 * needed. freenect_dsp_load_sdk_weights() can load Microsoft's own weights
 * from a file that freenect-driver-install or fwfetcher.py extract on the
 * user's machine; libfreenect never contains them.
 *
 * Angles are in radians, 0 straight ahead, positive towards microphone 4.
 */

#define FREENECT_DSP_RATE 16000
#define FREENECT_DSP_FRAME 256       ///< samples per processing frame (16 ms)
#define FREENECT_DSP_NUM_BEAMS 11    ///< fixed beams, -50..+50 degrees in 10 degree steps
#define FREENECT_DSP_LATENCY 512     ///< output delay in samples

typedef struct freenect_dsp freenect_dsp;

typedef enum {
	/// Pick among the fixed beams by the source direction, with hysteresis
	/// (the SDK's BeamAngleMode.Automatic). The default.
	FREENECT_DSP_BEAM_AUTOMATIC = 0,
	/// Steer a beam continuously to the source direction (BeamAngleMode.Adaptive).
	FREENECT_DSP_BEAM_ADAPTIVE = 1,
	/// Keep the beam set with freenect_dsp_set_beam() (BeamAngleMode.Manual).
	FREENECT_DSP_BEAM_MANUAL = 2,
} freenect_dsp_beam_mode;

/// A DSP for the given array geometry (NULL: the Kinect's). NULL if the
/// geometry is not a line of 2-4 microphones, or out of memory.
FREENECTAPI freenect_dsp* freenect_dsp_create(const freenect_mic_geometry* geometry);
FREENECTAPI void freenect_dsp_destroy(freenect_dsp* dsp);

/// Processes n frames of interleaved 4-channel audio (mic 1..4), full scale
/// 2^31 (the USB format, S32_LE), and writes n mono samples to out, delayed
/// by FREENECT_DSP_LATENCY. Any n. Returns n, or < 0 on error.
FREENECTAPI int freenect_dsp_process(freenect_dsp* dsp, const int32_t* frames, int n, int16_t* out);
/// The same with float samples, full scale 1.0 (the usual OS capture format).
FREENECTAPI int freenect_dsp_process_float(freenect_dsp* dsp, const float* frames, int n, int16_t* out);
/// The same with one array per microphone, as libfreenect's audio callback
/// delivers them.
FREENECTAPI int freenect_dsp_process_planar(freenect_dsp* dsp, const int32_t* mic1, const int32_t* mic2,
	const int32_t* mic3, const int32_t* mic4, int n, int16_t* out);

FREENECTAPI int freenect_dsp_set_beam_mode(freenect_dsp* dsp, freenect_dsp_beam_mode mode);
/// For FREENECT_DSP_BEAM_MANUAL: beam 0..10 (-50..+50 degrees).
FREENECTAPI int freenect_dsp_set_beam(freenect_dsp* dsp, int beam);
/// The direction the output listens to now.
FREENECTAPI double freenect_dsp_get_beam_angle(freenect_dsp* dsp);
/// The tracked sound source direction and its confidence, 0..1 (the SDK's
/// SoundSourceAngle and SoundSourceAngleConfidence). Returns 0, or -1 if no
/// source has been found yet.
FREENECTAPI int freenect_dsp_get_source_angle(freenect_dsp* dsp, double* angle, double* confidence);

/// Loads Microsoft's beam weights from the file that freenect-driver-install
/// or fwfetcher.py --audio-weights write (path NULL: the usual locations).
/// Returns 0, or < 0 if there is none or it is not valid; the designed
/// weights stay in use then. Only for the Kinect's own geometry.
FREENECTAPI int freenect_dsp_load_sdk_weights(freenect_dsp* dsp, const char* path);
/// 1 while Microsoft's weights are in use, 0 with the designed ones.
FREENECTAPI int freenect_dsp_using_sdk_weights(freenect_dsp* dsp);

/// Noise suppression, automatic gain control and echo cancellation, on the
/// beam output. Need libfreenect built with SpeexDSP; -ENOTSUP otherwise.
FREENECTAPI int freenect_dsp_set_noise_suppression(freenect_dsp* dsp, int on);
FREENECTAPI int freenect_dsp_set_agc(freenect_dsp* dsp, int on);
FREENECTAPI int freenect_dsp_set_echo_cancellation(freenect_dsp* dsp, int on);
/// For echo cancellation: what the loudspeakers play, mono 16 kHz, in step
/// with the microphone input (call before the matching process call).
FREENECTAPI int freenect_dsp_feed_reference(freenect_dsp* dsp, const int16_t* samples, int n);

#ifdef __cplusplus
}
#endif
