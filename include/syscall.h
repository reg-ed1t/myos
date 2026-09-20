#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "idt.h"

#define SYS_WRITE_CHAR 1

#define SYSCALL_SUCCESS 0
#define SYSCALL_INVALID 0xFFFFFFFF

uint32_t syscall_handler(struct registers* regs);

#endif
