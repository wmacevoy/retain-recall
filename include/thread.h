#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef _WIN32

class Thread
{
 public: HANDLE hThread;
 public: Thread();
 public: void start();
 public: void join();
 public: virtual void run();
};

#else

class Thread
{
 public: pthread_t hThread;
 public: Thread();
 public: void start();
 public: void join();
 public: virtual void run();
};

#endif

