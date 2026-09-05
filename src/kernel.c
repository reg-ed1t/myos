#include "vga.h"
#include "idt.h"
#include "drivers.h"
#include "io.h"
#include "gdt.h"
#include "pmm.h"
#include "vmm.h"

extern void timer_isr_asm();
extern void keyboard_isr_asm();

int str_compare(const volatile char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void process_command(const volatile char* buffer) {
    if (str_compare(buffer, "help") == 0) {
        debug_put('C', 75);
		kprint("help-list all commands. pleased now?\n");
		kprint("clear-clear the screen. and now?\n");
		kprint("sleep-cpu halt for some time. and now?\n");
		kprint("crash-testing a crash. and now?\n");
		kprint("pcrash-testing a pmm crash. and now?\n");
		kprint("time-prints the current time. and now?\n");
		kprint("beep-beep! making beep beep sounds. now you definitely are.");
    } else if (str_compare(buffer, "clear") == 0) {
		clear();
    } else if (str_compare(buffer, "sleep") == 0) {
		sleep(200);
	} else if (str_compare(buffer, "pcrash") == 0) {
		uint32_t* unmapped_ptr = (uint32_t*)0xA0000000;
		*unmapped_ptr = 123;
	} else if (str_compare(buffer, "crash") == 0) {
        volatile int a = 5;
        volatile int b = 0;
        volatile int c = a / b;
        (void)c;
	} else if (str_compare(buffer, "time") == 0) {
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
	} else if (str_compare(buffer, "beep") == 0) {
        kprint("Beeping...");
        beep(750, 200); // 750 Hz tone for 20 ticks (approx 200ms at 100Hz PIT clock)
	} else{
		kprint("unknown command");}
}

int old_grid_x = 0;
int old_grid_y = 0;

void update_mouse_pointer() {
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

void kernel_main(void) {
	clear();

    debug_put('M', 69); //debug main started
	
	setup_gdt();
    setup_idt();
    
    debug_put('P', 72); //debug PIC remapping start
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
	
	// Initialize PMM to track 32MB of RAM, placing the bitmap array at 0x400000 (4MB)
    pmm_init(32 * 1024 * 1024, 0x400000);

    // Free up the available memory region for general allocation use (e.g., from 5MB to 32MB)
    pmm_init_region(0x500000, 27 * 1024 * 1024);

    kprint("Physical Memory Manager online.");
    new_line();
	void* block1 = pmm_alloc_block();
    void* block2 = pmm_alloc_block();

    kprint("Allocated Block 1 at: ");
    kprint_int((uint32_t)block1); // Uses your custom integer printing helper
    new_line();
    kprint("Allocated Block 2 at: ");
    kprint_int((uint32_t)block2);
    new_line();

    pmm_free_block(block1); // Return memory safely
    
	init_vmm();
    kprint("VMM (Paging) fully online.");
    new_line();

    // Allocate 1 physical block from PMM
    void* phys_frame = pmm_alloc_block();

    // Map physical frame to high virtual address 0xC0000000
    map_page(phys_frame, (void*)0xC0000000, PAGE_PRESENT | PAGE_RW);

    // Test writing to the virtual address
    uint32_t* test_ptr = (uint32_t*)0xC0000000;
    *test_ptr = 0xDEADBEEF;

    if (*test_ptr == 0xDEADBEEF) {
        kprint("Virtual Memory Test Passed! Mapped 0xC0000000 successfully.");
        new_line();
    }
	init_timer(100);
	
	init_mouse();
	
    // mask everything except IRQ1
	outb(0x21, 0xF8); 	
    outb(0xA1, 0xEF);

    kprint("system up.");
    
    sym = ((sym / 160) + 1) * 160;
    update_cursor(sym / 2);
	
	

	debug_put('s', 73); // debug STI
    asm volatile("sti");

    // Infinite kernel execution loop
    while(1) {
        if (command_ready) {
			set_cell_color(old_grid_x, old_grid_y, (VGA_C_BLUE << 4) | VGA_C_WHITE);
            process_command(command_buffer);
			new_line();
            command_len = 0;
            command_ready = 0;
			//old_grid_x = 0;
            //old_grid_y = 0;
        }
		update_mouse_pointer();
        asm volatile("hlt");
    }
}