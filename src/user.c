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

    void *code_phys = pmm_alloc_block();
    void *stack_phys = pmm_alloc_block();

    if (!code_phys || !stack_phys) {
        kprint("alloc fail\n");
        return;
    }

    uint32_t code_va = 0x40000000;
    uint32_t stack_va = 0x40001000;

    if (!map_page(code_phys, (void*)code_va, PAGE_PRESENT | PAGE_RW | PAGE_USER) ||
        !map_page(stack_phys, (void*)stack_va, PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
        kprint("map fail\n");
        return;
    }

    uint8_t *code = (uint8_t*)code_va;

    // mov eax, SYS_WRITE_CHAR
    code[0] = 0xB8;
    code[1] = 0x01;
    code[2] = 0x00;
    code[3] = 0x00;
    code[4] = 0x00;

    // mov ebx, 'U'
    code[5] = 0xBB;
    code[6] = 'U';
    code[7] = 0x00;
    code[8] = 0x00;
    code[9] = 0x00;

    // int 0x80
    code[10] = 0xCD;
    code[11] = 0x80;

    // mov eax, SYS_WRITE_CHAR
    code[12] = 0xB8;
    code[13] = 0x01;
    code[14] = 0x00;
    code[15] = 0x00;
    code[16] = 0x00;

    // mov ebx, 'S'
    code[17] = 0xBB;
    code[18] = 'S';
    code[19] = 0x00;
    code[20] = 0x00;
    code[21] = 0x00;

    // int 0x80
    code[22] = 0xCD;
    code[23] = 0x80;

    // mov eax, SYS_WRITE_CHAR
    code[24] = 0xB8;
    code[25] = 0x01;
    code[26] = 0x00;
    code[27] = 0x00;
    code[28] = 0x00;

    // mov ebx, 'E'
    code[29] = 0xBB;
    code[30] = 'E';
    code[31] = 0x00;
    code[32] = 0x00;
    code[33] = 0x00;

    // int 0x80
    code[34] = 0xCD;
    code[35] = 0x80;

    // mov eax, SYS_WRITE_CHAR
    code[36] = 0xB8;
    code[37] = 0x01;
    code[38] = 0x00;
    code[39] = 0x00;
    code[40] = 0x00;

    // mov ebx, 'R'
    code[41] = 0xBB;
    code[42] = 'R';
    code[43] = 0x00;
    code[44] = 0x00;
    code[45] = 0x00;

    // int 0x80
    code[46] = 0xCD;
    code[47] = 0x80;

    // jmp $
    code[48] = 0xEB;
    code[49] = 0xFE;

    set_kernel_stack((uint32_t)&stack_top);

    enter_user_mode(code_va, stack_va + 0x1000);
}
