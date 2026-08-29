#include "drivers.h"
#include "io.h"
#include "vga.h"

volatile char command_buffer[64];
volatile int command_len = 0;
volatile uint8_t command_ready = 0;
volatile uint32_t timer_ticks = 0;
const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 0x00 - 0x09 */
  '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r',	/* 0x0A - 0x13 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',    0,	/* 0x14 - 0x1D */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 0x1E - 0x27 */
 '\'', '`',   0, '\\', 'z', 'x', 'c', 'v', 'b', 'n',	/* 0x28 - 0x31 */
  'm', ',', '.', '/',   0, '*',   0, ' ',   0,   0	/* 0x32 - 0x3B */
};

void init_timer(uint32_t frequency) {
    uint32_t divisor = 1193182 / frequency; //tick frequency

    //Channel 0, Access mode: lobyte/hibyte, Operating mode 3 (square wave), Binary count
    outb(0x43, 0x36);

    //divisor
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler() {
    timer_ticks++;
    
    if (timer_ticks % 100 == 0) {
        debug_put('T', 79); //debug: every 100 ticks
		if (timer_ticks % 30 == 0){
			debug_put('t', 79);
		}
    }

    //EOI
    outb(0x20, 0x20);
}

void sleep(uint32_t ticks) {
    uint32_t target_ticks = timer_ticks + ticks;
    while(timer_ticks < target_ticks) {
        asm volatile("hlt"); //halt CPU
    }
}

void keyboard_handler() {
    debug_put('H', 74);

    uint8_t scancode = inb(0x60);

    if (scancode & 0x80) {
        outb(0x20, 0x20); // EOI
        return;
    }

    if (scancode < sizeof(scancode_to_ascii)) {
        char ascii = scancode_to_ascii[scancode];

		if (ascii == '\n') {
            command_buffer[command_len] = '\0';
            
            sym = ((sym / 160) + 1) * 160;
            
            command_ready = 1;
			
        } else if (ascii == '\b') {
            if (command_len > 0) {
                command_len--;
                sym = sym - 2;
                
                video_memory[sym] = ' ';
                video_memory[1 + sym] = (VGA_C_BLUE << 4) | VGA_C_WHITE;
                update_cursor(sym / 2);
            }
		} else if (ascii != 0) {
            //store commands
            if (command_len < 63) {
                command_buffer[command_len++] = ascii;
                
                if (sym >= 4000) {
                    scroll();
                }
                
                video_memory[sym] = ascii;
                video_memory[sym + 1] = (VGA_C_BLUE << 4) | VGA_C_WHITE;
                sym += 2;
                update_cursor(sym / 2);
            }
        }
    }

    // EOI
    outb(0x20, 0x20);
}

//cmos raw date
uint8_t get_cmos_register(int reg) {
    outb(CMOS_ADDRESS_PORT, reg);
    return inb(CMOS_DATA_PORT);
}

uint8_t rtc_second;
uint8_t rtc_minute;
uint8_t rtc_hour;
uint8_t rtc_day;
uint8_t rtc_month;
uint32_t rtc_year;

//rtc timer
void read_rtc() {
    while (get_cmos_register(0x0A) & 0x80);

    rtc_second = get_cmos_register(0x00);
    rtc_minute = get_cmos_register(0x02);
    rtc_hour   = get_cmos_register(0x04);
    rtc_day    = get_cmos_register(0x07);
    rtc_month  = get_cmos_register(0x08);
    uint32_t year_short = get_cmos_register(0x09);

    //convert
    uint8_t register_b = get_cmos_register(0x0B);
    if (!(register_b & 0x04)) {
        rtc_second = (rtc_second & 0x0F) + ((rtc_second / 16) * 10);
        rtc_minute = (rtc_minute & 0x0F) + ((rtc_minute / 16) * 10);
        rtc_hour   = ((rtc_hour & 0x0F) + (((rtc_hour & 0x70) / 16) * 10)) | (rtc_hour & 0x80);
        rtc_day    = (rtc_day & 0x0F) + ((rtc_day / 16) * 10);
        rtc_month  = (rtc_month & 0x0F) + ((rtc_month / 16) * 10);
        year_short = (year_short & 0x0F) + ((year_short / 16) * 10);
    }

    //convert to 24h format
    if (!(register_b & 0x02) && (rtc_hour & 0x80)) {
        rtc_hour = ((rtc_hour & 0x7F) + 12) % 24;
    }

    rtc_year = year_short + 2000;
}

void play_sound(uint32_t frequency) {
    if (frequency == 0) return;

    uint32_t divisor = 1193182 / frequency;

    outb(PIT_COMMAND_PORT, 0xB6);

    outb(PIT_CHANNEL2_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t speaker_state = inb(SPEAKER_PORT);
    if ((speaker_state & 0x03) != 0x03) {
        outb(SPEAKER_PORT, speaker_state | 0x03);
    }
}

void stop_sound() {
    uint8_t speaker_state = inb(SPEAKER_PORT) & 0xFC;
    outb(SPEAKER_PORT, speaker_state);
}

void beep(uint32_t frequency, uint32_t duration_ticks) {
    play_sound(frequency);
    sleep(duration_ticks);
    stop_sound();
}

int32_t volatile mouse_x = 0;
int32_t volatile mouse_y = 0;
uint8_t volatile mouse_buttons = 0;

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((inb(PS2_STATUS_PORT) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(PS2_STATUS_PORT) & 2) == 0) return;
        }
    }
}

void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0xD4);
    mouse_wait(1);
    outb(PS2_DATA_PORT, data);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(PS2_DATA_PORT);
}

void init_mouse() {
    uint8_t status;

    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0xA8);

    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0x20);
    mouse_wait(0);
    status = (inb(PS2_DATA_PORT) | 2);
    
    mouse_wait(1);
    outb(PS2_COMMAND_PORT, 0x60);
    mouse_wait(1);
    outb(PS2_DATA_PORT, status);

    mouse_write(0xF6);
    mouse_read();
    
    mouse_write(0xF4);
    mouse_read();
}

uint8_t mouse_cycle = 0;
int8_t mouse_packet[3];

void mouse_handler() {
    uint8_t status = inb(PS2_STATUS_PORT);
    
    if (!(status & 0x20)) {
        outb(0x20, 0x20);
        outb(0xA0, 0x20);
        return;
    }

    mouse_packet[mouse_cycle++] = inb(PS2_DATA_PORT);

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        if ((mouse_packet[0] & 0x08) == 0) {
            outb(0x20, 0x20);
            outb(0xA0, 0x20);
            return;
        }

        mouse_buttons = mouse_packet[0] & 0x07;
        int32_t offset_x = mouse_packet[1];
        int32_t offset_y = mouse_packet[2];

        if (mouse_packet[0] & 0x10) offset_x |= 0xFFFFFF00;
        if (mouse_packet[0] & 0x20) offset_y |= 0xFFFFFF00;

        mouse_x += offset_x;
        mouse_y -= offset_y;

        if (mouse_buttons & 1) debug_put('L', 70); // Left Clicked
        if (mouse_buttons & 2) debug_put('R', 71); // Right Clicked
		if (mouse_buttons & 4) debug_put('M', 72); // middle click
    }

    outb(0x20, 0x20);
    outb(0xA0, 0x20);
}