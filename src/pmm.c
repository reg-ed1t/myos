#include "pmm.h"
#include "vga.h"

// Pointer to the start of the bitmap array in memory
uint32_t* pmm_bitmap = 0;
uint32_t  pmm_max_blocks = 0;
uint32_t  pmm_bitmap_size = 0;

// Inline helpers to handle bit configurations inside the array
static inline void pmm_bitmap_set(uint32_t bit) {
    pmm_bitmap[bit / 32] |= (1 << (bit % 32));
}

static inline void pmm_bitmap_unset(uint32_t bit) {
    pmm_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

static inline uint8_t pmm_bitmap_test(uint32_t bit) {
    return (pmm_bitmap[bit / 32] & (1 << (bit % 32))) != 0;
}

// Find the first free bit (0) inside the tracking array
int pmm_bitmap_first_free() {
    for (uint32_t i = 0; i < ((pmm_max_blocks + 31) / 32); i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) { // If any bit in this 32-bit chunk is 0
            for (int j = 0; j < 32; j++) {
                if (!(pmm_bitmap[i] & (1 << j))) {
                    return (i * 32) + j;
                }
            }
        }
    }
    return -1; // Out of physical memory!
}

void pmm_init(uint32_t mem_size, uint32_t bitmap_start_addr) {
    pmm_max_blocks = mem_size / PMM_BLOCK_SIZE;
    pmm_bitmap = (uint32_t*)bitmap_start_addr;
    pmm_bitmap_size = (pmm_max_blocks + 31) / 32;

    // By default, mark all memory regions as reserved/used (all bits set to 1)
    // We do this for safety so we don't accidentally allocate non-existent RAM
    for (uint32_t i = 0; i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0xFFFFFFFF;
    }
}

// Mark a specific memory region as available for use (unset bits to 0)
void pmm_init_region(uint32_t base_addr, uint32_t size) {
    uint32_t align_block = base_addr / PMM_BLOCK_SIZE;
    uint32_t num_blocks = size / PMM_BLOCK_SIZE;

    if (align_block >= pmm_max_blocks) {
        return;
    }

    if (num_blocks > pmm_max_blocks - align_block) {
        num_blocks = pmm_max_blocks - align_block;
    }

    for (uint32_t i = 0; i < num_blocks; i++) {
        pmm_bitmap_unset(align_block + i);
    }
}

// Force lock a region so it cannot be given away (set bits to 1)
void pmm_deinit_region(uint32_t base_addr, uint32_t size) {
    uint32_t align_block = base_addr / PMM_BLOCK_SIZE;
    uint32_t num_blocks = size / PMM_BLOCK_SIZE;
	
	if (align_block >= pmm_max_blocks) {
        return;
    }

    if (num_blocks > pmm_max_blocks - align_block) {
        num_blocks = pmm_max_blocks - align_block;
    }

    for (uint32_t i = 0; i < num_blocks; i++) {
        pmm_bitmap_set(align_block + i);
    }
}

// Allocate one single 4KB block of raw RAM
void* pmm_alloc_block() {
    int free_bit = pmm_bitmap_first_free();
    if (free_bit == -1) {
        return 0; // Return NULL pointer
    }

    pmm_bitmap_set(free_bit);
    uint32_t physical_address = free_bit * PMM_BLOCK_SIZE;
    return (void*)physical_address;
}

// Free an allocated block back into the pool
void pmm_free_block(void* p) {
    uint32_t addr = (uint32_t)p;
    uint32_t block = addr / PMM_BLOCK_SIZE;

    if (block >= pmm_max_blocks) {
        kprint("PMM: invalid block address\n");
        return;
    }

    if (!pmm_bitmap_test(block)) {
        kprint("PMM: double free detected\n");
        return;
    }

    pmm_bitmap_unset(block);
}