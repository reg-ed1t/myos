#ifndef vga_h
#define vga_h

#include <stdint.h>

extern volatile char* video_memory;
extern volatile uint16_t sym;

// colors for printing
enum vga_colors{
	VGA_C_BLACK = 0,
	VGA_C_BLUE = 1,
	VGA_C_GREEN = 2,
	VGA_C_CYAN = 3,
	VGA_C_RED = 4,
	VGA_C_MAGENTA = 5,
	VGA_C_BROWN = 6,
	VGA_C_LIGHT_GREY = 7,
	VGA_C_DARK_GREY = 8,
	VGA_C_LIGHT_BLUE = 9,
	VGA_C_LIGHT_GREEN = 10,
	VGA_C_LIGHT_CYAN = 11,
	VGA_C_LIGHT_RED = 12,
	VGA_C_LIGHT_MAGENTA = 13,
	VGA_C_LIGHT_BROWN = 14,
	VGA_C_WHITE = 15,
};

void clear();
void new_line();
void scroll(void);
void debug_put(char c, int pos);
void update_cursor(int character_index);
void kprint(const char* str);
void kprint_int(uint32_t num);
void kprint_hex(uint32_t num);
void set_cell_color(int x, int y, uint8_t color);

#endif