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
#pragma once

#include "libfreenect.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Structure to represent a single 16-bit signed little-endian PCM sample.
typedef struct {
	int16_t left;
	int16_t right;
	int16_t center;
	int16_t lfe;
	int16_t surround_left;
	int16_t surround_right;
} freenect_sample_51;

/**
 * Typedef for "you wanted this microphone data, here it is" event callbacks.
 * TODO: Timestamp details
 * The format of the unknown stream is as of yet undetermined.
 *
 * @param dev Device which triggered this callback
 * @param num_samples Number of samples provided in each of the audio data arrays (mic[1-4] and cancelled)
 * @param mic1 Microphone data for the leftmost microphone: 32-bit PCM little-endian samples at 16kHz.
 * @param mic2 Microphone data for the left-middle microphone: 32-bit PCM little-endian samples at 16kHz.
 * @param mic3 Microphone data for the right-middle microphone: 32-bit PCM little-endian samples at 16kHz.
 * @param mic4 Microphone data for the rightmost microphone: 32-bit PCM little-endian samples at 16kHz.
 * @param cancelled Noise-cancelled audio data: 16-bit PCM little-endian samples at 16kHz.
 */
typedef void (*freenect_audio_in_cb)(freenect_device *dev, int num_samples,
                                     int32_t* mic1, int32_t* mic2,
                                     int32_t* mic3, int32_t* mic4,
                                     int16_t* cancelled, void *unknown/*, timestamp_t timestamp*/);

/**
 * Typedef for "you're playing audio, the library needs you to fill up the outgoing audio buffer" event callbacks
 * The library will request samples at a rate of 48000Hz.
 *
 * @param dev Device this callback was triggered for
 * @param samples Pointer to the memory where the library expects you to copy the next sample_count freenect_sample_51's to.
 * @param sample_count Bidirectional. in: maximum number of samples the driver wants (don't copy in more than this, you'll clobber memory).  out: actual number of samples provided to the driver.
 */
typedef void (*freenect_audio_out_cb)(freenect_device *dev, freenect_sample_51* samples, int* sample_count);

/**
 * Set the audio in callback.  This is the function called when the library
 * has new microphone samples.  It will be called approximately 62.5 times per
 * second (16kHz sample rate, expect 512 samples/callback)
 *
 * @param dev Device for which to set the callback
 * @param callback Callback function to set
 */
FREENECTAPI void freenect_set_audio_in_callback(freenect_device *dev, freenect_audio_in_cb callback);

/**
 * Set the audio out callback.  This is the "tell me what audio you're about
 * to play through the speakers so the Kinect can subtract it out" callback for
 * a given device.  If you choose not set an audio_out_callback, the library
 * will send silence to the Kinect for you - it requires data either way.
 *
 * @param dev Device for which to set the callback
 * @param callback Callback function to set
 */
FREENECTAPI void freenect_set_audio_out_callback(freenect_device *dev, freenect_audio_out_cb callback);

/**
 * Start streaming audio for the specified device.
 *
 * @param dev Device for which to start audio streaming
 *
 * @return 0 on success, < 0 if error
 */
FREENECTAPI int freenect_start_audio(freenect_device* dev);

/**
 * Stop streaming audio for the specified device.
 *
 * @param dev Device for which to stop audio streaming
 *
 * @return 0 on success, < 0 if error
 */
FREENECTAPI int freenect_stop_audio(freenect_device* dev);

/**
 * Whether freenect_start_audio() can stream from this device.
 *
 * libfreenect speaks the audio protocol of the Xbox firmware (audios.bin).
 * With Microsoft's UAC firmware (UAC.BIN) the microphones are a standard USB
 * audio device of the operating system (4 channels, 32-bit, 16 kHz): record
 * from it with ALSA, PipeWire, Core Audio or WASAPI instead. On Windows,
 * libfreenect's own audio streaming is off unless
 * LIBFREENECT_WINDOWS_AUDIO_UNSAFE=1, because it hung a test machine.
 *
 * @param dev Device to ask
 *
 * @return 1 if the audio controller is open and freenect_start_audio() can
 *         stream from it, 0 otherwise
 */
FREENECTAPI int freenect_audio_streaming_available(freenect_device* dev);

#define FREENECT_MAX_MICS 4

/// One microphone of the array. Positions in mm: x forward (away from the
/// Kinect's front), y along the array, z up. Angles in 1/10000 radian.
typedef struct {
	uint16_t type;          ///< 2: cardioid
	int16_t x_mm;
	int16_t y_mm;
	int16_t z_mm;
	int16_t vertical_angle;
	int16_t horizontal_angle;
} freenect_mic;

/// The microphone array, as the Kinect's audio firmware describes it to the
/// operating system (Microsoft's microphone array descriptor).
typedef struct {
	uint16_t array_type;            ///< 0: linear
	int16_t vertical_angle_begin;   ///< working range, 1/10000 radian
	int16_t vertical_angle_end;
	int16_t horizontal_angle_begin;
	int16_t horizontal_angle_end;
	uint16_t freq_low_hz;           ///< working frequency band
	uint16_t freq_high_hz;
	int num_mics;
	freenect_mic mics[FREENECT_MAX_MICS]; ///< in the order of the audio channels (mic1..mic4)
} freenect_mic_geometry;

/**
 * Read the microphone array geometry from the device's audio controller
 * (either firmware). Needs FREENECT_DEVICE_AUDIO to be open.
 *
 * @param dev Device to ask
 * @param out Where to write the geometry
 *
 * @return 1 if it came from the device; 0 if the device could not be asked
 *         (audio not open, or no valid answer), in which case *out holds the
 *         values every Kinect reports (y = -113, 36, 76, 113 mm); -1 if out
 *         is NULL
 */
FREENECTAPI int freenect_get_mic_geometry(freenect_device* dev, freenect_mic_geometry* out);

#ifdef __cplusplus
}
#endif
