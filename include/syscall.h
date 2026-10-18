#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "idt.h"

#define SYS_WRITE_CHAR  1
#define SYS_WRITE       2
#define SYS_EXIT        3
#define SYS_OPEN        4
#define SYS_READ        5
#define SYS_WRITE_FILE  6
#define SYS_CLOSE       7

#define SYSCALL_SUCCESS 0
#define SYSCALL_INVALID 0xFFFFFFFF

uint32_t syscall_handler(struct registers* regs);

#endif
