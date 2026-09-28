#ifndef TYMALLOC_H
#define TYMALLOC_H

#include <stddef.h>

void* ty_malloc(size_t size);
void  ty_free(void* p);

#endif