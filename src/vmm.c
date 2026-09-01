#include "vmm.h"
#include "pmm.h"
#include "vga.h"

extern void load_page_directory_asm(uint32_t* page_directory_phys);
extern void enable_paging_asm(void);
extern void invalidate_tlb_asm(uint32_t virt_addr);

static uint32_t* current_page_directory_phys = 0;

void init_vmm() {
    // 1. Allocate a physical frame for the Master Page Directory
    current_page_directory_phys = (uint32_t*)pmm_alloc_block();
    uint32_t* pd = current_page_directory_phys;

    // Clear all directory entries (Mark NOT PRESENT)
    for (int i = 0; i < 1024; i++) {
        pd[i] = 0 | PAGE_RW;
    }

    // 2. Identity Map the first 8MB (0x0 to 0x800000) using PMM-allocated page tables
    // This keeps kernel code, stack, VGA buffer, and PMM bitmap active when paging turns on.
    for (uint32_t phys = 0; phys < 0x800000; phys += 4096) {
        uint32_t pd_idx = PAGE_DIRECTORY_INDEX(phys);
        uint32_t pt_idx = PAGE_TABLE_INDEX(phys);

        // Allocate a page table if one doesn't exist for this 4MB block
        if (!(pd[pd_idx] & PAGE_PRESENT)) {
            void* new_pt_phys = pmm_alloc_block();
            // Clear new page table memory
            uint32_t* pt_ptr = (uint32_t*)new_pt_phys;
            for(int k = 0; k < 1024; k++) pt_ptr[k] = 0;

            pd[pd_idx] = ((uint32_t)new_pt_phys) | PAGE_PRESENT | PAGE_RW;
        }

        uint32_t* pt = (uint32_t*)(pd[pd_idx] & ~0xFFF);
        pt[pt_idx] = phys | PAGE_PRESENT | PAGE_RW;
    }

    // 3. Set up RECURSIVE MAPPING at slot 1023
    // Point slot 1023 back to the Page Directory itself
    pd[RECURSIVE_PD_INDEX] = ((uint32_t)current_page_directory_phys) | PAGE_PRESENT | PAGE_RW;

    // 4. Register control registers & enable CPU paging
    load_page_directory_asm(current_page_directory_phys);
    enable_paging_asm();
}

void map_page(void* phys_addr, void* virt_addr, uint32_t flags) {
    uint32_t vaddr = (uint32_t)virt_addr;
    uint32_t paddr = (uint32_t)phys_addr;

    uint32_t pd_idx = PAGE_DIRECTORY_INDEX(vaddr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(vaddr);

    // Access page directory via recursive mapping memory
    uint32_t* pd = (uint32_t*)VMM_PAGE_DIR_BASE;

    // Check if Page Table is present
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        void* new_pt_phys = pmm_alloc_block();
        pd[pd_idx] = ((uint32_t)new_pt_phys) | PAGE_PRESENT | PAGE_RW | flags;

        // Invalidate TLB for the corresponding page table access range
        invalidate_tlb_asm(VMM_PAGE_TABLE_BASE + (pd_idx * 4096));

        // Zero out the newly mapped page table
        uint32_t* pt = (uint32_t*)(VMM_PAGE_TABLE_BASE + (pd_idx * 4096));
        for (int i = 0; i < 1024; i++) {
            pt[i] = 0;
        }
    }

    // Access Page Table through recursive address offset and insert physical mapping
    uint32_t* pt = (uint32_t*)(VMM_PAGE_TABLE_BASE + (pd_idx * 4096));
    pt[pt_idx] = (paddr & ~0xFFF) | PAGE_PRESENT | flags;

    // Invalidate stale TLB entry for the target virtual address
    invalidate_tlb_asm(vaddr);
}

void unmap_page(void* virt_addr) {
    uint32_t vaddr = (uint32_t)virt_addr;
    uint32_t pd_idx = PAGE_DIRECTORY_INDEX(vaddr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(vaddr);

    uint32_t* pd = (uint32_t*)VMM_PAGE_DIR_BASE;

    if (pd[pd_idx] & PAGE_PRESENT) {
        uint32_t* pt = (uint32_t*)(VMM_PAGE_TABLE_BASE + (pd_idx * 4096));
        pt[pt_idx] = 0; // Mark page NOT PRESENT
        invalidate_tlb_asm(vaddr);
    }
}

// Low-level diagnostic C handler called from exception 14 assembly stub
void page_fault_handler_c(uint32_t error_code, uint32_t faulting_address) {
    kprint("PAGE FAULT DETECTED! ");
    kprint("Faulting Virtual Address: ");
    
    // Convert faulting virtual address to displayable output
    uint32_t num = faulting_address;
    char hex_str[11] = "0x00000000";
    char hex_chars[] = "0123456789ABCDEF";
    for(int i = 9; i >= 2; i--) {
        hex_str[i] = hex_chars[num & 0xF];
        num >>= 4;
    }
    kprint(hex_str);
    new_line();

    if (!(error_code & 0x1)) kprint("Reason: Page Not Present\n");
    if (error_code & 0x2)   kprint("Reason: Write Operation Violation\n");
    if (error_code & 0x4)   kprint("Reason: Processor Executing in User-Mode\n");

    while(1) { asm volatile("hlt"); }
}