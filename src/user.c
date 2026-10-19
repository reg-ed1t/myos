#include "user.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "vga.h"
#include "io.h"

static uint32_t user_code_va  = 0;
static uint32_t user_stack_va = 0;
static void* user_code_phys  = 0;
static void* user_stack_phys = 0;
extern void enter_user_mode(uint32_t entry, uint32_t user_esp);
extern uint32_t stack_top;

void enter_ring3(void)
{
    print_serial("going to enter ring3\n");
    kprint("RING3 START\n");

    void* code_phys  = pmm_alloc_block();
    void* stack_phys = pmm_alloc_block();

    if (!code_phys || !stack_phys) {
        if (code_phys)  pmm_free_block(code_phys);
        if (stack_phys) pmm_free_block(stack_phys);
        kprint("alloc fail\n");
        return;
    }

    uint32_t code_va  = 0x40000000;
    uint32_t stack_va = 0x40001000;

    if (!map_page(code_phys,  (void*)code_va,  PAGE_PRESENT | PAGE_RW | PAGE_USER) ||
        !map_page(stack_phys, (void*)stack_va, PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
        pmm_free_block(code_phys);
        pmm_free_block(stack_phys);
        kprint("map fail\n");
        return;
    }

    user_code_va   = code_va;
    user_stack_va  = stack_va;
    user_code_phys = code_phys;
    user_stack_phys = stack_phys;
    uint8_t* code = (uint8_t*)code_va;
    int i = 0;

    // string "test.txt" at 0x40000050
    code[0x50] = 't'; code[0x51] = 'e'; code[0x52] = 's'; code[0x53] = 't';
    code[0x54] = '.'; code[0x55] = 't'; code[0x56] = 'x'; code[0x57] = 't';
    code[0x58] = 0;

// string "hello" at 0x40000060
    code[0x60] = 'h'; code[0x61] = 'e'; code[0x62] = 'l'; code[0x63] = 'l';
    code[0x64] = 'o'; code[0x65] = 0;

// SYS_OPEN("test.txt")
    code[i++] = 0xB8; code[i++] = 0x04; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00; // mov eax, 4
    code[i++] = 0xBB; code[i++] = 0x50; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x40; // mov ebx, 0x40000050
    code[i++] = 0xCD; code[i++] = 0x80; // int 0x80
// eax now holds fd

// SYS_WRITE_FILE(fd, "hello", 5)
    code[i++] = 0x89; code[i++] = 0xC3; // mov ebx, eax
    code[i++] = 0xB8; code[i++] = 0x06; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00; // mov eax, 6
    code[i++] = 0xB9; code[i++] = 0x60; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x40; // mov ecx, 0x40000060
    code[i++] = 0xBA; code[i++] = 0x05; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00; // mov edx, 5
    code[i++] = 0xCD; code[i++] = 0x80;

// SYS_READ(fd, buffer, 5)  (buffer at 0x40000070)
    code[i++] = 0xB8; code[i++] = 0x05; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00; // mov eax, 5
    code[i++] = 0xB9; code[i++] = 0x70; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x40; // mov ecx, 0x40000070
    code[i++] = 0xBA; code[i++] = 0x05; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00; // mov edx, 5
    code[i++] = 0xCD; code[i++] = 0x80;

// SYS_WRITE(buffer, 5)  so we can see what was read
    code[i++] = 0xB8; code[i++] = 0x02; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00; // mov eax, 2
    code[i++] = 0xBB; code[i++] = 0x70; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x40; // mov ebx, 0x40000070
    code[i++] = 0xB9; code[i++] = 0x05; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00; // mov ecx, 5
    code[i++] = 0xCD; code[i++] = 0x80;

// SYS_CLOSE(fd)
    code[i++] = 0xB8; code[i++] = 0x07; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00; // mov eax, 7
    code[i++] = 0xCD; code[i++] = 0x80;

// SYS_EXIT(0)
    code[i++] = 0xB8; code[i++] = 0x03; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00;
    code[i++] = 0xBB; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00; code[i++] = 0x00;
    code[i++] = 0xCD; code[i++] = 0x80;

    code[i++] = 0xEB; code[i++] = 0xFE; // safety

    set_kernel_stack((uint32_t)&stack_top);
    enter_user_mode(code_va, stack_va + 0x1000);
}

void cleanup_user_mode(void)
{
    if (user_code_va) {
        unmap_page((void*)user_code_va);
        pmm_free_block(user_code_phys);
        user_code_va = 0;
        user_code_phys = 0;
    }
    if (user_stack_va) {
        unmap_page((void*)user_stack_va);
        pmm_free_block(user_stack_phys);
        user_stack_va = 0;
        user_stack_phys = 0;
    }
}

