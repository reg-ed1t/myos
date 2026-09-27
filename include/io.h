#ifndef IO_H
#define IO_H

#include <stdint.h>

#define hlt() __asm__ __volatile__("hlt")
#define cli() __asm__ __volatile__("cli")
#define sti() __asm__ __volatile__("sti")

static void init_serial(void);
static int is_transmit_empty(void);
static void write_serial(char c);
static inline void print_serial(const char *str);

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int initialized = 0;
static void init_serial(void) {
    if(initialized){
        return;}

    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
    initialized = 1;
}

static int is_transmit_empty(void) {
    return inb(0x3F8 + 5) & 0x20;
}

static void write_serial(char c) {
    init_serial();
    while (is_transmit_empty() == 0);
    outb(0x3F8, c);
}

static inline void print_serial(const char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        write_serial(str[i]);
    }
}

#endif
