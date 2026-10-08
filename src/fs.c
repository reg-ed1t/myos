#include "fs.h"
#include "kstring.h"

#define FS_MAGIC            0x5346594Du
#define FS_VERSION          1u
#define FS_MIN_SECTORS      16u
#define FS_MAX_SECTORS      0xFFFFu

#define FS_DIRENT_SIZE      32u
#define FS_DIRENTS_PER_SEC  (FS_SECTOR_SIZE / FS_DIRENT_SIZE)
#define FS_DIR_SECTORS      (FS_MAX_FILES / FS_DIRENTS_PER_SEC)

#define FAT_ENTRIES_PER_SEC (FS_SECTOR_SIZE / 2u)
#define FAT_FREE            ((uint16_t)0x0000)
#define FAT_EOC             ((uint16_t)0xFFFF)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t total_sectors;
    uint32_t fat_start;
    uint32_t fat_sectors;
    uint32_t dir_start;
    uint32_t dir_sectors;
    uint32_t data_start;
    uint32_t data_blocks;
} fs_super_t;

typedef struct {
    char name[FS_NAME_MAX + 1];
    uint32_t size;
    uint16_t first_block;
    uint8_t used;
    uint8_t reserved;
} fs_dirent_t;

typedef union {
    uint8_t bytes[FS_SECTOR_SIZE];
    uint16_t fat[FS_SECTOR_SIZE / 2];
    fs_dirent_t dir[FS_SECTOR_SIZE / FS_DIRENT_SIZE];
    fs_super_t super;
} fs_sector_t;

typedef char fs_check_dirent_size[(sizeof(fs_dirent_t) == FS_DIRENT_SIZE) ? 1 : -1];
typedef char fs_check_sector_size[(sizeof(fs_sector_t) == FS_SECTOR_SIZE) ? 1 : -1];

static block_device_t* fs_dev = 0;
static fs_super_t      fs_sb;

const char* fs_strerror(int error)
{
    switch (error) {
        case FS_OK: return "ok";
        case FS_ERR_NOT_MOUNTED: return "no file system mounted";
        case FS_ERR_IO: return "I/O error";
        case FS_ERR_BAD_NAME: return "invalid file name (1-23 chars, no spaces or '/')";
        case FS_ERR_NOT_FOUND: return "file not found";
        case FS_ERR_EXISTS: return "file already exists";
        case FS_ERR_NO_SPACE: return "no space left on device";
        case FS_ERR_DIR_FULL: return "directory is full";
        case FS_ERR_CORRUPT: return "file system is corrupt";
        case FS_ERR_BAD_FS: return "no valid file system on device";
        case FS_ERR_BAD_ARG: return "invalid argument";
        default: return "unknown error";
    }
}

static int dev_read(uint32_t lba, fs_sector_t* sector)
{
    return fs_dev->read(fs_dev, lba, 1, sector) ? FS_OK : FS_ERR_IO;
}

static int dev_write(uint32_t lba, const fs_sector_t* sector)
{
    return fs_dev->write(fs_dev, lba, 1, sector) ? FS_OK : FS_ERR_IO;
}

static int block_valid(uint32_t block)
{
    return block >= 1 && block <= fs_sb.data_blocks;
}

static uint32_t block_to_sector(uint32_t block)
{
    return fs_sb.data_start + block - 1;
}

static uint32_t blocks_for(uint32_t length)
{
    return length / FS_SECTOR_SIZE + ((length % FS_SECTOR_SIZE) != 0 ? 1u : 0u);
}

static int fat_get(uint16_t block, uint16_t* next)
{
    fs_sector_t sector;
    int r = dev_read(fs_sb.fat_start + block / FAT_ENTRIES_PER_SEC, &sector);

    if (r != FS_OK) {
        return r;
    }

    *next = sector.fat[block % FAT_ENTRIES_PER_SEC];
    return FS_OK;
}

static int fat_set(uint16_t block, uint16_t value)
{
    fs_sector_t sector;
    uint32_t lba = fs_sb.fat_start + block / FAT_ENTRIES_PER_SEC;
    int r = dev_read(lba, &sector);

    if (r != FS_OK) {
        return r;
    }

    sector.fat[block % FAT_ENTRIES_PER_SEC] = value;
    return dev_write(lba, &sector);
}

static int fat_alloc(uint16_t* out)
{
    fs_sector_t sector;

    for (uint32_t s = 0; s < fs_sb.fat_sectors; s++) {

        int r = dev_read(fs_sb.fat_start + s, &sector);

        if (r != FS_OK) {
            return r;
        }

        for (uint32_t i = 0; i < FAT_ENTRIES_PER_SEC; i++) {

            uint32_t block = s * FAT_ENTRIES_PER_SEC + i;

            if (block == 0) {
                continue;
            }

            if (block > fs_sb.data_blocks) {
                return FS_ERR_NO_SPACE;
            }

            if (sector.fat[i] == FAT_FREE) {
                sector.fat[i] = FAT_EOC;

                r = dev_write(fs_sb.fat_start + s, &sector);

                if (r != FS_OK) {
                    return r;
                }

                *out = (uint16_t)block;
                return FS_OK;
            }
        }
    }

    return FS_ERR_NO_SPACE;
}

static int count_free_blocks(uint32_t* out)
{
    fs_sector_t sector;
    uint32_t count = 0;

    for (uint32_t s = 0; s < fs_sb.fat_sectors; s++) {

        int r = dev_read(fs_sb.fat_start + s, &sector);

        if (r != FS_OK) {
            return r;
        }

        for (uint32_t i = 0; i < FAT_ENTRIES_PER_SEC; i++) {

            uint32_t block = s * FAT_ENTRIES_PER_SEC + i;

            if (block_valid(block) && sector.fat[i] == FAT_FREE) {
                count++;
            }
        }
    }

    *out = count;
    return FS_OK;
}

static int chain_next(uint16_t block, uint16_t* next)
{
    if (!block_valid(block)) {
        return FS_ERR_CORRUPT;
    }

    return fat_get(block, next);
}

static int chain_stats(uint16_t first, uint32_t* length, uint16_t* last)
{
    uint32_t count = 0;
    uint16_t tail = 0;
    uint16_t block = first;

    while (block != 0 && block != FAT_EOC) {

        uint16_t next;

        if (count >= fs_sb.data_blocks) {
            return FS_ERR_CORRUPT;
        }

        int r = chain_next(block, &next);

        if (r != FS_OK) {
            return r;
        }

        count++;
        tail = block;
        block = next;
    }

    if (length) {
        *length = count;
    }

    if (last) {
        *last = tail;
    }

    return FS_OK;
}

static int free_chain(uint16_t first)
{
    uint32_t steps = 0;
    uint16_t block = first;

    while (block != 0 && block != FAT_EOC) {

        uint16_t next;

        if (steps++ >= fs_sb.data_blocks) {
            return FS_ERR_CORRUPT;
        }

        int r = chain_next(block, &next);

        if (r != FS_OK) {
            return r;
        }

        r = fat_set(block, FAT_FREE);

        if (r != FS_OK) {
            return r;
        }

        block = next;
    }

    return FS_OK;
}

static int write_block(uint16_t block, const uint8_t* data, uint32_t length)
{
    fs_sector_t sector;

    memcpy(sector.bytes, data, length);

    if (length < FS_SECTOR_SIZE) {
        memset(sector.bytes + length, 0, FS_SECTOR_SIZE - length);
    }

    return dev_write(block_to_sector(block), &sector);
}


static int name_valid(const char* name)
{
    if (!name) {
        return 0;
    }

    size_t length = kstrlen(name);

    if (length == 0 || length > FS_NAME_MAX) {
        return 0;
    }

    for (size_t i = 0; i < length; i++) {

        unsigned char c = (unsigned char)name[i];

        if (c <= 0x20 || c >= 0x7F || c == '/' || c == '\\') {
            return 0;
        }
    }

    return 1;
}

static int name_equals(const char* stored, const char* name)
{
    for (uint32_t i = 0; i <= FS_NAME_MAX; i++) {

        if (stored[i] != name[i]) {
            return 0;
        }

        if (name[i] == '\0') {
            return 1;
        }
    }

    return 0;
}

static int dir_load(uint32_t index, fs_dirent_t* entry)
{
    fs_sector_t sector;
    int r = dev_read(fs_sb.dir_start + index / FS_DIRENTS_PER_SEC, &sector);

    if (r != FS_OK) {
        return r;
    }

    *entry = sector.dir[index % FS_DIRENTS_PER_SEC];
    return FS_OK;
}

static int dir_store(uint32_t index, const fs_dirent_t* entry)
{
    fs_sector_t sector;
    uint32_t lba = fs_sb.dir_start + index / FS_DIRENTS_PER_SEC;
    int r = dev_read(lba, &sector);

    if (r != FS_OK) {
        return r;
    }

    sector.dir[index % FS_DIRENTS_PER_SEC] = *entry;
    return dev_write(lba, &sector);
}

static int dir_find(const char* name, fs_dirent_t* entry, uint32_t* index)
{
    fs_sector_t sector;

    for (uint32_t s = 0; s < fs_sb.dir_sectors; s++) {

        int r = dev_read(fs_sb.dir_start + s, &sector);

        if (r != FS_OK) {
            return r;
        }

        for (uint32_t i = 0; i < FS_DIRENTS_PER_SEC; i++) {

            if (sector.dir[i].used && name_equals(sector.dir[i].name, name)) {
                *entry = sector.dir[i];
                *index = s * FS_DIRENTS_PER_SEC + i;
                return FS_OK;
            }
        }
    }

    return FS_ERR_NOT_FOUND;
}

static int dir_find_free(uint32_t* index)
{
    fs_sector_t sector;

    for (uint32_t s = 0; s < fs_sb.dir_sectors; s++) {

        int r = dev_read(fs_sb.dir_start + s, &sector);

        if (r != FS_OK) {
            return r;
        }

        for (uint32_t i = 0; i < FS_DIRENTS_PER_SEC; i++) {

            if (!sector.dir[i].used) {
                *index = s * FS_DIRENTS_PER_SEC + i;
                return FS_OK;
            }
        }
    }

    return FS_ERR_DIR_FULL;
}

static void dirent_init(fs_dirent_t* entry, const char* name)
{
    memset(entry, 0, sizeof(*entry));
    kstrlcpy(entry->name, name, sizeof(entry->name));
    entry->used = 1;
}

static int dir_find_or_prepare(const char* name, fs_dirent_t* entry,
                               uint32_t* index, int* existed)
{
    int r = dir_find(name, entry, index);

    if (r == FS_OK) {
        *existed = 1;
        return FS_OK;
    }

    if (r != FS_ERR_NOT_FOUND) {
        return r;
    }

    r = dir_find_free(index);

    if (r != FS_OK) {
        return r;
    }

    dirent_init(entry, name);
    *existed = 0;
    return FS_OK;
}

int fs_format(block_device_t* dev)
{
    if (!dev || !dev->read || !dev->write) {
        return FS_ERR_BAD_ARG;
    }

    uint32_t total = dev->sector_count;

    if (total > FS_MAX_SECTORS) {
        total = FS_MAX_SECTORS;
    }

    if (total < FS_MIN_SECTORS) {
        return FS_ERR_NO_SPACE;
    }

    uint32_t fat_sectors = (total * 2u + FS_SECTOR_SIZE - 1u) / FS_SECTOR_SIZE;
    uint32_t overhead    = 1u + fat_sectors + FS_DIR_SECTORS;

    if (total <= overhead) {
        return FS_ERR_NO_SPACE;
    }

    fs_sector_t sector;
    memset(&sector, 0, sizeof(sector));

    for (uint32_t s = 1; s < overhead; s++) {

        if (!dev->write(dev, s, 1, &sector)) {
            return FS_ERR_IO;
        }
    }

    sector.super.magic = FS_MAGIC;
    sector.super.version = FS_VERSION;
    sector.super.total_sectors = total;
    sector.super.fat_start = 1u;
    sector.super.fat_sectors = fat_sectors;
    sector.super.dir_start = 1u + fat_sectors;
    sector.super.dir_sectors = FS_DIR_SECTORS;
    sector.super.data_start = overhead;
    sector.super.data_blocks = total - overhead;

    if (!dev->write(dev, 0, 1, &sector)) {
        return FS_ERR_IO;
    }

    return fs_mount(dev);
}

int fs_mount(block_device_t* dev)
{
    if (!dev || !dev->read || !dev->write) {
        return FS_ERR_BAD_ARG;
    }

    fs_sector_t sector;

    if (!dev->read(dev, 0, 1, &sector)) {
        return FS_ERR_IO;
    }

    const fs_super_t* sb = &sector.super;

    if (sb->magic != FS_MAGIC || sb->version != FS_VERSION) {
        return FS_ERR_BAD_FS;
    }

    if (sb->total_sectors < FS_MIN_SECTORS ||
        sb->total_sectors > FS_MAX_SECTORS ||
        sb->total_sectors > dev->sector_count ||
        sb->fat_start != 1u ||
        sb->fat_sectors == 0 || sb->fat_sectors > 256u ||
        sb->dir_start != sb->fat_start + sb->fat_sectors ||
        sb->dir_sectors != FS_DIR_SECTORS ||
        sb->data_start != sb->dir_start + sb->dir_sectors ||
        sb->data_blocks == 0 || sb->data_blocks > 0xFFFEu ||
        sb->data_start + sb->data_blocks > sb->total_sectors ||
        sb->fat_sectors * FAT_ENTRIES_PER_SEC < sb->data_blocks + 1u) {
        return FS_ERR_BAD_FS;
    }

    fs_sb  = *sb;
    fs_dev = dev;

    return FS_OK;
}

void fs_unmount(void)
{
    fs_dev = 0;
}

int fs_is_mounted(void)
{
    return fs_dev != 0;
}

int fs_create(const char* name)
{
    if (!fs_dev) {
        return FS_ERR_NOT_MOUNTED;
    }

    if (!name_valid(name)) {
        return FS_ERR_BAD_NAME;
    }

    fs_dirent_t entry;
    uint32_t index;
    int existed;

    int r = dir_find_or_prepare(name, &entry, &index, &existed);

    if (r != FS_OK) {
        return r;
    }

    if (existed) {
        return FS_ERR_EXISTS;
    }

    return dir_store(index, &entry);
}

static void write_abort(int existed, uint32_t index, fs_dirent_t* entry,
                        uint16_t new_first)
{
    free_chain(new_first);

    if (existed) {
        entry->first_block = 0;
        entry->size = 0;
        dir_store(index, entry);
    }
}

int fs_write(const char* name, const void* data, uint32_t length)
{
    if (!fs_dev) {
        return FS_ERR_NOT_MOUNTED;
    }

    if (!name_valid(name)) {
        return FS_ERR_BAD_NAME;
    }

    if (length != 0 && !data) {
        return FS_ERR_BAD_ARG;
    }

    fs_dirent_t entry;
    uint32_t index;
    int existed;

    int r = dir_find_or_prepare(name, &entry, &index, &existed);

    if (r != FS_OK) {
        return r;
    }

    uint32_t needed = blocks_for(length);
    uint32_t reusable = 0;
    uint32_t free_blocks = 0;

    if (existed) {
        r = chain_stats(entry.first_block, &reusable, 0);

        if (r != FS_OK) {
            return r;
        }
    }

    r = count_free_blocks(&free_blocks);

    if (r != FS_OK) {
        return r;
    }

    if (needed > free_blocks + reusable) {
        return FS_ERR_NO_SPACE;
    }

    if (existed) {
        r = free_chain(entry.first_block);

        entry.first_block = 0;
        entry.size = 0;

        if (r != FS_OK) {
            dir_store(index, &entry);
            return r;
        }
    }

    const uint8_t* source = (const uint8_t*)data;
    uint32_t remaining = length;
    uint16_t first = 0;
    uint16_t previous = 0;

    while (remaining > 0) {

        uint16_t block = 0;
        uint32_t chunk = remaining < FS_SECTOR_SIZE ? remaining : FS_SECTOR_SIZE;

        r = fat_alloc(&block);

        if (r == FS_OK) {
            r = write_block(block, source, chunk);

            if (r == FS_OK && previous != 0) {
                r = fat_set(previous, block);
            }

            if (r != FS_OK) {
                fat_set(block, FAT_FREE);
            }
        }

        if (r != FS_OK) {
            write_abort(existed, index, &entry, first);
            return r;
        }

        if (first == 0) {
            first = block;
        }

        previous = block;
        source += chunk;
        remaining -= chunk;
    }

    entry.first_block = first;
    entry.size = length;

    r = dir_store(index, &entry);

    if (r != FS_OK) {
        write_abort(existed, index, &entry, first);
    }

    return r;
}

static void append_undo(uint16_t tail, uint16_t new_first,
                        uint32_t used_in_last, uint32_t top_up)
{
    free_chain(new_first);

    if (tail == 0) {
        return;
    }

    fat_set(tail, FAT_EOC);

    if (top_up > 0) {

        fs_sector_t sector;
        uint32_t lba = block_to_sector(tail);

        if (dev_read(lba, &sector) == FS_OK) {
            memset(sector.bytes + used_in_last, 0, top_up);
            dev_write(lba, &sector);
        }
    }
}

int fs_append(const char* name, const void* data, uint32_t length)
{
    if (!fs_dev) {
        return FS_ERR_NOT_MOUNTED;
    }

    if (!name_valid(name)) {
        return FS_ERR_BAD_NAME;
    }

    if (length != 0 && !data) {
        return FS_ERR_BAD_ARG;
    }

    fs_dirent_t entry;
    uint32_t index;
    int existed;

    int r = dir_find_or_prepare(name, &entry, &index, &existed);

    if (r != FS_OK) {
        return r;
    }

    if (length == 0) {
        return existed ? FS_OK : dir_store(index, &entry);
    }

    uint32_t old_size = entry.size;

    if (length > 0xFFFFFFFFu - old_size) {
        return FS_ERR_NO_SPACE;
    }

    uint32_t used_in_last = old_size % FS_SECTOR_SIZE;
    uint32_t room = (old_size != 0 && used_in_last != 0)
                    ? FS_SECTOR_SIZE - used_in_last : 0;
    uint32_t top_up = length < room ? length : room;
    uint32_t extra_blocks = blocks_for(length - top_up);
    uint32_t free_blocks = 0;

    r = count_free_blocks(&free_blocks);

    if (r != FS_OK) {
        return r;
    }

    if (extra_blocks > free_blocks) {
        return FS_ERR_NO_SPACE;
    }

    uint32_t chain_length = 0;
    uint16_t tail = 0;

    r = chain_stats(entry.first_block, &chain_length, &tail);

    if (r != FS_OK) {
        return r;
    }

    if (chain_length != blocks_for(old_size)) {
        return FS_ERR_CORRUPT;
    }

    const uint8_t* source = (const uint8_t*)data;

    if (top_up > 0) {

        fs_sector_t sector;
        uint32_t lba = block_to_sector(tail);

        r = dev_read(lba, &sector);

        if (r != FS_OK) {
            return r;
        }

        memcpy(sector.bytes + used_in_last, source, top_up);

        r = dev_write(lba, &sector);

        if (r != FS_OK) {
            return r;
        }

        source += top_up;
    }

    uint32_t remaining = length - top_up;
    uint16_t new_first = 0;
    uint16_t last = tail;

    while (remaining > 0) {

        uint16_t block = 0;
        uint32_t chunk = remaining < FS_SECTOR_SIZE ? remaining : FS_SECTOR_SIZE;

        r = fat_alloc(&block);

        if (r == FS_OK) {
            r = write_block(block, source, chunk);

            if (r == FS_OK && last != 0) {
                r = fat_set(last, block);
            }

            if (r != FS_OK) {
                fat_set(block, FAT_FREE);
            }
        }

        if (r != FS_OK) {
            append_undo(tail, new_first, used_in_last, top_up);
            return r;
        }

        if (new_first == 0) {
            new_first = block;

            if (entry.first_block == 0) {
                entry.first_block = block;
            }
        }

        last = block;
        source += chunk;
        remaining -= chunk;
    }

    entry.size = old_size + length;

    r = dir_store(index, &entry);

    if (r != FS_OK) {
        append_undo(tail, new_first, used_in_last, top_up);
    }

    return r;
}

int fs_read(const char* name, uint32_t offset, void* buffer, uint32_t length)
{
    if (!fs_dev) {
        return FS_ERR_NOT_MOUNTED;
    }

    if (!name_valid(name)) {
        return FS_ERR_BAD_NAME;
    }

    if (length != 0 && !buffer) {
        return FS_ERR_BAD_ARG;
    }

    fs_dirent_t entry;
    uint32_t index;

    int r = dir_find(name, &entry, &index);

    if (r != FS_OK) {
        return r;
    }

    if (offset >= entry.size || length == 0) {
        return 0;
    }

    uint32_t total = entry.size - offset;

    if (total > length) {
        total = length;
    }

    uint16_t block = entry.first_block;

    for (uint32_t skip = offset / FS_SECTOR_SIZE; skip > 0; skip--) {

        uint16_t next;

        r = chain_next(block, &next);

        if (r != FS_OK) {
            return r;
        }

        block = next;
    }

    uint32_t in_block = offset % FS_SECTOR_SIZE;
    uint32_t done = 0;
    uint8_t* destination = (uint8_t*)buffer;

    while (done < total) {

        fs_sector_t sector;
        uint16_t next = 0;

        if (!block_valid(block)) {
            return FS_ERR_CORRUPT;
        }

        r = dev_read(block_to_sector(block), &sector);

        if (r != FS_OK) {
            return r;
        }

        uint32_t chunk = FS_SECTOR_SIZE - in_block;

        if (chunk > total - done) {
            chunk = total - done;
        }

        memcpy(destination + done, sector.bytes + in_block, chunk);

        done += chunk;
        in_block = 0;

        if (done < total) {
            r = chain_next(block, &next);

            if (r != FS_OK) {
                return r;
            }

            block = next;
        }
    }

    return (int)total;
}

int fs_delete(const char* name)
{
    if (!fs_dev) {
        return FS_ERR_NOT_MOUNTED;
    }

    if (!name_valid(name)) {
        return FS_ERR_BAD_NAME;
    }

    fs_dirent_t entry;
    uint32_t index;

    int r = dir_find(name, &entry, &index);

    if (r != FS_OK) {
        return r;
    }

    uint16_t first = entry.first_block;

    memset(&entry, 0, sizeof(entry));

    r = dir_store(index, &entry);

    if (r != FS_OK) {
        return r;
    }

    r = free_chain(first);

    return (r == FS_ERR_CORRUPT) ? FS_OK : r;
}

int fs_rename(const char* old_name, const char* new_name)
{
    if (!fs_dev) {
        return FS_ERR_NOT_MOUNTED;
    }

    if (!name_valid(old_name) || !name_valid(new_name)) {
        return FS_ERR_BAD_NAME;
    }

    fs_dirent_t entry;
    fs_dirent_t other;
    uint32_t index;
    uint32_t other_index;

    int r = dir_find(old_name, &entry, &index);

    if (r != FS_OK) {
        return r;
    }

    r = dir_find(new_name, &other, &other_index);

    if (r == FS_OK) {
        return other_index == index ? FS_OK : FS_ERR_EXISTS;
    }

    if (r != FS_ERR_NOT_FOUND) {
        return r;
    }

    memset(entry.name, 0, sizeof(entry.name));
    kstrlcpy(entry.name, new_name, sizeof(entry.name));

    return dir_store(index, &entry);
}

int fs_stat(const char* name, uint32_t* size)
{
    if (!fs_dev) {
        return FS_ERR_NOT_MOUNTED;
    }

    if (!name_valid(name)) {
        return FS_ERR_BAD_NAME;
    }

    fs_dirent_t entry;
    uint32_t index;

    int r = dir_find(name, &entry, &index);

    if (r != FS_OK) {
        return r;
    }

    if (size) {
        *size = entry.size;
    }

    return FS_OK;
}

int fs_readdir(uint32_t* cursor, char* name_out, uint32_t* size_out)
{
    if (!fs_dev) {
        return FS_ERR_NOT_MOUNTED;
    }

    if (!cursor || !name_out || !size_out) {
        return FS_ERR_BAD_ARG;
    }

    while (*cursor < FS_MAX_FILES) {

        fs_dirent_t entry;
        int r = dir_load(*cursor, &entry);

        (*cursor)++;

        if (r != FS_OK) {
            return r;
        }

        if (entry.used) {
            memcpy(name_out, entry.name, FS_NAME_MAX);
            name_out[FS_NAME_MAX] = '\0';
            *size_out = entry.size;
            return 1;
        }
    }

    return 0;
}

int fs_get_info(fs_info_t* info)
{
    if (!fs_dev) {
        return FS_ERR_NOT_MOUNTED;
    }

    if (!info) {
        return FS_ERR_BAD_ARG;
    }

    uint32_t free_blocks = 0;
    uint32_t files = 0;
    fs_sector_t sector;

    int r = count_free_blocks(&free_blocks);

    if (r != FS_OK) {
        return r;
    }

    for (uint32_t s = 0; s < fs_sb.dir_sectors; s++) {

        r = dev_read(fs_sb.dir_start + s, &sector);

        if (r != FS_OK) {
            return r;
        }

        for (uint32_t i = 0; i < FS_DIRENTS_PER_SEC; i++) {

            if (sector.dir[i].used) {
                files++;
            }
        }
    }

    info->block_size = FS_SECTOR_SIZE;
    info->total_blocks = fs_sb.data_blocks;
    info->free_blocks = free_blocks;
    info->file_count = files;
    info->max_files = FS_MAX_FILES;

    return FS_OK;
}
