/*
* This file is part of the OpenKinect Project. http://www.openkinect.org
*
* Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
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

#ifndef LIBUSBEMU_THREAD_INTERFACE_WRAPPER_FOR_WIN32_THREADS_H
#define LIBUSBEMU_THREAD_INTERFACE_WRAPPER_FOR_WIN32_THREADS_H

#include <windows.h>
#include <stdio.h>

namespace libusbemu {

struct QuickThread
{
private:
  HANDLE hThread;

  // Type-safe wrapper to convert arbitrary function signatures into the
  // required signature of Win32 Thread Procedures (LPTHREAD_START_ROUTINE).
  // This wrapper can be extended in the future to support member-functions
  // to run as thread procedures.
  template<typename F>
  struct ThreadWrapper
  {
    struct State
    {
      F* threadproc;
      void* threadparams;
    };
    static DWORD WINAPI Thunk(/*__in*/ LPVOID lpParameter)
    {
      State* state ((State*)lpParameter);
      state->threadproc(state->threadparams);
      delete(state);
      return(0);
    }
  };

public:
  template<typename F>
  inline QuickThread(F* proc, void* params) : hThread(NULL)
  {
    fprintf(stdout, "Thread created.\n");
    // the 'typename' is required here because of dependent names...
    // VC++ relaxes this constraint, but it goes against the standard.
    // 'State' is allocated here, but will be released once the thread
    // terminates within the 'thunk' function that wraps the thread.
    typename ThreadWrapper<F>::State* state = new typename ThreadWrapper<F>::State;
    state->threadproc   = proc;
    state->threadparams = params;
    hThread = CreateThread(NULL, 0, &ThreadWrapper<F>::Thunk, (LPVOID)state, 0, NULL);
  }

  inline QuickThread(LPTHREAD_START_ROUTINE proc, void* params)  : hThread(NULL)
  {
    fprintf(stdout, "Thread created.\n");
    hThread = CreateThread(NULL, 0, proc, params, 0, NULL);
  }

  inline ~QuickThread()
  {
    CloseHandle(hThread);
    fprintf(stdout, "Thread resources released.\n");
  }

  inline void Join()
  {
    WaitForSingleObject(hThread, INFINITE);
  }

  inline bool TryJoin()
  {
    return(WAIT_OBJECT_0 == WaitForSingleObject(hThread, 0));
  }

  inline bool LowerPriority()
  {
    return(TRUE == SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL));
  };

  inline bool RaisePriority()
  {
    return(TRUE == SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL));
  };
};



struct QuickMutex
{
private:
  CRITICAL_SECTION cs;

public:
  inline QuickMutex()
  {
    InitializeCriticalSection(&cs);
  }

  inline ~QuickMutex()
  {
    DeleteCriticalSection(&cs);
  }

  inline void Enter()
  {
    EnterCriticalSection(&cs);
  }

  inline void Leave()
  {
    LeaveCriticalSection(&cs);
  }
};



struct QuickEvent
{
private:
  HANDLE hEvent;

public:
  inline QuickEvent() : hEvent(NULL)
  {
    hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  }
  inline ~QuickEvent()
  {
    CloseHandle(hEvent);
  }
  inline void Signal()
  {
    SetEvent(hEvent);
  }
  inline void Reset()
  {
    ResetEvent(hEvent);
  }
  inline bool Check()
  {
    return(WAIT_OBJECT_0 == WaitForSingleObject(hEvent, 0));
  }
  inline void Wait()
  {
    WaitForSingleObject(hEvent, INFINITE);
  }
};

} //end of 'namespace libusbemu'

#endif//LIBUSBEMU_THREAD_INTERFACE_WRAPPER_FOR_WIN32_THREADS_H
