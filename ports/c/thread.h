#pragma once

/*
  A thread, on win/mac/linux.
  ===========================

  Restored from include/thread.h, which commit 441b58c ("Modernize to C++17
  and CMake") deleted -- correctly, for C++.  Once <thread> was portable, a
  hand-rolled shim over CreateThread and pthread_create was dead weight.

  C did not get that reprieve.  C11 added <threads.h>, but it is optional
  (__STDC_NO_THREADS__) and macOS does not ship it at all, so a C test that
  runs on all three platforms still needs this.  The original's comment
  still applies: this makes no use of retain, it is merely how the test
  gets more than one thread.

  Function pointer plus argument, where the C++ original had a virtual
  run().  Naming follows ../oopc: Thread_verb(me, ...).
*/

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct ThreadStruct Thread;

struct ThreadStruct {
#ifdef _WIN32
    HANDLE handle;
#else
    pthread_t handle;
#endif
    void (*run)(void *argument);
    void *argument;
};

/* Run `run(argument)` on a new thread.  Every started thread must be joined. */
void Thread_start(Thread *me, void (*run)(void *argument), void *argument);

/* Wait for the thread to finish and release the handle. */
void Thread_join(Thread *me);

/* Sleep the calling thread.  Sleep() on Win32, nanosleep() elsewhere --
   the test needs it, and it is the same portability question. */
void Thread_sleep(unsigned milliseconds);
