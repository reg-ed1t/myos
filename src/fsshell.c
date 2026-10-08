#include "fsshell.h"
#include "fs.h"
#include "ramdisk.h"
#include "heap.h"
#include "kstring.h"
#include "vga.h"
#include "io.h"

#define SHELL_LINE_MAX 256u
#define NAME_BUFFER    (FS_NAME_MAX + 2u)

static block_device_t* data_disk = 0;

static void copy_line(char* dst, const volatile char* src, uint32_t capacity)
{
    uint32_t i = 0;

    while (i + 1 < capacity && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static const char* skip_spaces(const char* p)
{
    while (*p == ' ') {
        p++;
    }

    return p;
}

static int is_command(const char* line, const char* command)
{
    while (*command != '\0') {

        if (*line != *command) {
            return 0;
        }

        line++;
        command++;
    }

    return *line == '\0' || *line == ' ';
}

static const char* next_word(const char* p, char* out, uint32_t capacity)
{
    uint32_t n = 0;

    p = skip_spaces(p);

    while (*p != '\0' && *p != ' ') {

        if (n + 1 < capacity) {
            out[n++] = *p;
        }

        p++;
    }

    out[n] = '\0';

    return skip_spaces(p);
}

static void print_error(const char* command, int error)
{
    kprint(command);
    kprint(": ");
    kprint(fs_strerror(error));
}

static void print_usage(const char* text)
{
    kprint("usage: ");
    kprint(text);
}

static void cmd_ls(void)
{
    uint32_t cursor = 0;
    uint32_t size = 0;
    char name[FS_NAME_MAX + 1];
    int first = 1;
    int r;

    while ((r = fs_readdir(&cursor, name, &size)) == 1) {

        if (!first) {
            kprint("\n");
        }

        first = 0;

        kprint(name);

        for (uint32_t pad = (uint32_t)kstrlen(name); pad < FS_NAME_MAX + 2; pad++) {
            kprint(" ");
        }

        kprint_int(size);
        kprint(size == 1 ? " byte" : " bytes");
    }

    if (r < 0) {
        if (!first) {
            kprint("\n");
        }

        print_error("ls", r);
        return;
    }

    if (first) {
        kprint("(no files)");
    }
}

static void cmd_cat(const char* args)
{
    char name[NAME_BUFFER];
    uint32_t size = 0;

    next_word(args, name, sizeof(name));

    if (name[0] == '\0') {
        print_usage("cat <file>");
        return;
    }

    int r = fs_stat(name, &size);

    if (r != FS_OK) {
        print_error("cat", r);
        return;
    }

    if (size == 0) {
        kprint("(empty file)");
        return;
    }

    uint8_t chunk[64];
    char out[2 * sizeof(chunk) + 1];
    uint32_t offset = 0;
    int pending_newline = 0;

    while (offset < size) {

        int n = fs_read(name, offset, chunk, sizeof(chunk));

        if (n <= 0) {

            if (n < 0) {
                kprint("\n");
                print_error("cat", n);
            }

            return;
        }

        uint32_t o = 0;

        for (int i = 0; i < n; i++) {

            if (pending_newline) {
                out[o++] = '\n';
                pending_newline = 0;
            }

            if (chunk[i] == '\n') {
                pending_newline = 1;
            } else if (chunk[i] >= 0x20 && chunk[i] < 0x7F) {
                out[o++] = (char)chunk[i];
            } else {
                out[o++] = '.';
            }
        }

        out[o] = '\0';
        kprint(out);

        offset += (uint32_t)n;
    }
}

static void cmd_write(const char* args, int append)
{
    char name[NAME_BUFFER];
    const char* text = next_word(args, name, sizeof(name));

    if (name[0] == '\0' || (append && *text == '\0')) {
        print_usage(append ? "append <file> <text>" : "write <file> [text]");
        return;
    }

    char data[SHELL_LINE_MAX + 2];
    uint32_t length = 0;

    if (*text != '\0') {
        length = (uint32_t)kstrlcpy(data, text, SHELL_LINE_MAX);

        if (length >= SHELL_LINE_MAX) {
            length = SHELL_LINE_MAX - 1;
        }

        data[length++] = '\n';
    }

    int r = append ? fs_append(name, data, length) : fs_write(name, data, length);

    if (r != FS_OK) {
        print_error(append ? "append" : "write", r);
        return;
    }

    kprint(append ? "appended " : "wrote ");
    kprint_int(length);
    kprint(" bytes to ");
    kprint(name);
}

static void cmd_touch(const char* args)
{
    char name[NAME_BUFFER];

    next_word(args, name, sizeof(name));

    if (name[0] == '\0') {
        print_usage("touch <file>");
        return;
    }

    int r = fs_create(name);

    if (r != FS_OK && r != FS_ERR_EXISTS) {
        print_error("touch", r);
        return;
    }

    kprint("ok");
}

static void cmd_rm(const char* args)
{
    char name[NAME_BUFFER];

    next_word(args, name, sizeof(name));

    if (name[0] == '\0') {
        print_usage("rm <file>");
        return;
    }

    int r = fs_delete(name);

    if (r != FS_OK) {
        print_error("rm", r);
        return;
    }

    kprint("removed ");
    kprint(name);
}

static void cmd_mv(const char* args)
{
    char from[NAME_BUFFER];
    char to[NAME_BUFFER];

    const char* rest = next_word(args, from, sizeof(from));
    next_word(rest, to, sizeof(to));

    if (from[0] == '\0' || to[0] == '\0') {
        print_usage("mv <from> <to>");
        return;
    }

    int r = fs_rename(from, to);

    if (r != FS_OK) {
        print_error("mv", r);
        return;
    }

    kprint("renamed ");
    kprint(from);
    kprint(" to ");
    kprint(to);
}

static void cmd_cp(const char* args)
{
    char from[NAME_BUFFER];
    char to[NAME_BUFFER];
    uint32_t size = 0;

    const char* rest = next_word(args, from, sizeof(from));
    next_word(rest, to, sizeof(to));

    if (from[0] == '\0' || to[0] == '\0') {
        print_usage("cp <from> <to>");
        return;
    }

    if (kstrcmp(from, to) == 0) {
        kprint("cp: source and destination are the same file");
        return;
    }

    int r = fs_stat(from, &size);

    if (r != FS_OK) {
        print_error("cp", r);
        return;
    }

    uint8_t* buffer = (uint8_t*)kmalloc(size != 0 ? size : 1);

    if (!buffer) {
        kprint("cp: out of memory");
        return;
    }

    r = (size == 0) ? 0 : fs_read(from, 0, buffer, size);

    if (r >= 0 && (uint32_t)r == size) {
        r = fs_write(to, buffer, size);
    } else if (r >= 0) {
        r = FS_ERR_CORRUPT;
    }

    kfree(buffer);

    if (r != FS_OK) {
        print_error("cp", r);
        return;
    }

    kprint("copied ");
    kprint_int(size);
    kprint(" bytes to ");
    kprint(to);
}

static void print_kb(uint32_t bytes)
{
    uint32_t tenth = (bytes % 1024u) * 10u / 1024u;

    kprint_int(bytes / 1024u);

    if (tenth != 0) {
        kprint(".");
        kprint_int(tenth);
    }

    kprint(" KB");
}

static void cmd_df(void)
{
    fs_info_t info;
    int r = fs_get_info(&info);

    if (r != FS_OK) {
        print_error("df", r);
        return;
    }

    uint32_t used = info.total_blocks - info.free_blocks;

    kprint("RAM disk: ");
    print_kb(info.total_blocks * info.block_size);
    kprint(" total, ");
    print_kb(used * info.block_size);
    kprint(" used, ");
    print_kb(info.free_blocks * info.block_size);
    kprint(" free\n");

    kprint("Blocks: ");
    kprint_int(used);
    kprint(" of ");
    kprint_int(info.total_blocks);
    kprint(" used (");
    kprint_int(info.block_size);
    kprint(" bytes each), files: ");
    kprint_int(info.file_count);
    kprint(" of ");
    kprint_int(info.max_files);
}

int fs_shell_handle(const volatile char* line)
{
    char buffer[SHELL_LINE_MAX];

    copy_line(buffer, line, sizeof(buffer));

    const char* args = buffer;

    while (*args != '\0' && *args != ' ') {
        args++;
    }

    args = skip_spaces(args);

    if (is_command(buffer, "ls")) {
        cmd_ls();
    } else if (is_command(buffer, "cat")) {
        cmd_cat(args);
    } else if (is_command(buffer, "write")) {
        cmd_write(args, 0);
    } else if (is_command(buffer, "append")) {
        cmd_write(args, 1);
    } else if (is_command(buffer, "touch")) {
        cmd_touch(args);
    } else if (is_command(buffer, "cp")) {
        cmd_cp(args);
    } else if (is_command(buffer, "mv")) {
        cmd_mv(args);
    } else if (is_command(buffer, "rm")) {
        cmd_rm(args);
    } else if (is_command(buffer, "df")) {
        cmd_df();
    } else {
        return 0;
    }

    return 1;
}

int fs_shell_init(void)
{

    data_disk = ramdisk_create(RAMDISK_DEFAULT_SECTORS);

    if (!data_disk) {
        kprint("WARNING: not enough memory for the RAM disk.");
        new_line();
        return 0;
    }

    int r = fs_format(data_disk);

    if (r != FS_OK) {
        kprint("WARNING: could not format the RAM disk: ");
        kprint(fs_strerror(r));
        new_line();
        return 0;
    }

    static const char welcome[] =
            "Welcome to myos! This file lives on the RAM disk.\n"
            "Try: ls, cat, write, append, cp, mv, rm, df\n";

    fs_write("readme.txt", welcome, (uint32_t)(sizeof(welcome) - 1));

    print_serial("RAM disk ready.\n");

    return 1;
}
