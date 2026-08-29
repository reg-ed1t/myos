#include "idt.h"
#include "vga.h"
#include "io.h"
#include "drivers.h"

struct IDT_entry idt[256];

extern void keyboard_isr_asm();
extern void timer_isr_asm();
extern void mouse_isr_asm();

void dummy_handler() {
    outb(0x20, 0x20);
    outb(0xA0, 0x20);
}

// Declare the assembly stubs
extern void exception_0();
extern void exception_13();
extern void exception_14();

struct registers {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; //pushed by pusha
    uint32_t int_no, err_code;                       //manually
    uint32_t eip, cs, eflags, useresp, ss;           //by CPU
};

void exception_handler(struct registers* regs) {
    clear();
    if (regs->int_no == 0) {
        kprint("E: Divide by Zero Error!");
    } else if (regs->int_no == 13) {
        kprint("E: General Protection Fault!");
    } else {
        kprint("E: Unknown CPU Exception!");
    }
    
    while(1) {
        asm volatile("hlt");
    }
}

void setup_idt() {
    debug_put('I', 70); // debug IDT start
    
    for(int i = 0; i < 256; i++) {
        idt[i].offset_lowerbits = ((uint32_t)dummy_handler) & 0xFFFF;
        idt[i].selector = 0x08;
        idt[i].zero = 0;
        idt[i].type_attr = 0x8E;
        idt[i].offset_higherbits = (((uint32_t)dummy_handler) >> 16) & 0xFFFF;
    }
	
	//keyboard IDT
    uint32_t kb_address = (uint32_t)keyboard_isr_asm;
    idt[0x21].offset_lowerbits = kb_address & 0xFFFF;
    idt[0x21].offset_higherbits = (kb_address >> 16) & 0xFFFF;
	
	//timer IDT
	uint32_t timer_address = (uint32_t)timer_isr_asm;
    idt[0x20].offset_lowerbits = timer_address & 0xFFFF;
    idt[0x20].offset_higherbits = (timer_address >> 16) & 0xFFFF;
	
	//exceptions
    uint32_t exc0_addr = (uint32_t)exception_0;
    idt[0].offset_lowerbits = exc0_addr & 0xFFFF;
    idt[0].offset_higherbits = (exc0_addr >> 16) & 0xFFFF;

    uint32_t exc13_addr = (uint32_t)exception_13;
    idt[13].offset_lowerbits = exc13_addr & 0xFFFF;
    idt[13].offset_higherbits = (exc13_addr >> 16) & 0xFFFF;

    uint32_t exc14_addr = (uint32_t)exception_14;
    idt[14].offset_lowerbits = exc14_addr & 0xFFFF;
    idt[14].offset_higherbits = (exc14_addr >> 16) & 0xFFFF;
	
	//mouse
	uint32_t mouse_address = (uint32_t)mouse_isr_asm;
    idt[0x2C].offset_lowerbits = mouse_address & 0xFFFF;
    idt[0x2C].offset_higherbits = (mouse_address >> 16) & 0xFFFF;

    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) idtr = { sizeof(idt) - 1, (uint32_t)idt };

    asm volatile("lidt %0" : : "m"(idtr));
    debug_put('D', 71); //debug IDT loaded
}