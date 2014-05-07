#pragma once

#include <stddef.h>

/* sort elements in the range (i0,i1] 
   using compare(size_t i,size_t j) and swap(size_t i,size_t j) */

void sort(size_t i0, size_t i1);

// user defined functions
int compare(size_t i0, size_t i1);
void swap(size_t i0, size_t i1);
