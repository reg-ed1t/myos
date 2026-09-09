#ifndef VMM_H
#define VMM_H

#include <stdint.h>

/*
 * Page Entry Attributes
 */
#define PAGE_PRESENT        0x001
#define PAGE_RW             0x002
#define PAGE_USER           0x004
#define PAGE_WRITE_THROUGH  0x008
#define PAGE_CACHE_DISABLE  0x010

/*
 * Virtual address-space layout.
 */
#define VMM_LOW_START       0x00000000
#define VMM_LOW_END         0x01000000

#define KERNEL_HEAP_START   0x01000000
#define KERNEL_HEAP_END     0x40000000

#define USER_SPACE_START    0x40000000
#define USER_SPACE_END      0xC0000000

#define KERNEL_SPACE_START  0xC0000000
#define KERNEL_SPACE_END    0xFFC00000

#define VMM_PAGE_TABLE_BASE 0xFFC00000
#define VMM_PAGE_DIR_BASE   0xFFFFF000

#define RECURSIVE_PD_INDEX  1023

/*
 * Index extraction helpers.
 */
#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3FF)
#define PAGE_TABLE_INDEX(x)     (((x) >> 12) & 0x3FF)
#define PAGE_ALIGN(x)           ((x) & ~0xFFFU)

typedef uint32_t pd_entry_t;
typedef uint32_t pt_entry_t;

int init_vmm(void);

int map_page(
        void* phys_addr,
        void* virt_addr,
        uint32_t flags
);

int unmap_page(void* virt_addr);

void page_fault_handler_c(
        uint32_t error_code,
        uint32_t faulting_address,
        uint32_t eip,
        uint32_t cs
);

#endif
