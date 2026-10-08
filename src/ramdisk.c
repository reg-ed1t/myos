#include "ramdisk.h"
#include "heap.h"
#include "kstring.h"

static int ramdisk_range_ok(const block_device_t* dev,
                            uint32_t lba, uint32_t count)
{
    return count != 0 && lba < dev->sector_count && count <= dev->sector_count - lba;
}

static int ramdisk_read(block_device_t* dev,
                        uint32_t lba, uint32_t count, void* buffer)
{
    if (!dev || !dev->private_data || !buffer || !ramdisk_range_ok(dev, lba, count)) {
        return 0;
    }

    const uint8_t* storage = (const uint8_t*)dev->private_data;

    memcpy(buffer, storage + (size_t)lba * BLOCKDEV_SECTOR_SIZE, (size_t)count * BLOCKDEV_SECTOR_SIZE);

    return 1;
}

static int ramdisk_write(block_device_t* dev, uint32_t lba, uint32_t count, const void* buffer)
{
    if (!dev || !dev->private_data || !buffer || !ramdisk_range_ok(dev, lba, count)) {
        return 0;
    }

    uint8_t* storage = (uint8_t*)dev->private_data;

    memcpy(storage + (size_t)lba * BLOCKDEV_SECTOR_SIZE, buffer, (size_t)count * BLOCKDEV_SECTOR_SIZE);

    return 1;
}

block_device_t* ramdisk_create(uint32_t sector_count)
{
    if (sector_count < RAMDISK_MIN_SECTORS ||
        sector_count > RAMDISK_MAX_SECTORS) {
        return 0;
    }

    block_device_t* dev = (block_device_t*)kmalloc(sizeof(block_device_t));
    uint8_t* storage = (uint8_t*)kmalloc(sector_count * BLOCKDEV_SECTOR_SIZE);

    if (!dev || !storage) {
        kfree(dev);
        kfree(storage);
        return 0;
    }

    memset(storage, 0, (size_t)sector_count * BLOCKDEV_SECTOR_SIZE);

    dev->name = "ram0";
    dev->sector_count = sector_count;
    dev->private_data = storage;
    dev->read = ramdisk_read;
    dev->write = ramdisk_write;

    return dev;
}

void ramdisk_destroy(block_device_t* dev)
{
    if (!dev) {
        return;
    }

    kfree(dev->private_data);
    kfree(dev);
}
