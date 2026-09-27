#include "idt.h"
#include "vga.h"
#include "io.h"

struct IDT_entry idt[256];

//IRQ handlers

extern void keyboard_isr_asm(void);
extern void timer_isr_asm(void);
extern void mouse_isr_asm(void);
extern void syscall_isr_asm(void);

extern void dummy_isr(void);
extern void default_master_irq(void);
extern void default_slave_irq(void);


//CPU exception handlers
extern void exception_0(void);
extern void exception_1(void);
extern void exception_2(void);
extern void exception_3(void);
extern void exception_4(void);
extern void exception_5(void);
extern void exception_6(void);
extern void exception_7(void);
extern void exception_8(void);
extern void exception_9(void);
extern void exception_10(void);
extern void exception_11(void);
extern void exception_12(void);
extern void exception_13(void);
extern void exception_14(void);
extern void exception_15(void);
extern void exception_16(void);
extern void exception_17(void);
extern void exception_18(void);
extern void exception_19(void);
extern void exception_30(void);


static const char* exception_names[32] = {
        "Divide Error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack-Segment Fault",
        "General Protection Fault",
        "Page Fault",
        "Reserved",
        "x87 Floating-Point Exception",
        "Alignment Check",
        "Machine Check",
        "SIMD Floating-Point Exception",
        "Virtualization Exception",
        "Control Protection Exception",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Hypervisor Injection Exception",
        "VMM Communication Exception",
        "Security Exception",
        "Reserved"
};


static void set_gate(int number, void (*handler)(void))
{
    uint32_t address = (uint32_t)handler;

    idt[number].offset_lowerbits = address & 0xFFFF;

    idt[number].offset_higherbits = (address >> 16) & 0xFFFF;

    idt[number].selector = 0x08;
    idt[number].zero = 0;
    idt[number].type_attr = 0x8E;
}


void exception_handler(struct registers* regs)
{
    cli();

    clear();

    kprint("========== CPU EXCEPTION ==========\n\n");

    kprint("Exception: ");
    kprint_int(regs->int_no);
    kprint(" - ");

    if (regs->int_no < 32) {
        kprint(exception_names[regs->int_no]);
    } else {
        kprint("Unknown");
    }

    kprint("\n");

    kprint("Error code: ");
    kprint_hex(regs->err_code);
    kprint("\n");

    kprint("EIP:        ");
    kprint_hex(regs->eip);
    kprint("\n");

    kprint("CS:         ");
    kprint_hex(regs->cs);
    kprint("\n");

    kprint("EFLAGS:     ");
    kprint_hex(regs->eflags);
    kprint("\n");

    /*If the exception happened while running
    at privilege level 3, the CPU also supplied user ESP and SS.*/
    if ((regs->cs & 3) != 0) {
        kprint("User ESP:   ");
        kprint_hex(regs->useresp);
        kprint("\n");

        kprint("User SS:    ");
        kprint_hex(regs->ss);
        kprint("\n");
    }

    //Page faults put the faulting linear address into CR2.
    if (regs->int_no == 14) {
        uint32_t fault_address;

        __asm__ __volatile__("mov %%cr2, %0" : "=r"(fault_address));

        kprint("\nPage fault address: ");
        kprint_hex(fault_address);
        kprint("\n");

        if (regs->err_code & 0x01) {
            kprint("Reason: protection violation\n");
        } else {
            kprint("Reason: non-present page\n");
        }

        if (regs->err_code & 0x02) {
            kprint("Access: write\n");
        } else {
            kprint("Access: read\n");
        }

        if (regs->err_code & 0x04) {
            kprint("Privilege: user\n");
        } else {
            kprint("Privilege: kernel\n");
        }

        if (regs->err_code & 0x08) {
            kprint("Reserved-bit violation: yes\n");
        }

        if (regs->err_code & 0x10) {
            kprint("Instruction fetch: yes\n");
        }
    }

    kprint("\nSystem halted.\n");

    while (1) {
        hlt();
    }
}

void setup_idt(void)
{
    print_serial("going to idt setup\n");

    for (int i = 0; i < 256; i++) {
        set_gate(i, dummy_isr);
    }

    void (*exceptions[])(void) = {
        exception_0,  exception_1,  exception_2,  exception_3,  exception_4,
        exception_5,  exception_6,  exception_7,  exception_8,  exception_9,
        exception_10, exception_11, exception_12, exception_13, exception_14,
        exception_15, exception_16, exception_17, exception_18, exception_19
    };

    for (int i = 0; i < 20; i++) {
        set_gate(i, exceptions[i]);
    }

    set_gate(30, exception_30);

    for (int i = 0x20; i <= 0x27; i++) {
        set_gate(i, default_master_irq);
    }

    for (int i = 0x28; i <= 0x2F; i++) {
        set_gate(i, default_slave_irq);
    }

    //Timer
    set_gate(0x20,timer_isr_asm);

    //Keyboard.
    set_gate(0x21,keyboard_isr_asm);

    //Mouse = IRQ12 = vector 0x2C.
    set_gate(0x2C,mouse_isr_asm);

    set_gate(0x80, syscall_isr_asm);
    idt[0x80].type_attr = 0xEE;

    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) idtr = {
        sizeof(idt) - 1,
        (uint32_t)idt
    };

    __asm__ __volatile__("lidt %0": : "m"(idtr));
    print_serial("idt done\n");
}
