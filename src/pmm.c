#include "pmm.h"
#include "vga.h"

uint32_t* pmm_bitmap = 0;
uint32_t  pmm_max_blocks = 0;
uint32_t  pmm_bitmap_size = 0;

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
    for (uint32_t i = 0;
         i < ((pmm_max_blocks + 31) / 32);
         i++) {

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

    pmm_bitmap_size =
            (pmm_max_blocks + 31) / 32;

    /*
     * Start with everything reserved.
     *
     * Individual usable regions are opened later.
     */
    for (uint32_t i = 0;
         i < pmm_bitmap_size;
         i++) {

        pmm_bitmap[i] = 0xFFFFFFFFU;
    }
}


void pmm_init_region(uint32_t base_addr, uint32_t size)
{
    uint32_t align_block =
            base_addr / PMM_BLOCK_SIZE;

    uint32_t num_blocks =
            size / PMM_BLOCK_SIZE;

    if (align_block >= pmm_max_blocks) {
        return;
    }

    if (num_blocks >
        pmm_max_blocks - align_block) {

        num_blocks =
                pmm_max_blocks - align_block;
    }

    for (uint32_t i = 0;
         i < num_blocks;
         i++) {

        pmm_bitmap_unset(align_block + i);
    }
}


void pmm_deinit_region(uint32_t base_addr, uint32_t size)
{
    uint32_t align_block =
            base_addr / PMM_BLOCK_SIZE;

    uint32_t num_blocks =
            size / PMM_BLOCK_SIZE;

    if (align_block >= pmm_max_blocks) {
        return;
    }

    if (num_blocks >
        pmm_max_blocks - align_block) {

        num_blocks =
                pmm_max_blocks - align_block;
    }

    for (uint32_t i = 0;
         i < num_blocks;
         i++) {

        pmm_bitmap_set(align_block + i);
    }
}


static uint32_t multiboot_get_max_memory(
        const multiboot_info_t* mbi
)
{
    uint32_t max_addr = 0;

    uint32_t current =
            mbi->mmap_addr;

    uint32_t end =
            mbi->mmap_addr + mbi->mmap_length;

    while (current < end) {

        multiboot_mmap_entry_t* entry =
                (multiboot_mmap_entry_t*)current;

        /*
         * We only support the physical address space
         * visible to a 32-bit kernel.
         *
         * If the region extends above 4 GiB, clamp it.
         */
        uint32_t region_end;

        if (entry->addr_high != 0 ||
            entry->len_high != 0) {

            region_end = 0xFFFFFFFFU;
        } else {
            region_end =
                    entry->addr_low + entry->len_low;

            /*
             * Detect 32-bit overflow.
             */
            if (region_end < entry->addr_low) {
                region_end = 0xFFFFFFFFU;
            }
        }

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            if (region_end > max_addr) {
                max_addr = region_end;
            }
        }

        current += entry->size + sizeof(entry->size);
    }

    /*
     * Round down to the last complete 4 KiB block.
     */
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

    if (mbi->mmap_length == 0 ||
        mbi->mmap_addr == 0) {

        kprint("PMM: invalid memory map\n");
        return 0;
    }

    uint32_t max_memory =
            multiboot_get_max_memory(mbi);

    if (max_memory < PMM_BLOCK_SIZE) {
        kprint("PMM: no usable memory found\n");
        return 0;
    }

    /*
     * Keep the bitmap at 4 MiB for now.
     *
     * This is safe for the current memory layout because
     * the kernel itself starts at 1 MiB and the bitmap is
     * reserved again below.
     */
    const uint32_t bitmap_start =
            0x400000;

    pmm_init(max_memory, bitmap_start);

    /*
     * Open every usable region reported by GRUB.
     */
    uint32_t current =
            mbi->mmap_addr;

    uint32_t end =
            mbi->mmap_addr + mbi->mmap_length;

    while (current < end) {

        multiboot_mmap_entry_t* entry =
                (multiboot_mmap_entry_t*)current;

        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE &&
            entry->addr_high == 0 &&
            entry->len_high == 0) {

            uint32_t base =
                    entry->addr_low;

            uint32_t size =
                    entry->len_low;

            /*
             * Only whole 4 KiB blocks are useful.
             */
            base =
                    (base + PMM_BLOCK_SIZE - 1)
                    & ~(PMM_BLOCK_SIZE - 1);

            size =
                    size & ~(PMM_BLOCK_SIZE - 1);

            if (size != 0) {
                pmm_init_region(base, size);
            }
        }

        current +=
                entry->size + sizeof(entry->size);
    }

    /*
     * Reserve the kernel image.
     */
    pmm_deinit_region(
            kernel_start,
            kernel_end - kernel_start
    );

    /*
     * Reserve the bitmap itself.
     */
    uint32_t bitmap_bytes =
            pmm_bitmap_size * sizeof(uint32_t);

    pmm_deinit_region(
            bitmap_start,
            bitmap_bytes
    );

    /*
     * Reserve the Multiboot information structure.
     */
    pmm_deinit_region(
            (uint32_t)mbi,
            sizeof(multiboot_info_t)
    );

    /*
     * Reserve the memory-map entries themselves.
     */
    pmm_deinit_region(
            mbi->mmap_addr,
            mbi->mmap_length
    );

    /*
     * Reserve the module table if GRUB supplied one.
     *
     * We don't currently load modules, but reserving the
     * descriptors prevents PMM from handing them out.
     */
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
    int free_bit =
            pmm_bitmap_first_free();

    if (free_bit == -1) {
        return 0;
    }

    pmm_bitmap_set((uint32_t)free_bit);

    uint32_t physical_address =
            (uint32_t)free_bit * PMM_BLOCK_SIZE;

    return (void*)physical_address;
}


void pmm_free_block(void* p)
{
    uint32_t addr =
            (uint32_t)p;

    /*
     * Physical allocations must be page aligned.
     */
    if ((addr & (PMM_BLOCK_SIZE - 1)) != 0) {
        kprint("PMM: unaligned block address\n");
        return;
    }

    uint32_t block =
            addr / PMM_BLOCK_SIZE;

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
