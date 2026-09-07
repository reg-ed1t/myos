#ifndef idt_h
#define idt_h

#include <stdint.h>

struct registers {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; //pushed by pusha
    uint32_t int_no, err_code;                       //manually
    uint32_t eip, cs, eflags, useresp, ss;           //by CPU
};

struct IDT_entry {
    uint16_t offset_lowerbits;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_higherbits;
} __attribute__((packed));

extern struct IDT_entry idt[256];
void exception_handler(struct registers* regs);
void setup_idt(void);

#endif
