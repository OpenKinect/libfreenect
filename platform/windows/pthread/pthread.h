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
 * Minimal, header-only POSIX threads subset for Windows (Vista and later),
 * built on native SRW locks, condition variables and _beginthreadex.
 *
 * It covers exactly what the libfreenect wrappers and examples use, so that
 * they build with MSVC without installing pthreads-win32. CMake only puts this
 * directory on the include path when no real pthreads-win32 is found
 * (see cmake_modules/FindThreads.cmake).
 *
 * Semantics: mutexes are non-recursive (PTHREAD_MUTEX_DEFAULT); attributes
 * other than NULL are ignored; there is no cancellation.
 */

#pragma once

#ifndef _WIN32
#error "platform/windows/pthread/pthread.h is only for Windows builds"
#endif

#include <windows.h>
#include <process.h>
#include <stdlib.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER) && !defined(__cplusplus)
#define FN_PTHREAD_INLINE static __inline
#else
#define FN_PTHREAD_INLINE static inline
#endif

/* ---- types ------------------------------------------------------------- */

struct fn_pthread {
	HANDLE handle;
	void *(*start)(void *);
	void *arg;
	void *result;
};

typedef struct fn_pthread *pthread_t;
typedef struct fn_pthread_attr pthread_attr_t;          /* only NULL is supported */
typedef struct fn_pthread_mutexattr pthread_mutexattr_t; /* only NULL is supported */
typedef struct fn_pthread_condattr pthread_condattr_t;   /* only NULL is supported */

typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
#define PTHREAD_COND_INITIALIZER CONDITION_VARIABLE_INIT

/* ---- threads ----------------------------------------------------------- */

FN_PTHREAD_INLINE unsigned __stdcall fn_pthread_trampoline(void *p)
{
	struct fn_pthread *t = (struct fn_pthread *)p;
	t->result = t->start(t->arg);
	return 0;
}

FN_PTHREAD_INLINE int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                                     void *(*start)(void *), void *arg)
{
	struct fn_pthread *t;
	uintptr_t h;
	(void)attr;
	t = (struct fn_pthread *)calloc(1, sizeof(*t));
	if (!t)
		return ENOMEM;
	t->start = start;
	t->arg = arg;
	/* Create suspended so t->handle is set before the thread can run. */
	h = _beginthreadex(NULL, 0, fn_pthread_trampoline, t, CREATE_SUSPENDED, NULL);
	if (h == 0) {
		free(t);
		return EAGAIN;
	}
	t->handle = (HANDLE)h;
	*thread = t;
	ResumeThread(t->handle);
	return 0;
}

FN_PTHREAD_INLINE int pthread_join(pthread_t thread, void **result)
{
	if (!thread)
		return EINVAL;
	if (WaitForSingleObject(thread->handle, INFINITE) != WAIT_OBJECT_0)
		return EINVAL;
	if (result)
		*result = thread->result;
	CloseHandle(thread->handle);
	free(thread);
	return 0;
}

/* ---- mutexes ----------------------------------------------------------- */

FN_PTHREAD_INLINE int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr)
{
	(void)attr;
	InitializeSRWLock(m);
	return 0;
}

FN_PTHREAD_INLINE int pthread_mutex_destroy(pthread_mutex_t *m)
{
	(void)m; /* SRW locks need no cleanup */
	return 0;
}

FN_PTHREAD_INLINE int pthread_mutex_lock(pthread_mutex_t *m)
{
	AcquireSRWLockExclusive(m);
	return 0;
}

FN_PTHREAD_INLINE int pthread_mutex_trylock(pthread_mutex_t *m)
{
	return TryAcquireSRWLockExclusive(m) ? 0 : EBUSY;
}

FN_PTHREAD_INLINE int pthread_mutex_unlock(pthread_mutex_t *m)
{
	ReleaseSRWLockExclusive(m);
	return 0;
}

/* ---- condition variables ----------------------------------------------- */

FN_PTHREAD_INLINE int pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *attr)
{
	(void)attr;
	InitializeConditionVariable(c);
	return 0;
}

FN_PTHREAD_INLINE int pthread_cond_destroy(pthread_cond_t *c)
{
	(void)c; /* condition variables need no cleanup */
	return 0;
}

FN_PTHREAD_INLINE int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
	return SleepConditionVariableSRW(c, m, INFINITE, 0) ? 0 : EINVAL;
}

FN_PTHREAD_INLINE int pthread_cond_signal(pthread_cond_t *c)
{
	WakeConditionVariable(c);
	return 0;
}

FN_PTHREAD_INLINE int pthread_cond_broadcast(pthread_cond_t *c)
{
	WakeAllConditionVariable(c);
	return 0;
}

#ifdef __cplusplus
}
#endif
