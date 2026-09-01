#ifndef idt_h
#define idt_h

#include <stdint.h>

struct IDT_entry {
    uint16_t offset_lowerbits;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_higherbits;
} __attribute__((packed));

extern struct IDT_entry idt[256];

void setup_idt();
void dummy_handler();

#endif