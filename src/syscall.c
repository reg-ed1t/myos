#include "syscall.h"
#include "vga.h"

static uint32_t sys_write_char(uint32_t character)
{
    char str[2];

    str[0] = (char)character;
    str[1] = '\0';

    kprint(str);

    return SYSCALL_SUCCESS;
}

uint32_t syscall_handler(struct registers* regs)
{
    switch (regs->eax) {
        case SYS_WRITE_CHAR:
            return sys_write_char(regs->ebx);

        default:
            return SYSCALL_INVALID;
    }
}
