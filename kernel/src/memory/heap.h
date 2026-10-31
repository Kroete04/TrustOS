
#ifndef TRUSTOS_HEAP_H
#define TRUSTOS_HEAP_H

#include <stddef.h>
#include <stdbool.h>

bool heap_init(void);

void *kmalloc(size_t size);

void kfree(void *ptr);

#endif
