#include "idt.h"
#include "vga.h"
#include "io.h"
#include "drivers.h"

struct IDT_entry idt[256];

//IRQ handlers

extern void keyboard_isr_asm(void);
extern void timer_isr_asm(void);
extern void mouse_isr_asm(void);

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


static void set_gate(
        int number,
        void (*handler)(void)
)
{
    uint32_t address = (uint32_t)handler;

    idt[number].offset_lowerbits =
            address & 0xFFFF;

    idt[number].offset_higherbits =
            (address >> 16) & 0xFFFF;

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
    at privilege level 3, the CPU also supplied
    user ESP and SS.*/
    if ((regs->cs & 3) != 0) {
        kprint("User ESP:   ");
        kprint_hex(regs->useresp);
        kprint("\n");

        kprint("User SS:    ");
        kprint_hex(regs->ss);
        kprint("\n");
    }

    /*Page faults put the faulting linear address
    into CR2.*/
    if (regs->int_no == 14) {
        uint32_t fault_address;

        __asm__ __volatile__(
                "mov %%cr2, %0"
                : "=r"(fault_address)
                );

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
    debug_put('I', 70);

    uint32_t dummy_addr =
            (uint32_t)dummy_isr;

    /*Everything initially points at a harmless
    dummy interrupt handler.*/
    for (int i = 0; i < 256; i++) {
        idt[i].offset_lowerbits =
                dummy_addr & 0xFFFF;

        idt[i].offset_higherbits =
                (dummy_addr >> 16) & 0xFFFF;

        idt[i].selector = 0x08;
        idt[i].zero = 0;
        idt[i].type_attr = 0x8E;
    }

    //CPU exceptions 0-19.
    set_gate(0, exception_0);
    set_gate(1, exception_1);
    set_gate(2, exception_2);
    set_gate(3, exception_3);
    set_gate(4, exception_4);
    set_gate(5, exception_5);
    set_gate(6, exception_6);
    set_gate(7, exception_7);
    set_gate(8, exception_8);
    set_gate(9, exception_9);
    set_gate(10, exception_10);
    set_gate(11, exception_11);
    set_gate(12, exception_12);
    set_gate(13, exception_13);
    set_gate(14, exception_14);
    set_gate(15, exception_15);
    set_gate(16, exception_16);
    set_gate(17, exception_17);
    set_gate(18, exception_18);
    set_gate(19, exception_19);

    //Exception 30
    set_gate(30, exception_30);

    /*PIC IRQs.
    Master IRQs: 0x20-0x27
    Slave IRQs:  0x28-0x2F*/

    uint32_t master_irq_addr =
            (uint32_t)default_master_irq;

    uint32_t slave_irq_addr =
            (uint32_t)default_slave_irq;

    for (int i = 0x22; i <= 0x27; i++) {
        idt[i].offset_lowerbits =
                master_irq_addr & 0xFFFF;

        idt[i].offset_higherbits =
                (master_irq_addr >> 16) & 0xFFFF;
    }

    for (int i = 0x28; i <= 0x2F; i++) {
        idt[i].offset_lowerbits =
                slave_irq_addr & 0xFFFF;

        idt[i].offset_higherbits =
                (slave_irq_addr >> 16) & 0xFFFF;
    }

    //Timer
    set_gate(
            0x20,
            timer_isr_asm
    );

    //Keyboard.
    set_gate(
            0x21,
            keyboard_isr_asm
    );

    //Mouse = IRQ12 = vector 0x2C.
    set_gate(
            0x2C,
            mouse_isr_asm
    );

    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) idtr = {
            sizeof(idt) - 1,
            (uint32_t)idt
    };

    __asm__ __volatile__(
            "lidt %0": : "m"(idtr)
            );

    debug_put('D', 71);
}
