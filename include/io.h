#ifndef IO_H
#define IO_H

#include <stdint.h>

#define hlt() __asm__ __volatile__("hlt")
#define cli() __asm__ __volatile__("cli")
#define sti() __asm__ __volatile__("sti")

void init_serial(void);
void print_serial(const char *str);

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

#endif
