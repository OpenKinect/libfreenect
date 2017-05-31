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
#pragma once

#include <math.h>
#ifdef _WIN32
	#include <direct.h>
	#include <windows.h>
	#if defined(_MSC_VER) && _MSC_VER < 1900
		#define snprintf _snprintf
	#endif
	#define popen _popen
#else
	#include <time.h>
	#include <unistd.h>
#endif


double get_time()
{
#ifdef _WIN32
	SYSTEMTIME st;
	GetSystemTime(&st);
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	ULARGE_INTEGER li;
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	// FILETIME is given as a 64-bit value for the number of 100-nanosecond
	// intervals that have passed since Jan 1, 1601 (UTC).  The difference between that
	// epoch and the POSIX epoch (Jan 1, 1970) is 116444736000000000 such ticks.
	uint64_t total_usecs = (li.QuadPart - 116444736000000000L) / 10L;
	return (total_usecs / 1000000.);
#else
	struct timeval cur;
	gettimeofday(&cur, NULL);
	return cur.tv_sec + cur.tv_usec / 1000000.;
#endif
}

void sleep_highres(double tm)
{
#ifdef _WIN32
	int msec = floor(tm * 1000);
	if (msec > 0) {
		Sleep(msec);
	}
#else
	int sec = floor(tm);
	int usec = (tm - sec) * 1000000;
	if (tm > 0) {
		sleep(sec);
		usleep(usec);
	}
#endif
}
