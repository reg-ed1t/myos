#ifndef VMM_H
#define VMM_H

#include <stdint.h>

// Page Entry Attributes
#define PAGE_PRESENT  0x001
#define PAGE_RW       0x002
#define PAGE_USER     0x004
#define PAGE_WRITE_THROUGH 0x008
#define PAGE_CACHE_DISABLE 0x010

// Index extraction helpers
#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3FF)
#define PAGE_TABLE_INDEX(x)     (((x) >> 12) & 0x3FF)
#define PAGE_ALIGN(x)           ((x) & ~0xFFF)

// Recursive mapping addresses (Index 1023)
#define RECURSIVE_PD_INDEX      1023
#define VMM_PAGE_TABLE_BASE     0xFFC00000
#define VMM_PAGE_DIR_BASE       0xFFFFF000

typedef uint32_t pd_entry_t;
typedef uint32_t pt_entry_t;

void init_vmm(void);
void map_page(void* phys_addr, void* virt_addr, uint32_t flags);
void unmap_page(void* virt_addr);
void page_fault_handler(uint32_t error_code, uint32_t faulting_address);

#endif