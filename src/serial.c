#include "io.h"

#define COM1 0x3F8

#ifdef DEBUG
static int serial_ready = 0;

static void serial_putc(char c)
{
    for (uint32_t spin = 0; spin < 100000; spin++) {
        if (inb(COM1 + 5) & 0x20) {
            outb(COM1, (uint8_t)c);
            return;
        }
    }
}
#endif

void init_serial(void)
{
#ifdef DEBUG
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x1E);
    outb(COM1 + 0, 0xAE); // loopback self-test

    if (inb(COM1 + 0) != 0xAE) {
        serial_ready = 0;
        return;
    }

    outb(COM1 + 4, 0x0F);
    serial_ready = 1;
#endif
}

void print_serial(const char *str)
{
#ifdef DEBUG
    if (!serial_ready) return;
    for (int i = 0; str[i]; i++) {
        serial_putc(str[i]);
    }
#else
    (void)str;
#endif
}
