#include "syscall.h"
#include "vga.h"
#include "fs.h"
#include "kstring.h"
#include "heap.h"

#define MAX_FDS 8

typedef struct {
    int used;
    char name[FS_NAME_MAX + 1];
    uint32_t offset;
} fd_entry_t;

static fd_entry_t fd_table[MAX_FDS];

static int alloc_fd(const char* name)
{
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = 1;
            kstrlcpy(fd_table[i].name, name, sizeof(fd_table[i].name));
            fd_table[i].offset = 0;
            return i;
        }
    }
    return -1;
}

static void free_fd(int fd)
{
    if (fd >= 0 && fd < MAX_FDS) {
        fd_table[fd].used = 0;
        fd_table[fd].name[0] = 0;
        fd_table[fd].offset = 0;
    }
}

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
    return 0xDEADBEEF;
}

static uint32_t sys_open(uint32_t name_ptr)
{
    const char* name = (const char*)name_ptr;

    int r = fs_create(name);
    if (r != FS_OK && r != FS_ERR_EXISTS)
        return (uint32_t)r;

    int fd = alloc_fd(name);
    if (fd < 0)
        return (uint32_t)FS_ERR_NO_SPACE;

    return (uint32_t)fd;
}

static uint32_t sys_read(uint32_t fd, uint32_t buf, uint32_t len)
{
    if (fd >= MAX_FDS || !fd_table[fd].used)
        return (uint32_t)FS_ERR_BAD_ARG;

    int n = fs_read(fd_table[fd].name, fd_table[fd].offset, (void*)buf, len);
    if (n > 0)
        fd_table[fd].offset += (uint32_t)n;

    return (uint32_t)n;
}

static uint32_t sys_write_file(uint32_t fd, uint32_t buf, uint32_t len)
{
    if (fd >= MAX_FDS || !fd_table[fd].used)
        return (uint32_t)FS_ERR_BAD_ARG;

    int r = fs_write(fd_table[fd].name, (const void*)buf, len);
    if (r == FS_OK)
        fd_table[fd].offset = len;   // simple: treat as overwrite

    return (uint32_t)r;
}

static uint32_t sys_close(uint32_t fd)
{
    free_fd((int)fd);
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
