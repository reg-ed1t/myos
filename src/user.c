#include "user.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "vga.h"

extern void enter_user_mode(uint32_t entry, uint32_t user_esp);
extern uint32_t stack_top;

void enter_ring3(void)
{
    kprint("RING3 START\n");

    void *code_phys  = pmm_alloc_block();
    void *stack_phys = pmm_alloc_block();

    if (!code_phys || !stack_phys) {
        kprint("alloc fail\n");
        return;
    }

    uint32_t code_va  = 0x40000000;
    uint32_t stack_va = 0x40001000;

    if (!map_page(code_phys,  (void*)code_va,  PAGE_PRESENT | PAGE_RW | PAGE_USER) ||
        !map_page(stack_phys, (void*)stack_va, PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
        kprint("map fail\n");
        return;
    }

    // Map VGA so user mode can write to the screen
    if (!map_page((void*)0xB8000, (void*)0x40003000, PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
        kprint("map vga fail\n");
        return;
    }

    // Tiny user program that writes "USER" and then loops forever
    uint8_t *code = (uint8_t*)code_va;

    // mov eax, 0x40003000
    code[0]  = 0xB8; code[1]  = 0x00; code[2]  = 0x30; code[3]  = 0x00; code[4]  = 0x40;

    // mov word [eax+0], 'U' | 0x0F00
    code[5]  = 0x66; code[6]  = 0xC7; code[7]  = 0x00;
    code[8]  = 0x55; code[9]  = 0x0F;

    // mov word [eax+2], 'S'
    code[10] = 0x66; code[11] = 0xC7; code[12] = 0x40; code[13] = 0x02;
    code[14] = 0x53; code[15] = 0x0F;

    // mov word [eax+4], 'E'
    code[16] = 0x66; code[17] = 0xC7; code[18] = 0x40; code[19] = 0x04;
    code[20] = 0x45; code[21] = 0x0F;

    // mov word [eax+6], 'R'
    code[22] = 0x66; code[23] = 0xC7; code[24] = 0x40; code[25] = 0x06;
    code[26] = 0x52; code[27] = 0x0F;

    // jmp $
    code[28] = 0xEB;
    code[29] = 0xFE;

    set_kernel_stack((uint32_t)&stack_top);

    enter_user_mode(code_va, stack_va + 0x800);
}
