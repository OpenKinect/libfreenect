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

#include <stdlib.h>
#include <string.h>
#include "pool.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef SRWLOCK fn_mutex;
typedef CONDITION_VARIABLE fn_cond;
typedef HANDLE fn_thread;
#define mutex_init(m) InitializeSRWLock(m)
#define mutex_lock(m) AcquireSRWLockExclusive(m)
#define mutex_unlock(m) ReleaseSRWLockExclusive(m)
#define mutex_free(m) ((void)0)
#define cond_init(c) InitializeConditionVariable(c)
#define cond_wait(c, m) SleepConditionVariableSRW(c, m, INFINITE, 0)
#define cond_broadcast(c) WakeAllConditionVariable(c)
#define cond_free(c) ((void)0)
#else
#include <pthread.h>
#include <unistd.h>
typedef pthread_mutex_t fn_mutex;
typedef pthread_cond_t fn_cond;
typedef pthread_t fn_thread;
#define mutex_init(m) pthread_mutex_init(m, NULL)
#define mutex_lock(m) pthread_mutex_lock(m)
#define mutex_unlock(m) pthread_mutex_unlock(m)
#define mutex_free(m) pthread_mutex_destroy(m)
#define cond_init(c) pthread_cond_init(c, NULL)
#define cond_wait(c, m) pthread_cond_wait(c, m)
#define cond_broadcast(c) pthread_cond_broadcast(c)
#define cond_free(c) pthread_cond_destroy(c)
#endif

#define MAX_THREADS 64

struct fn_pool {
	int threads, started;
	fn_thread tid[MAX_THREADS];
	fn_mutex lock;
	fn_cond work, done;
	unsigned generation; // a new job
	int quit;
	// the job
	fn_pool_fn fn;
	void* ctx;
	int n, chunk, next, busy;
};

// Takes chunks of the current job until none is left.
static void work(fn_pool* p)
{
	for (;;) {
		int b;
		mutex_lock(&p->lock);
		b = p->next;
		if (b >= p->n) {
			mutex_unlock(&p->lock);
			return;
		}
		p->next = b + p->chunk;
		p->busy++;
		mutex_unlock(&p->lock);
		p->fn(p->ctx, b, b + p->chunk < p->n ? b + p->chunk : p->n);
		mutex_lock(&p->lock);
		if (--p->busy == 0 && p->next >= p->n)
			cond_broadcast(&p->done);
		mutex_unlock(&p->lock);
	}
}

#ifdef _WIN32
static DWORD WINAPI worker(LPVOID arg)
#else
static void* worker(void* arg)
#endif
{
	fn_pool* p = arg;
	unsigned seen = 0;
	for (;;) {
		mutex_lock(&p->lock);
		while (p->generation == seen && !p->quit)
			cond_wait(&p->work, &p->lock);
		if (p->quit) {
			mutex_unlock(&p->lock);
			break;
		}
		seen = p->generation;
		mutex_unlock(&p->lock);
		work(p);
	}
	return 0;
}

int fn_pool_cpus(void)
{
#ifdef _WIN32
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
#else
	long n = sysconf(_SC_NPROCESSORS_ONLN);
	return n > 0 ? (int)n : 1;
#endif
}

fn_pool* fn_pool_create(int threads)
{
	fn_pool* p = calloc(1, sizeof(*p));
	int i;
	if (!p)
		return NULL;
	if (threads < 1)
		threads = 1;
	if (threads > MAX_THREADS)
		threads = MAX_THREADS;
	p->threads = threads;
	mutex_init(&p->lock);
	cond_init(&p->work);
	cond_init(&p->done);
	for (i = 0; i < threads - 1; i++) {
#ifdef _WIN32
		p->tid[i] = CreateThread(NULL, 0, worker, p, 0, NULL);
		if (!p->tid[i])
			break;
#else
		if (pthread_create(&p->tid[i], NULL, worker, p) != 0)
			break;
#endif
		p->started++;
	}
	p->threads = p->started + 1;
	return p;
}

void fn_pool_destroy(fn_pool* p)
{
	int i;
	if (!p)
		return;
	mutex_lock(&p->lock);
	p->quit = 1;
	cond_broadcast(&p->work);
	mutex_unlock(&p->lock);
	for (i = 0; i < p->started; i++) {
#ifdef _WIN32
		WaitForSingleObject(p->tid[i], INFINITE);
		CloseHandle(p->tid[i]);
#else
		pthread_join(p->tid[i], NULL);
#endif
	}
	cond_free(&p->work);
	cond_free(&p->done);
	mutex_free(&p->lock);
	free(p);
}

int fn_pool_threads(const fn_pool* p)
{
	return p ? p->threads : 1;
}

void fn_pool_run(fn_pool* p, fn_pool_fn fn, void* ctx, int n, int chunk)
{
	if (n <= 0)
		return;
	if (chunk < 1)
		chunk = 1;
	if (!p || p->started == 0 || n <= chunk) {
		fn(ctx, 0, n);
		return;
	}
	mutex_lock(&p->lock);
	p->fn = fn;
	p->ctx = ctx;
	p->n = n;
	p->chunk = chunk;
	p->next = 0;
	p->busy = 0;
	p->generation++;
	cond_broadcast(&p->work);
	mutex_unlock(&p->lock);
	work(p); // the caller helps
	mutex_lock(&p->lock);
	while (p->busy > 0 || p->next < p->n)
		cond_wait(&p->done, &p->lock);
	mutex_unlock(&p->lock);
}
