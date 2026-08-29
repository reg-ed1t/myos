#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PMM_BLOCK_SIZE 4096

void pmm_init(uint32_t mem_size, uint32_t bitmap_start_addr);
void pmm_init_region(uint32_t base_addr, uint32_t size);
void pmm_deinit_region(uint32_t base_addr, uint32_t size);

void* pmm_alloc_block(void);
void pmm_free_block(void* p);

#endif