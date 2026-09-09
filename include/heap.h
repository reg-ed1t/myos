#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

#define KERNEL_HEAP_START 0x01000000
#define KERNEL_HEAP_END   0x40000000

void heap_init(void);

void* kmalloc(uint32_t size);
void kfree(void* ptr);

#endif
