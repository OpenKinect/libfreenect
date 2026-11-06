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
 * Clean shutdown for the console examples.
 *
 *   fn_install_exit_handlers();          at the start of main()
 *   while (!fn_exit_requested) ...;      main loop; poll with a short timeout
 *   ...stop streams, close the device, freenect_shutdown()...
 *   fn_exit_done();                      as the last step before returning
 *
 * POSIX: SIGINT, SIGTERM, SIGHUP (terminal closed) and SIGQUIT request exit.
 * Windows: Ctrl+C, Ctrl+Break, closing the console window, logoff and
 * shutdown request exit. For window close, logoff and shutdown, Windows ends
 * the process as soon as the handler returns, so the handler waits (at most
 * FN_EXIT_GRACE_MS) until main() has called fn_exit_done().
 */

#pragma once

#include <signal.h>

#define FN_EXIT_GRACE_MS 4000

#if defined(_MSC_VER) && !defined(__cplusplus)
#define FN_EXIT_INLINE static __inline
#else
#define FN_EXIT_INLINE static inline
#endif

static volatile sig_atomic_t fn_exit_requested = 0;

#ifdef _WIN32
#include <windows.h>

static HANDLE fn_exit_done_event = NULL;

FN_EXIT_INLINE BOOL WINAPI fn_console_handler(DWORD type)
{
	switch (type) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		fn_exit_requested = 1;
		return TRUE;
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		fn_exit_requested = 1;
		if (fn_exit_done_event)
			WaitForSingleObject(fn_exit_done_event, FN_EXIT_GRACE_MS);
		return TRUE;
	default:
		return FALSE;
	}
}

FN_EXIT_INLINE void fn_install_exit_handlers(void)
{
	fn_exit_done_event = CreateEventW(NULL, TRUE, FALSE, NULL);
	SetConsoleCtrlHandler(fn_console_handler, TRUE);
}

FN_EXIT_INLINE void fn_exit_done(void)
{
	if (fn_exit_done_event)
		SetEvent(fn_exit_done_event);
}

/* Sleep for ms milliseconds, returning early once exit has been requested. */
FN_EXIT_INLINE void fn_sleep_ms(unsigned ms)
{
	while (ms > 0 && !fn_exit_requested) {
		unsigned step = ms < 100 ? ms : 100;
		Sleep(step);
		ms -= step;
	}
}

#else
#include <unistd.h>

FN_EXIT_INLINE void fn_sleep_ms(unsigned ms)
{
	while (ms > 0 && !fn_exit_requested) {
		unsigned step = ms < 100 ? ms : 100;
		usleep(step * 1000);
		ms -= step;
	}
}

FN_EXIT_INLINE void fn_signal_handler(int sig)
{
	(void)sig;
	fn_exit_requested = 1;
}

FN_EXIT_INLINE void fn_install_exit_handlers(void)
{
	signal(SIGINT, fn_signal_handler);
	signal(SIGTERM, fn_signal_handler);
#ifdef SIGHUP
	signal(SIGHUP, fn_signal_handler);
#endif
#ifdef SIGQUIT
	signal(SIGQUIT, fn_signal_handler);
#endif
}

FN_EXIT_INLINE void fn_exit_done(void)
{
}

#endif
