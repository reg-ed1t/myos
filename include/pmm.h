#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PMM_BLOCK_SIZE 4096

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_INFO_MEMORY_MAP  0x00000040

#define MULTIBOOT_MEMORY_AVAILABLE 1

typedef struct {
    uint32_t size;

    uint32_t addr_low;
    uint32_t addr_high;

    uint32_t len_low;
    uint32_t len_high;

    uint32_t type;
} multiboot_mmap_entry_t;

typedef struct {
    uint32_t flags;

    uint32_t mem_lower;
    uint32_t mem_upper;

    uint32_t boot_device;

    uint32_t cmdline;

    uint32_t mods_count;
    uint32_t mods_addr;

    uint32_t syms[4];

    uint32_t mmap_length;
    uint32_t mmap_addr;

    uint32_t drives_length;
    uint32_t drives_addr;

    uint32_t config_table;

    uint32_t boot_loader_name;

    uint32_t apm_table;

    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;

    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
} multiboot_info_t;

extern uint32_t* pmm_bitmap;
extern uint32_t  pmm_max_blocks;
extern uint32_t  pmm_bitmap_size;

void pmm_init(uint32_t mem_size, uint32_t bitmap_start_addr);

void pmm_init_region(uint32_t base_addr, uint32_t size);
void pmm_deinit_region(uint32_t base_addr, uint32_t size);

int pmm_init_multiboot(
        const multiboot_info_t* mbi,
        uint32_t kernel_start,
        uint32_t kernel_end
);

void* pmm_alloc_block(void);
void pmm_free_block(void* p);

#endif
