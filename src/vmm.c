#include "io.h"
#include "vmm.h"
#include "pmm.h"
#include "vga.h"

extern void load_page_directory_asm(uint32_t* page_directory_phys);
extern void enable_paging_asm(void);
extern void invalidate_tlb_asm(uint32_t virt_addr);

static uint32_t* current_page_directory_phys = 0;

int init_vmm(void) {
    // 1. Allocate a physical frame for the Master Page Directory
    current_page_directory_phys = (uint32_t*)pmm_alloc_block();
    uint32_t* pd = current_page_directory_phys;
    if (!current_page_directory_phys) {
        kprint("VMM: failed to allocate page directory\n");
        return 0;
    }

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

			if (!new_pt_phys) {
				kprint("VMM: out of physical memory\n");
				return 0;
			}

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
    return 1;
}

int map_page(void* phys_addr, void* virt_addr, uint32_t flags)
{
    uint32_t vaddr = (uint32_t)virt_addr;
    uint32_t paddr = (uint32_t)phys_addr;

    uint32_t pd_idx = PAGE_DIRECTORY_INDEX(vaddr);
    uint32_t pt_idx = PAGE_TABLE_INDEX(vaddr);

    uint32_t* pd = (uint32_t*)VMM_PAGE_DIR_BASE;

    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        void* new_pt_phys = pmm_alloc_block();

        if (!new_pt_phys) {
            kprint("VMM: out of physical memory\n");
            return 0;
        }

        pd[pd_idx] = ((uint32_t)new_pt_phys) |
                     PAGE_PRESENT |
                     PAGE_RW |
                     flags;

        invalidate_tlb_asm(
                VMM_PAGE_TABLE_BASE + (pd_idx * 4096)
        );

        uint32_t* pt =
                (uint32_t*)(VMM_PAGE_TABLE_BASE + (pd_idx * 4096));

        for (int i = 0; i < 1024; i++) {
            pt[i] = 0;
        }
    }

    uint32_t* pt =
            (uint32_t*)(VMM_PAGE_TABLE_BASE + (pd_idx * 4096));

    pt[pt_idx] = (paddr & ~0xFFF) | PAGE_PRESENT | flags;

    invalidate_tlb_asm(vaddr);

    return 1;
}

int unmap_page(void* virt_addr)
{
    uint32_t vaddr =
            (uint32_t)virt_addr;

    uint32_t pd_idx =
            PAGE_DIRECTORY_INDEX(vaddr);

    uint32_t pt_idx =
            PAGE_TABLE_INDEX(vaddr);

    /*
     * The recursive page-directory entry is special.
     * Never allow it to be unmapped through this function.
     */
    if (pd_idx == RECURSIVE_PD_INDEX) {
        return 0;
    }

    uint32_t* pd =
            (uint32_t*)VMM_PAGE_DIR_BASE;

    /*
     * No page table exists for this virtual address.
     */
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        return 0;
    }

    uint32_t* pt =
            (uint32_t*)
                    (VMM_PAGE_TABLE_BASE +
                     (pd_idx * 4096));

    /*
     * The page is already unmapped.
     */
    if (!(pt[pt_idx] & PAGE_PRESENT)) {
        return 0;
    }

    /*
     * Remove the PTE first.
     */
    pt[pt_idx] = 0;

    /*
     * Make sure the CPU no longer has the old translation.
     */
    invalidate_tlb_asm(vaddr);


    /*
     * Check whether the page table is now completely empty.
     */
    for (int i = 0; i < 1024; i++) {

        if (pt[i] & PAGE_PRESENT) {
            return 1;
        }
    }


    /*
     * Nothing is using this page table anymore.
     *
     * Save its physical address before clearing the PDE.
     */
    uint32_t pt_phys =
            pd[pd_idx] & ~0xFFFU;

    /*
     * Remove the page-directory entry.
     */
    pd[pd_idx] = 0;

    /*
     * The recursive page-table virtual address now
     * refers to an unmapped page, so invalidate it too.
     */
    invalidate_tlb_asm(
            VMM_PAGE_TABLE_BASE +
            (pd_idx * 4096)
    );

    /*
     * Return the now-unused page table to the PMM.
     */
    pmm_free_block((void*)pt_phys);

    return 1;
}

// Low-level diagnostic C handler called from exception 14 assembly stub
void page_fault_handler_c(uint32_t error_code, uint32_t faulting_address) {
    kprint("PAGE FAULT\n");

    kprint("Address: ");
    kprint_hex(faulting_address);
    kprint("\n");

    if (error_code & 0x1)
        kprint("Present: yes (protection violation)\n");
    else{
		kprint("Present: no\n");}

    if (error_code & 0x2){
        kprint("Access: write\n");}
    else{
        kprint("Access: read\n");}

    if (error_code & 0x4){
        kprint("Mode: user\n");}
    else{
        kprint("Mode: kernel\n");}
	
	while(1) { hlt(); }
}
