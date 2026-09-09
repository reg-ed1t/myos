#include "kstring.h"
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

void kernel_main(
        uint32_t multiboot_magic,
        uint32_t multiboot_info_addr
);

static void process_command(const volatile char* buffer) {
    if (kstrcmp(buffer, "help") == 0) {
        debug_put('C', 75);
		kprint("help-list all commands. pleased now?\n");
		kprint("clear-clear the screen. and now?\n");
		kprint("sleep-cpu halt for some time. and now?\n");
		kprint("crash-testing a crash. and now?\n");
		kprint("pcrash-testing a pmm crash. and now?\n");
		kprint("time-prints the current time. and now?\n");
		kprint("beep-beep! making beep beep sounds. now you definitely are.");
    } else if (kstrcmp(buffer, "clear") == 0) {
		clear();
    } else if (kstrcmp(buffer, "sleep") == 0) {
		sleep(200);
	} else if (kstrcmp(buffer, "pcrash") == 0) {
		uint32_t* unmapped_ptr = (uint32_t*)0xA0000000;
		*unmapped_ptr = 123;
	} else if (kstrcmp(buffer, "crash") == 0) {
        volatile int a = 5;
        volatile int b = 0;
        volatile int c = a / b;
        (void)c;
	} else if (kstrcmp(buffer, "time") == 0) {
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
	} else if (kstrcmp(buffer, "beep") == 0) {
        kprint("Beeping...");
        beep(750, 200); // 750 Hz tone for 20 ticks (approx 200ms at 100Hz PIT clock)
	} else{
		kprint("unknown command");}
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

    /*
     * Remap the PIC:
     *
     * Master IRQs -> vectors 0x20-0x27
     * Slave IRQs  -> vectors 0x28-0x2F
     */
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


    /*
     * Initialize PMM from the actual Multiboot memory map.
     */
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


    /*
     * Basic PMM allocation test.
     */
    void* block1 = pmm_alloc_block();

    if (!block1) {

        kprint("FATAL: PMM allocation failed (block1).");
        new_line();

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


    /*
     * Initialize virtual memory.
     */
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


    /*
     * Allocate a physical frame for the VMM test.
     */
    void* phys_frame =
            pmm_alloc_block();

    if (!phys_frame) {

        kprint("FATAL: PMM allocation failed (VMM test frame).");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }


    if (!map_page(
            phys_frame,
            (void*)0xC0000000,
            PAGE_PRESENT | PAGE_RW)) {

        kprint("FATAL: VMM mapping failed.");
        new_line();

        pmm_free_block(phys_frame);

        cli();

        while (1) {
            hlt();
        }
    }


    uint32_t* test_ptr =
            (uint32_t*)0xC0000000;

    *test_ptr = 0xDEADBEEF;

    if (*test_ptr == 0xDEADBEEF) {

        kprint(
                "Virtual Memory Test Passed! "
                "Mapped 0xC0000000 successfully."
        );

        new_line();
    }
    /*
    * Test unmapping the page.
    */
    if (!unmap_page((void*)0xC0000000)) {
        kprint("FATAL: VMM unmap test failed.");
        new_line();

        cli();

        while (1) {
            hlt();
        }
    }

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
