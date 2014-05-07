#include "thread.h"

//
// thread in itself makes no use of retain, it is merely a class to test retain
// in a multi-threaded environment.
//

#ifdef _WIN32
static DWORD WINAPI ThreadRun(LPVOID arg)
{
  ((Thread*)arg)->run();
  return 0;
}

Thread::Thread()
{
  hThread = 0;
}

void Thread::start()
{
  hThread = CreateThread(NULL,NULL,ThreadRun,this,NULL,NULL);
}

void Thread::run()
{

}

void Thread::join()
{
  WaitForSingleObject(hThread,INFINITE);
}

#else
static void *ThreadRun(void *arg)
{
  ((Thread*)arg)->run();
  return 0;
}

Thread::Thread()
{
  hThread = 0;
}

void Thread::start()
{
  pthread_create( &hThread, NULL, ThreadRun, this);
}

void Thread::run()
{

}

void Thread::join()
{
  pthread_join(hThread,NULL);
}

#endif
