#include "user.h"
#include "pmm.h"
#include "vmm.h"
#include "gdt.h"
#include "vga.h"
#include "io.h"
#include "kstring.h"
#include "syscall.h"

#define NAME_OFF   0x800 // data lives far behind the code, so the two can never overlap
#define HELLO_OFF  0x810
#define BUFFER_OFF 0x820

static uint32_t user_code_va  = 0;
static uint32_t user_stack_va = 0;
static void* user_code_phys  = 0;
static void* user_stack_phys = 0;
extern void enter_user_mode(uint32_t entry, uint32_t user_esp);
extern uint32_t ring0_stack_top;

// Write a 32-bit immediate, little endian
static void emit32(uint8_t* code, int* i, uint32_t value)
{
    for (int k = 0; k < 4; k++) {
        code[(*i)++] = (uint8_t)(value >> (8 * k));
    }
}

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
        unmap_page((void*)code_va);
        unmap_page((void*)stack_va);
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

    memset(code, 0, 4096);
    kstrlcpy((char*)(code + NAME_OFF), "test.txt", 16);
    kstrlcpy((char*)(code + HELLO_OFF), "hello", 16);

    // SYS_OPEN("test.txt"), esi keeps the fd because every syscall returns in eax
    code[i++] = 0xB8; emit32(code, &i, SYS_OPEN);                 // mov eax, SYS_OPEN
    code[i++] = 0xBB; emit32(code, &i, code_va + NAME_OFF);       // mov ebx, name
    code[i++] = 0xCD; code[i++] = 0x80;                           // int 0x80
    code[i++] = 0x89; code[i++] = 0xC6;                           // mov esi, eax

    // SYS_WRITE_FILE(fd, "hello", 5)
    code[i++] = 0x89; code[i++] = 0xF3;                           // mov ebx, esi
    code[i++] = 0xB8; emit32(code, &i, SYS_WRITE_FILE);           // mov eax, SYS_WRITE_FILE
    code[i++] = 0xB9; emit32(code, &i, code_va + HELLO_OFF);      // mov ecx, hello
    code[i++] = 0xBA; emit32(code, &i, 5);                        // mov edx, 5
    code[i++] = 0xCD; code[i++] = 0x80;

    // SYS_READ(fd, buffer, 5)
    code[i++] = 0x89; code[i++] = 0xF3;                           // mov ebx, esi
    code[i++] = 0xB8; emit32(code, &i, SYS_READ);                 // mov eax, SYS_READ
    code[i++] = 0xB9; emit32(code, &i, code_va + BUFFER_OFF);     // mov ecx, buffer
    code[i++] = 0xBA; emit32(code, &i, 5);                        // mov edx, 5
    code[i++] = 0xCD; code[i++] = 0x80;

    // SYS_WRITE(buffer, 5), shows what was read
    code[i++] = 0xB8; emit32(code, &i, SYS_WRITE);                // mov eax, SYS_WRITE
    code[i++] = 0xBB; emit32(code, &i, code_va + BUFFER_OFF);     // mov ebx, buffer
    code[i++] = 0xB9; emit32(code, &i, 5);                        // mov ecx, 5
    code[i++] = 0xCD; code[i++] = 0x80;

    // SYS_CLOSE(fd)
    code[i++] = 0x89; code[i++] = 0xF3;                           // mov ebx, esi
    code[i++] = 0xB8; emit32(code, &i, SYS_CLOSE);                // mov eax, SYS_CLOSE
    code[i++] = 0xCD; code[i++] = 0x80;

    // SYS_EXIT(0)
    code[i++] = 0xB8; emit32(code, &i, SYS_EXIT);                 // mov eax, SYS_EXIT
    code[i++] = 0xBB; emit32(code, &i, 0);                        // mov ebx, 0
    code[i++] = 0xCD; code[i++] = 0x80;

    code[i++] = 0xEB; code[i++] = 0xFE; // safety

    set_kernel_stack((uint32_t)&ring0_stack_top);
    enter_user_mode(code_va, stack_va + 0x1000);

    // SYS_EXIT comes back here
    cleanup_user_mode();
    kprint("\nReturned from user mode");
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
