#include "gdt.h"

struct GDT_entry gdt[3];
struct GDT_ptr gdt_p;

extern void gdt_flush(uint32_t);

void set_gdt_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

void setup_gdt() {
    gdt_p.limit = (sizeof(struct GDT_entry) * 3) - 1;
    gdt_p.base  = (uint32_t)&gdt;

    // 1. Null descriptor
    set_gdt_gate(0, 0, 0, 0, 0);
    // 2. Code segment (Base: 0, Limit: 4GB, Access: Code, Granularity: 4KB blocks, 32-bit)
    set_gdt_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    // 3. Data segment (Base: 0, Limit: 4GB, Access: Data, Granularity: 4KB blocks, 32-bit)
    set_gdt_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    gdt_flush((uint32_t)&gdt_p);
}