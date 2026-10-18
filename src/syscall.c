#include "syscall.h"
#include "vga.h"
#include "fs.h"
#include "kstring.h"

static uint32_t sys_write_char(uint32_t c)
{
    char s[2] = { (char)c, 0 };
    kprint(s);
    return SYSCALL_SUCCESS;
}

static uint32_t sys_write(uint32_t ptr, uint32_t len)
{
    const char* s = (const char*)ptr;
    for (uint32_t i = 0; i < len; i++) {
        char t[2] = { s[i], 0 };
        kprint(t);
    }
    return SYSCALL_SUCCESS;
}

static uint32_t sys_exit(uint32_t code)
{
    (void)code;
    // Special magic value the ISR looks for
    return 0xDEADBEEF;
}

static uint32_t sys_open(uint32_t name_ptr)
{
    const char* name = (const char*)name_ptr;
    // For now we just check if the file exists
    fs_dirent_t entry;
    (void)entry;
    uint32_t index;
    (void)index;
    int r = fs_create(name);// create if not exists (simple)
    if (r != FS_OK && r != FS_ERR_EXISTS)
        return (uint32_t)r;
    return 0; // fake fd 0 for now
}

static uint32_t sys_read(uint32_t fd, uint32_t buf, uint32_t len)
{
    (void)fd;
    (void)buf;
    (void)len;
    // Very simple demo – just return 0 for now
    return 0;
}

static uint32_t sys_write_file(uint32_t fd, uint32_t buf, uint32_t len)
{
    (void)fd; (void)buf; (void)len;
    return 0;
}

static uint32_t sys_close(uint32_t fd)
{
    (void)fd;
    return SYSCALL_SUCCESS;
}

uint32_t syscall_handler(struct registers* regs)
{
    switch (regs->eax) {
        case SYS_WRITE_CHAR:
            return sys_write_char(regs->ebx);
        case SYS_WRITE:
            return sys_write(regs->ebx, regs->ecx);
        case SYS_EXIT:
            return sys_exit(regs->ebx);
        case SYS_OPEN:
            return sys_open(regs->ebx);
        case SYS_READ:
            return sys_read(regs->ebx, regs->ecx, regs->edx);
        case SYS_WRITE_FILE:
            return sys_write_file(regs->ebx, regs->ecx, regs->edx);
        case SYS_CLOSE:
            return sys_close(regs->ebx);
        default:
            return SYSCALL_INVALID;
    }
}
