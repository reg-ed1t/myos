#include "gdt.h"

#define GDT_ENTRIES 6

struct GDT_entry gdt[GDT_ENTRIES];
struct GDT_ptr gdt_p;
struct tss_entry tss;

extern void gdt_flush(uint32_t);
extern void tss_flush(void);

void set_gdt_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

static void write_tss(int num, uint16_t ss0, uint32_t esp0)
{
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(struct tss_entry) - 1;

    uint8_t *p = (uint8_t *)&tss;

    for (uint32_t i = 0; i < sizeof(struct tss_entry); i++) {
        p[i] = 0;
    }

    set_gdt_gate(num, base, limit, 0x89, 0x00);

    tss.ss0 = ss0;
    tss.esp0 = esp0;
    tss.iomap_base = sizeof(struct tss_entry);
}

void setup_gdt(void)
{
    gdt_p.limit = (sizeof(struct GDT_entry) * GDT_ENTRIES) - 1;
    gdt_p.base = (uint32_t)&gdt;

    set_gdt_gate(0, 0, 0, 0, 0);

    set_gdt_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    set_gdt_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    set_gdt_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    set_gdt_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    write_tss(5, 0x10, 0);

    gdt_flush((uint32_t)&gdt_p);
    tss_flush();
}

void set_kernel_stack(uint32_t stack)
{
    tss.esp0 = stack;
}
