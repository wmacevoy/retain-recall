#include <iostream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <string>

extern "C" {
#include "sort.h"
}

#include "retain.hpp"

//
// setup
//

class Sortable
{
public:
  virtual int size() const=0;
  virtual int compare(int i,int j) const=0;
  virtual void swap(int i,int j)=0;
  virtual ~Sortable() {};
};

extern "C" int compare(int i,int j)
{
  recall<Sortable>()->compare(i,j); // ***
}

extern "C" void swap(int i, int j)
{
  recall<Sortable>()->swap(i,j); // ***
}

template <typename T>
class SortableVectorDecorator : public Sortable
{
public:
  std::vector < T > &v;

  int size() const 
  { 
    return (int) v.size(); 
  }

  int compare(int i,int j) const 
  {
    if (v[i]<v[j]) return -1;
    if (v[j]<v[i]) return  1;
    return 0;
  }

  void swap(int i,int j) 
  { 
    std::swap(v[i],v[j]); 
  }

  SortableVectorDecorator(std::vector < T > &_v) : v(_v) {};
};

//
// use
//

void sort(Sortable &s)
{
  // use old procedural sort code in new polymorphic way
  retain<Sortable> as(&s); // ***
  sort(0,s.size());
}

template <typename T>
void sort(std::vector <T> &v)
{
  SortableVectorDecorator<T> sortable(v);
  sort(sortable);
}

int main()
{
  std::vector<double> v1;

  v1.push_back(1.1);
  v1.push_back(-2.2);
  v1.push_back(3.0);

  std::vector<std::string> v2;

  v2.push_back("caterwall");
  v2.push_back("cat");
  v2.push_back("caterpillar");

  sort(v1);

  for (size_t i=1; i<v1.size(); ++i) assert(v1[i-1]<=v1[i]);

  sort(v2);

  for (size_t i=1; i<v2.size(); ++i) assert(v2[i-1]<=v2[i]);
  
  return 0;
}
