#include <assert.h>
#include <iostream>
#include <string>
#include <sstream>
#include <math.h>
#include <stdlib.h>

#include "thread.h"

// #define RETAIN_SINGLE_THREADED

#include "retain.hpp"

using namespace std;

int x[100];


class Test : public Thread
{
public:
  int id;
  ostringstream out;

  Test(int _id) : id(_id) {}

  void state()
  {
    out << id << ": ";
    for (retain<int>::iterator i = retain<int>::begin(); i!=retain<int>::end(); ++i) {
      int *p=&(*i);
      if (p != 0) {
	out << "[" << p-x << "]";
      } else {
	out << "[null]";
      }
    }
    if (retained<int>()) { 
      if (recall<int>() != 0) {
	out << "@" << (recall<int>()-x);
      } else {
	out << "@null";
      }
    }
    out << endl;
#ifdef _WIN32
	Sleep(rand()%10);
#else
    usleep(1000*(rand()%10));
#endif
  }
  
  void run()
  { state();
    { retain<int> as(&x[id+0]); state();
      { retain<int> as(&x[id+1]); state(); }
      { retain<int> as_if(&x[id+2],true); state(); 
	{ retain<int> as(&x[id+3]); state(); }
	{ retain<int> as(&x[id+4]); state(); }
	state();
      }
      { retain<int> as_if(&x[id+5],false); state(); 
	{ retain<int> as(&x[id+6]); state(); }
	{ retain<int> as(&x[id+7]); state(); }
	state();
      }
    }
  }
};
  
string test()
{
  Test* tests[10];
  int n=10;

  for (int k=0; k<n; ++k) {
    tests[k]=new Test(10*k);
  }
  
#ifdef RETAIN_SINGLE_THREADED
  for (int k=0; k<n; ++k) {
    tests[k]->run();
  }
#else
  for (int k=1; k<n; ++k) {
    tests[k]->start();
  }
  tests[0]->run();
  for (int k=1; k<n; ++k) {
    if (k > 0) tests[k]->join();
  }
#endif

  string ans;
  for (int k=0; k<n; ++k) {
    ans += tests[k]->out.str();
  }

  for (int k=0; k<n; ++k) {
    delete tests[k];
  }

  return ans;
}

int main()
{

#ifdef RETAIN_SINGLE_THREADED
  cout << "single threaded" << endl;
#else
#ifdef _WIN32
  cout << "win32 threads" << endl;
#else
  cout << "posix threads" << endl;
#endif
#endif

  string ans=test();
  cout << ans << endl;

  for (int i=0; i<100; ++i) {
    assert(test() == ans);
  }
}

