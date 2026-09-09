#include "kstring.h"
#include "heap.h"
#include "vga.h"
#include "idt.h"
#include "drivers.h"
#include "io.h"
#include "gdt.h"
#include "pmm.h"
#include "vmm.h"

extern void timer_isr_asm(void);
extern void keyboard_isr_asm(void);

extern uint32_t __kernel_start;
extern uint32_t __kernel_end;

static void trigger_zero_divide(void)
{
    volatile int a = 5;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}


static void trigger_page_fault(void)
{
    volatile uint32_t* ptr = (volatile uint32_t*)0xA0000000;

    *ptr = 0xDEADBEEF;
}


static void trigger_invalid_opcode(void)
{
    __asm__ __volatile__("ud2");
}


static void trigger_overflow(void)
{
    __asm__ __volatile__("int $4");
}


static void trigger_breakpoint(void)
{
    __asm__ __volatile__("int $3");
}


static void trigger_bounds(void)
{
    __asm__ __volatile__("int $5");
}

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_addr);

static int command_is(const volatile char* buffer, const char* command)
{
    while (*command != '\0') {

        if (*buffer != *command) {
            return 0;
        }

        buffer++;
        command++;
    }

    return *buffer == '\0' || *buffer == ' ';
}

static const volatile char* command_argument(const volatile char* buffer)
{
    while (*buffer != '\0' && *buffer != ' ') {
        buffer++;
    }

    while (*buffer == ' ') {
        buffer++;
    }

    return buffer;
}

static void process_command(const volatile char* buffer)
{
    const volatile char* argument;

    if (command_is(buffer, "help")) {

        debug_put('C', 75);

        kprint("help - list all commands.\n");
        kprint("clear - clear the screen.\n");
        kprint("sleep - halt the CPU for some time.\n");
        kprint("crash - list crash tests.\n");
        kprint("time - print the current time.\n");
        kprint("beep - make a beep.\n");
        kprint("Crash tests:\n");
        kprint("  crash -zerodivide\n");
        kprint("  crash -pages\n");
        kprint("  crash -invalidopcode\n");
        kprint("  crash -overflow\n");
        kprint("  crash -breakpoint\n");
        kprint("  crash -bounds\n");

    } else if (command_is(buffer, "clear")) {

        clear();

    } else if (command_is(buffer, "sleep")) {

        sleep(200);

    } else if (command_is(buffer, "crash")) {

        argument = command_argument(buffer);

        if (*argument == '\0') {

            kprint("Crash tests need an argument.\n");

        } else if (kstrcmp(argument, "-zerodivide") == 0) {

            trigger_zero_divide();

        } else if (kstrcmp(argument, "-pages") == 0) {

            trigger_page_fault();

        } else if (kstrcmp(argument, "-invalidopcode") == 0) {

            trigger_invalid_opcode();

        } else if (kstrcmp(argument, "-overflow") == 0) {

            trigger_overflow();

        } else if (kstrcmp(argument, "-breakpoint") == 0) {

            trigger_breakpoint();

        } else if (kstrcmp(argument, "-bounds") == 0) {

            trigger_bounds();

        } else {

            kprint("unknown crash test\n");
        }

    } else if (command_is(buffer, "time")) {

        read_rtc();

        kprint("Current UTC Date/Time: ");
        kprint_int(rtc_year);
        kprint("-");
        kprint_int(rtc_month);
        kprint("-");
        kprint_int(rtc_day);
        kprint(" ");
        kprint_int(rtc_hour);
        kprint(":");
        kprint_int(rtc_minute);
        kprint(":");
        kprint_int(rtc_second);

    } else if (command_is(buffer, "beep")) {

        kprint("Beeping...");
        beep(750, 200);

    } else {

        kprint("unknown command\n");
    }
}

int old_grid_x = 0;
int old_grid_y = 0;

static void update_mouse_pointer(void) {
    int current_grid_x = mouse_x / 16;
    int current_grid_y = mouse_y / 16;

    if (current_grid_x < 0)  { current_grid_x = 0;  mouse_x = 0; }
    if (current_grid_x >= 80) { current_grid_x = 79; mouse_x = 79 * 16; }
    if (current_grid_y < 0)  { current_grid_y = 0;  mouse_y = 0; }
    if (current_grid_y >= 25) { current_grid_y = 24; mouse_y = 24 * 16; }

    if (current_grid_x == old_grid_x && current_grid_y == old_grid_y) {
        return;
    }

    set_cell_color(old_grid_x, old_grid_y, (VGA_C_BLUE << 4) | VGA_C_WHITE);

    set_cell_color(current_grid_x, current_grid_y, (VGA_C_RED << 4) | VGA_C_DARK_GREY);

    old_grid_x = current_grid_x;
    old_grid_y = current_grid_y;
}

void kernel_main(
        uint32_t multiboot_magic,
        uint32_t multiboot_info_addr
)
{
    clear();

    debug_put('M', 69);

    setup_gdt();
    setup_idt();

    debug_put('P', 72);

    /*Remap the PIC
    Master IRQs -> vectors 0x20-0x27
    Slave IRQs  -> vectors 0x28-0x2F*/
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    outb(0x21, 0x01);
    outb(0xA1, 0x01);


    /*
     * Verify that GRUB actually gave us Multiboot data.
     */
    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {

        kprint("FATAL: invalid Multiboot magic.");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }

    if (multiboot_info_addr == 0) {

        kprint("FATAL: invalid Multiboot info pointer.");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }

    //Initialize PMM from the actual Multiboot memory map
    multiboot_info_t* mbi =
            (multiboot_info_t*)multiboot_info_addr;

    if (!pmm_init_multiboot(
            mbi,
            (uint32_t)&__kernel_start,
            (uint32_t)&__kernel_end)) {

        kprint("FATAL: PMM initialization failed.");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }

    kprint("Physical Memory Manager online.");
    new_line();


    //Basic PMM allocation test.
    void* block1 = pmm_alloc_block();

    if (!block1) {

        kprint("FATAL: PMM allocation failed (block1).\n");

        kprint("PMM blocks: ");
        kprint_int(pmm_max_blocks);
        kprint("\n");

        kprint("PMM bitmap: ");
        kprint_hex((uint32_t)pmm_bitmap);
        kprint("\n");

        cli();

        while (1) {
            hlt();
        }
    }


    void* block2 = pmm_alloc_block();

    if (!block2) {

        kprint("FATAL: PMM allocation failed (block2).");
        new_line();

        pmm_free_block(block1);

        cli();

        while (1) {
            hlt();
        }
    }


    //Initialize virtual memory
    if (!init_vmm()) {

        kprint("FATAL: VMM initialization failed.");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }

    kprint("VMM (Paging) fully online.");
    new_line();

    heap_init();

    void* heap_test_a = kmalloc(64);
    void* heap_test_b = kmalloc(128);
    void* heap_test_c = kmalloc(4096);

    if (!heap_test_a ||
        !heap_test_b ||
        !heap_test_c) {

        kprint("FATAL: kernel heap allocation failed.");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }

    kprint("Kernel heap allocation test passed.");
    new_line();

    kfree(heap_test_b);

    void* heap_test_d = kmalloc(96);

    if (!heap_test_d) {
        kprint("FATAL: kernel heap reuse test failed.");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }

    kprint("Kernel heap reuse test passed.");
    new_line();

    kfree(heap_test_a);
    kfree(heap_test_c);
    kfree(heap_test_d);


    //Allocate a physical frame for the VMM test
    void* phys_frame = pmm_alloc_block();

    if (!phys_frame) {

        kprint("FATAL: PMM allocation failed (VMM test frame).\n");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }

    uint32_t* test_ptr = (uint32_t*)0xC0000000;

    if (!map_page(phys_frame, test_ptr, PAGE_RW)) {

        kprint("FATAL: VMM map test failed.\n");
        new_line();

        pmm_free_block(phys_frame);

        cli();

        while (1) {
            hlt();
        }
    }

    *test_ptr = 0xDEADBEEF;

    if (*test_ptr == 0xDEADBEEF) {
        kprint("Virtual Memory Test Passed!""Mapped 0xC0000000 successfully.");
        new_line();
    }

    if (!unmap_page((void*)0xC0000000)) {

        kprint("FATAL: VMM unmap test failed.\n");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }

    pmm_free_block(phys_frame);

    kprint("Virtual Memory Test Passed! Unmapped 0xC0000000 successfully.");
    new_line();

    init_timer(100);

    init_mouse();

    outb(0x21, 0xF8);
    outb(0xA1, 0xEF);

    kprint("system up.");
    
    sym = ((sym / 160) + 1) * 160;
    update_cursor(sym / 2);

	debug_put('s', 73); // debug STI
    sti();

    // Infinite kernel execution loop
    while(1) {
        if (command_ready) {
			set_cell_color(old_grid_x, old_grid_y, (VGA_C_BLUE << 4) | VGA_C_WHITE);
            process_command(command_buffer);
			new_line();
            command_len = 0;
            command_ready = 0;
        }
		update_mouse_pointer();
        hlt();
    }
}
