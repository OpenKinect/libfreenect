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

#include <windows.h>

/* File name of the installed firmware. */
#define UAC_FIRMWARE_NAME L"UACFirmware.bin"

/*
 * Download Microsoft's Kinect for Windows Runtime v1.8 installer from
 * download.microsoft.com, check its Authenticode signature, extract the
 * audio/motor controller firmware (UACFirmware.bin) from it without running
 * it, verify the firmware, and write it to dest_dir\UACFirmware.bin.
 * Nothing of Microsoft's is redistributed with libfreenect. Temporary files
 * are deleted. Returns 0 on success.
 *
 * If local_installer is not NULL, that file is used instead of downloading
 * (for offline installs and tests). It is checked in the same way.
 */
int uac_fetch_firmware(const wchar_t *dest_dir, const wchar_t *local_installer);

/* Default destination: %ProgramData%\libfreenect. */
void uac_default_dir(wchar_t *dir, size_t len);

/*
 * The Runtime installer, opened once for several files: kr_open() reads or
 * downloads it and checks Microsoft's signature; kr_extract() returns a file
 * of one of its MSI packages ("a0": drivers, "a1": runtime DLLs; malloc'ed,
 * NULL if missing); kr_close() deletes the temporary files.
 */
typedef struct {
	wchar_t tmp[MAX_PATH];
	char tmp_a[MAX_PATH];
	int ready;
} kinect_runtime;

int kr_open(kinect_runtime *kr, const wchar_t *local_installer);
BYTE *kr_extract(kinect_runtime *kr, const char *msi_name, const char *file, size_t *len);
void kr_close(kinect_runtime *kr);
/* Writes dest_dir\name, creating dest_dir. */
int kr_install(const wchar_t *dest_dir, const wchar_t *name, const BYTE *data, size_t len);
int kr_sha256_hex(const BYTE *data, size_t len, char hex[65]);
/* n bytes at virtual address va of an x64 PE image in memory, or NULL. */
const BYTE *kr_pe_at(const BYTE *img, size_t len, ULONGLONG va, size_t n);

/* UACFirmware.bin, from "a0". */
int kr_fetch_firmware(kinect_runtime *kr, const wchar_t *dest_dir);

/* Microsoft's beamformer weights for libfreenect's audio DSP, from
 * KinectAudio10.dll in "a1", verified and written as kinect_beam_weights.bin
 * (the same file as fwfetcher.py --audio-weights). */
#define AUDIO_WEIGHTS_NAME L"kinect_beam_weights.bin"
int kr_fetch_audio_weights(kinect_runtime *kr, const wchar_t *dest_dir);

/* The skeleton tracker's models (DATABASE.WSDF, HEADPOSITION.MSHDF) from
 * Kinect10.dll in "a1", copied unchanged and verified into dest_dir\nui
 * (the same files as fwfetcher.py --nui), for the libfreenect_nui addon. */
int kr_fetch_nui_models(kinect_runtime *kr, const wchar_t *dest_dir);
