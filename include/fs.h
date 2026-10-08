#ifndef FS_H
#define FS_H

#include <stdint.h>
#include <stddef.h>
#include "bdevice.h"

#define FS_SECTOR_SIZE BLOCKDEV_SECTOR_SIZE
#define FS_NAME_MAX 23u
#define FS_MAX_FILES 64u

//return codes
enum {
    FS_OK =  0,
    FS_ERR_NOT_MOUNTED = -1,//no file system mounted
    FS_ERR_IO = -2,//the block device reported an error
    FS_ERR_BAD_NAME = -3,//empty, too long, or bad characters
    FS_ERR_NOT_FOUND = -4,
    FS_ERR_EXISTS = -5,
    FS_ERR_NO_SPACE = -6,//not enough free blocks
    FS_ERR_DIR_FULL = -7,//all FS_MAX_FILES slots are used
    FS_ERR_CORRUPT = -8,//inconsistent on-disk structures
    FS_ERR_BAD_FS = -9,//no valid superblock on the device
    FS_ERR_BAD_ARG = -10
};

typedef struct {
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t file_count;
    uint32_t max_files;
} fs_info_t;

const char* fs_strerror(int error);

int fs_format(block_device_t* dev);

int fs_mount(block_device_t* dev);
void fs_unmount(void);
int fs_is_mounted(void);

int fs_create(const char* name);

int fs_write(const char* name, const void* data, uint32_t length);

int fs_append(const char* name, const void* data, uint32_t length);

int fs_read(const char* name, uint32_t offset, void* buffer, uint32_t length);

int fs_delete(const char* name);

int fs_rename(const char* old_name, const char* new_name);

int fs_stat(const char* name, uint32_t* size);

int fs_readdir(uint32_t* cursor, char* name_out, uint32_t* size_out);

int fs_get_info(fs_info_t* info);

#endif
