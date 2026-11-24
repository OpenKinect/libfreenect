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

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS // fopen, getenv
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "models.h"

static uint8_t* slurp(const char* path, size_t* len)
{
	FILE* f = fopen(path, "rb");
	uint8_t* b = NULL;
	long n;
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0 &&
	    (b = (uint8_t*)malloc((size_t)n)) != NULL && fread(b, 1, (size_t)n, f) == (size_t)n) {
		*len = (size_t)n;
	} else {
		free(b);
		b = NULL;
	}
	fclose(f);
	return b;
}

/*
 * Where fwfetcher.py --nui and freenect-driver-install --fetch-nui-models put
 * the models: $LIBFREENECT_NUI_MODELS, then the "nui" folder of each place
 * the audio firmware is looked for.
 */
uint8_t* fn_nui_read_model(const char* dir, const char* name, size_t* len, char* found, size_t found_size)
{
	char path[1024];
	const char* e;
	uint8_t* b;
#define TRY(base, sub) do { if (base) { snprintf(path, sizeof(path), "%s%s/%s", base, sub, name); \
		if ((b = slurp(path, len)) != NULL) { if (found) snprintf(found, found_size, "%s", path); return b; } } } while (0)
	if (dir) {
		TRY(dir, "");
		return NULL;
	}
	TRY(getenv("LIBFREENECT_NUI_MODELS"), "");
	if ((e = getenv("LIBFREENECT_FIRMWARE_PATH")) != NULL)
		TRY(e, "/nui");
#ifdef _WIN32
	TRY(getenv("LOCALAPPDATA"), "/libfreenect/nui");
	TRY(getenv("ProgramData"), "/libfreenect/nui");
#else
	if ((e = getenv("XDG_DATA_HOME")) != NULL && e[0] == '/')
		TRY(e, "/libfreenect/nui");
	else
		TRY(getenv("HOME"), "/.local/share/libfreenect/nui");
#ifdef __APPLE__
	TRY(getenv("HOME"), "/Library/Application Support/libfreenect/nui");
#endif
	TRY("/usr/local/share/libfreenect", "/nui");
	TRY("/usr/share/libfreenect", "/nui");
#endif
#undef TRY
	return NULL;
}
