/* nanosleep is POSIX, and -std=c11 alone hides it behind the feature test. */
#define _POSIX_C_SOURCE 199309L

#include "thread.h"

#include <assert.h>
#include <stddef.h>

#ifdef _WIN32

static DWORD WINAPI Thread_trampoline(LPVOID argument)
{
    Thread *me = (Thread *)argument;
    me->run(me->argument);
    return 0;
}

void Thread_start(Thread *me, void (*run)(void *argument), void *argument)
{
    me->run = run;
    me->argument = argument;
    me->handle = CreateThread(NULL, 0, Thread_trampoline, me, 0, NULL);
    assert(me->handle != NULL && "CreateThread failed");
}

void Thread_join(Thread *me)
{
    WaitForSingleObject(me->handle, INFINITE);
    /* The 2014 original never closed this; a handle per thread leaked. */
    CloseHandle(me->handle);
    me->handle = NULL;
}

void Thread_sleep(unsigned milliseconds)
{
    Sleep(milliseconds);
}

#else

#include <time.h>

static void *Thread_trampoline(void *argument)
{
    Thread *me = (Thread *)argument;
    me->run(me->argument);
    return NULL;
}

void Thread_start(Thread *me, void (*run)(void *argument), void *argument)
{
    int failed;
    me->run = run;
    me->argument = argument;
    failed = pthread_create(&me->handle, NULL, Thread_trampoline, me);
    assert(failed == 0 && "pthread_create failed");
    (void)failed;
}

void Thread_join(Thread *me)
{
    pthread_join(me->handle, NULL);
}

void Thread_sleep(unsigned milliseconds)
{
    struct timespec request;
    request.tv_sec = (time_t)(milliseconds / 1000u);
    request.tv_nsec = (long)(milliseconds % 1000u) * 1000000L;
    nanosleep(&request, NULL);
}

#endif
