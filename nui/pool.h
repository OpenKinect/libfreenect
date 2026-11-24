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

/* A small thread pool for data-parallel loops (Win32 threads or pthreads). */
typedef struct fn_pool fn_pool;

/* The work: items [begin, end). */
typedef void (*fn_pool_fn)(void* ctx, int begin, int end);

/* A pool with `threads` threads in all, the caller included (1: none
 * started; everything runs in the caller). NULL when out of resources. */
fn_pool* fn_pool_create(int threads);
void fn_pool_destroy(fn_pool* p);
int fn_pool_threads(const fn_pool* p);

/* Runs fn over items [0, n) in chunks of `chunk`, on the pool's threads and
 * the caller, and returns when all are done. Not reentrant. */
void fn_pool_run(fn_pool* p, fn_pool_fn fn, void* ctx, int n, int chunk);

/* The number of processors (at least 1). */
int fn_pool_cpus(void);
