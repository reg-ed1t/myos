#include "gdt.h"

#define GDT_ENTRIES 6

struct GDT_entry gdt[GDT_ENTRIES];
struct GDT_ptr gdt_p;
struct tss_entry tss;

extern void gdt_flush(uint32_t);
extern void tss_flush(void);

void set_gdt_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access      = access;
}

static void write_tss(int num, uint16_t ss0, uint32_t esp0)
{
    uint32_t base  = (uint32_t)&tss;
    uint32_t limit = sizeof(struct tss_entry) - 1;

    set_gdt_gate(num, base, limit, 0xE9, 0x00);

    // Clear TSS
    uint8_t *p = (uint8_t *)&tss;
    for (uint32_t i = 0; i < sizeof(struct tss_entry); i++) {
        p[i] = 0;
    }

    tss.ss0  = ss0;
    tss.esp0 = esp0;
    tss.cs   = 0x0B;
    tss.ss   = 0x13;
    tss.ds   = 0x13;
    tss.es   = 0x13;
    tss.fs   = 0x13;
    tss.gs   = 0x13;
    tss.iomap_base = sizeof(struct tss_entry);
}

void setup_gdt(void)
{
    gdt_p.limit = (sizeof(struct GDT_entry) * GDT_ENTRIES) - 1;
    gdt_p.base  = (uint32_t)&gdt;

    // Null
    set_gdt_gate(0, 0, 0, 0, 0);

    // Kernel code 0x08
    set_gdt_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // Kernel data 0x10
    set_gdt_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // User code 0x18 | RPL 3
    set_gdt_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // User data 0x20 | RPL 3
    set_gdt_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // TSS 0x28
    write_tss(5, 0x10, 0);

    gdt_flush((uint32_t)&gdt_p);
    tss_flush();
}

void set_kernel_stack(uint32_t stack)
{
    tss.esp0 = stack;
}
