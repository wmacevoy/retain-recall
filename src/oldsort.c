#include <stdio.h>
#include "sort.h"

double a[] = { 1.1, 1.3, -1.0, -2.0, 3.0 };

int compare(size_t i,size_t j)
{
  if (a[i] < a[j]) return -1;
  if (a[j] < a[i]) return  1;
  return 0;
}

void swap(size_t i, size_t j)
{
  double tmp=a[i];
  a[i]=a[j];
  a[j]=tmp;
}

int main()
{
  size_t i;

  sort(0,5);

  for (i=0; i<5; ++i) {
    printf("a[%zu]=%lg\n",i,a[i]);
  }

  return 0;
}
