#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include "vmm.h"

void heap_init(void);

void* kmalloc(uint32_t size);
void kfree(void* ptr);

#endif
