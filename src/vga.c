#include "vga.h"
#include "io.h"
#include "drivers.h"

volatile char* video_memory = (char*) 0xB8000;
volatile uint16_t sym = 0;

void clear(){
	for (int i = 0; i < 4000; i += 2) {
        video_memory[i] = ' ';
        video_memory[i + 1] = (VGA_C_BLUE << 4) | VGA_C_WHITE;
    }
    sym = 0;
    update_cursor(0);
	command_len = 0;
	command_buffer[0] = '\0';
}

void scroll() {

    for (int i = 0; i < 3840; i++) {
        video_memory[i] = video_memory[i + 160];
    }

    for (int i = 3840; i < 4000; i += 2) {
        video_memory[i] = ' ';
        video_memory[i + 1] = (VGA_C_BLUE << 4) | VGA_C_WHITE;
    }

    sym = 3840;
    update_cursor(sym / 2);
}

void new_line(){
    if (sym != 0) { 
		sym = ((sym / 160) + 1) * 160;
	} else {
		sym = 0;
	}
	if (sym >= 4000) {
		scroll();
	}
	update_cursor(sym / 2);
};

void debug_put(char c, int pos) {
    video_memory[pos * 2] = c;
    video_memory[pos * 2 + 1] = (VGA_C_RED << 4) | VGA_C_WHITE;
}

void update_cursor(int character_index) {
	
    uint16_t pos = character_index;

    //the lower 8 bits
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    
    //the upper 8 bits
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void kprint(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            new_line();
        } else {
            if (sym >= 4000) {
                scroll();
            }
            video_memory[sym] = str[i];
            video_memory[sym + 1] = (VGA_C_BLUE << 4) | VGA_C_WHITE;;
            sym += 2;
            update_cursor(sym / 2);
        }
    }
}

void kprint_int(uint32_t num) {
    char str[11];
    int i = 0;
    if (num == 0) {
        kprint("0");
        return;
    }
    while (num > 0) {
        str[i++] = (num % 10) + '0';
        num /= 10;
    }
    str[i] = '\0';
    
    //reverse
    for (int j = 0; j < i / 2; j++) {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }
    kprint(str);
}

void kprint_hex(uint32_t num) {
    char hex_str[11] = "0x00000000";
    char hex_chars[] = "0123456789ABCDEF";
    
    for (int i = 0; i < 8; i++) {
        hex_str[9 - i] = hex_chars[(num >> (i * 4)) & 0x0F];
    }
    kprint(hex_str);
}

void set_cell_color(int x, int y, uint8_t color) {
    if (x < 0 || x >= 80 || y < 0 || y >= 25) return;

    int offset = (y * 80 + x) * 2;
    video_memory[offset + 1] = color;
}