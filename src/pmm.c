#include "pmm.h"
#include "vga.h"

uint32_t* pmm_bitmap = 0;
uint32_t pmm_max_blocks = 0;
uint32_t pmm_bitmap_size = 0;

static inline void pmm_bitmap_set(uint32_t bit)
{
    pmm_bitmap[bit / 32] |= (1U << (bit % 32));
}

static inline void pmm_bitmap_unset(uint32_t bit)
{
    pmm_bitmap[bit / 32] &= ~(1U << (bit % 32));
}

static inline uint8_t pmm_bitmap_test(uint32_t bit)
{
    return (pmm_bitmap[bit / 32] & (1U << (bit % 32))) != 0;
}

static int pmm_bitmap_first_free(void)
{
    for (uint32_t i = 0; i < pmm_bitmap_size; i++) {

        if (pmm_bitmap[i] != 0xFFFFFFFFU) {

            for (uint32_t j = 0; j < 32; j++) {

                uint32_t bit = (i * 32) + j;

                if (bit >= pmm_max_blocks) {
                    return -1;
                }

                if (!(pmm_bitmap[i] & (1U << j))) {
                    return (int)bit;
                }
            }
        }
    }

    return -1;
}

void pmm_init(uint32_t mem_size, uint32_t bitmap_start_addr)
{
    pmm_max_blocks = mem_size / PMM_BLOCK_SIZE;
    pmm_bitmap = (uint32_t*)bitmap_start_addr;
    pmm_bitmap_size = (pmm_max_blocks + 31) / 32;

    for (uint32_t i = 0; i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0xFFFFFFFFU;
    }
}

void pmm_init_region(uint32_t base_addr, uint32_t size)
{
    uint32_t end_addr;

    if (size == 0) {
        return;
    }

    if (base_addr >= pmm_max_blocks * PMM_BLOCK_SIZE) {
        return;
    }

    if (size > 0xFFFFFFFFU - base_addr) {
        end_addr = 0xFFFFFFFFU;
    } else {
        end_addr = base_addr + size;
    }

    base_addr = (base_addr + PMM_BLOCK_SIZE - 1) & ~(PMM_BLOCK_SIZE - 1);
    end_addr &= ~(PMM_BLOCK_SIZE - 1);

    if (end_addr <= base_addr) {
        return;
    }

    uint32_t first_block = base_addr / PMM_BLOCK_SIZE;
    uint32_t last_block = end_addr / PMM_BLOCK_SIZE;

    if (last_block > pmm_max_blocks) {
        last_block = pmm_max_blocks;
    }

    for (uint32_t block = first_block; block < last_block; block++) {
        pmm_bitmap_unset(block);
    }
}

void pmm_deinit_region(uint32_t base_addr, uint32_t size)
{
    if (size == 0) {
        return;
    }

    if (base_addr >= pmm_max_blocks * PMM_BLOCK_SIZE) {
        return;
    }

    uint32_t end_addr;

    if (size > 0xFFFFFFFFU - base_addr) {
        end_addr = 0xFFFFFFFFU;
    } else {
        end_addr = base_addr + size;
    }

    base_addr &= ~(PMM_BLOCK_SIZE - 1);
    end_addr = (end_addr + PMM_BLOCK_SIZE - 1) & ~(PMM_BLOCK_SIZE - 1);

    uint32_t first_block = base_addr / PMM_BLOCK_SIZE;
    uint32_t last_block = end_addr / PMM_BLOCK_SIZE;

    if (last_block > pmm_max_blocks) {
        last_block = pmm_max_blocks;
    }

    for (uint32_t block = first_block; block < last_block; block++) {
        pmm_bitmap_set(block);
    }
}

static uint32_t multiboot_get_max_memory(const multiboot_info_t* mbi)
{
    uint32_t max_addr = 0;
    uint32_t current = mbi->mmap_addr;
    uint32_t end = mbi->mmap_addr + mbi->mmap_length;

    while (current < end) {

        multiboot_mmap_entry_t* entry =
                (multiboot_mmap_entry_t*)current;

        if (entry->addr_high == 0 && entry->len_high == 0) {

            uint32_t region_end;

            if (entry->len_low >
                0xFFFFFFFFU - entry->addr_low) {

                region_end = 0xFFFFFFFFU;

            } else {

                region_end =
                        entry->addr_low + entry->len_low;
            }

            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE &&
                region_end > max_addr) {

                max_addr = region_end;
            }
        }

        current += entry->size + sizeof(entry->size);
    }

    max_addr &= ~(PMM_BLOCK_SIZE - 1);

    return max_addr;
}

int pmm_init_multiboot(
        const multiboot_info_t* mbi,
        uint32_t kernel_start,
        uint32_t kernel_end
)
{
    if (!mbi) {
        kprint("PMM: invalid Multiboot info\n");
        return 0;
    }

    if (!(mbi->flags & MULTIBOOT_INFO_MEMORY_MAP)) {
        kprint("PMM: Multiboot memory map unavailable\n");
        return 0;
    }

    if (mbi->mmap_length == 0 || mbi->mmap_addr == 0) {
        kprint("PMM: invalid memory map\n");
        return 0;
    }

    uint32_t max_memory = multiboot_get_max_memory(mbi);

    if (max_memory < PMM_BLOCK_SIZE) {
        kprint("PMM: no usable memory found\n");
        return 0;
    }

    uint32_t bitmap_start = 0x400000;

    uint32_t bitmap_end =
            bitmap_start +
            ((max_memory / PMM_BLOCK_SIZE + 31) / 32) * sizeof(uint32_t);

    if (bitmap_end < bitmap_start) {
        kprint("PMM: bitmap address overflow\n");
        return 0;
    }

    pmm_init(max_memory, bitmap_start);

    uint32_t current = mbi->mmap_addr;
    uint32_t end = mbi->mmap_addr + mbi->mmap_length;

    while (current < end) {

        multiboot_mmap_entry_t* entry =
                (multiboot_mmap_entry_t*)current;

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE &&
            entry->addr_high == 0 &&
            entry->len_high == 0) {

            pmm_init_region(
                    entry->addr_low,
                    entry->len_low
            );
        }

        current += entry->size + sizeof(entry->size);
    }

    pmm_deinit_region(0, 0x100000);

    if (kernel_end > kernel_start) {
        pmm_deinit_region(
                kernel_start,
                kernel_end - kernel_start
        );
    }

    pmm_deinit_region(
            bitmap_start,
            bitmap_end - bitmap_start
    );

    pmm_deinit_region(
            (uint32_t)mbi,
            sizeof(multiboot_info_t)
    );

    pmm_deinit_region(
            mbi->mmap_addr,
            mbi->mmap_length
    );

    if (mbi->mods_count != 0 &&
        mbi->mods_addr != 0) {

        pmm_deinit_region(
                mbi->mods_addr,
                mbi->mods_count * 16
        );
    }

    return 1;
}

void* pmm_alloc_block(void)
{
    int free_bit = pmm_bitmap_first_free();

    if (free_bit == -1) {
        return 0;
    }

    pmm_bitmap_set((uint32_t)free_bit);

    return (void*)((uint32_t)free_bit * PMM_BLOCK_SIZE);
}

void pmm_free_block(void* p)
{
    uint32_t addr = (uint32_t)p;

    if ((addr & (PMM_BLOCK_SIZE - 1)) != 0) {
        kprint("PMM: unaligned block address\n");
        return;
    }

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
