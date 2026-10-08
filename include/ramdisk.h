#ifndef RAMDISK_H
#define RAMDISK_H

#include <stdint.h>
#include "bdevice.h"

#define RAMDISK_MIN_SECTORS      16u
#define RAMDISK_MAX_SECTORS      65535u   //~32 MiB, the most fs.c can address
#define RAMDISK_DEFAULT_SECTORS  2048u    //1 MiB

block_device_t* ramdisk_create(uint32_t sector_count);

// Give the memory back
void ramdisk_destroy(block_device_t* dev);

#endif
