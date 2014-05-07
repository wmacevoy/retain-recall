#include "sort.h"


void sort(size_t i0, size_t i1)
{
  if (i1 > i0) {
    size_t i,j;
    --i1;
    i=i0;
    j=i0+(i1-i0)/2;
    swap(j,i1);
    j=i0;
    while (i<i1) {
      if (compare(i,i1) <= 0) {
	swap(i,j);
	++j;
      }
      ++i;
    }
    swap(j,i1);
    sort(i0,j);
    sort(j+1,i1+1);
  }
}
