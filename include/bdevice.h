#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include <stdint.h>

#define BLOCKDEV_SECTOR_SIZE 512

typedef struct block_device {
    const char* name;
    uint32_t sector_count;
    void* private_data;

    int (*read)(struct block_device* dev, uint32_t lba, uint32_t count, void* buffer);

    int (*write)(struct block_device* dev, uint32_t lba, uint32_t count, const void* buffer);
} block_device_t;

#endif
