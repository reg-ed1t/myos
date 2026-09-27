#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "vga.h"

#define HEAP_PAGE_SIZE 4096

typedef struct heap_block {
    uint32_t size;
    uint32_t free;

    struct heap_block* next;
    struct heap_block* prev;
} heap_block_t;

static heap_block_t* heap_head = 0;

static uint32_t heap_current_end = KERNEL_HEAP_START;

static uint32_t align_up(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static void split_block(heap_block_t* block, uint32_t size)
{
    if (block->size <= size + sizeof(heap_block_t) + 16) {
        return;
    }

    heap_block_t* new_block =
            (heap_block_t*)((uint32_t)block +
                            sizeof(heap_block_t) +
                            size);

    new_block->size =
            block->size - size - sizeof(heap_block_t);

    new_block->free = 1;

    new_block->next = block->next;
    new_block->prev = block;

    if (new_block->next) {
        new_block->next->prev = new_block;
    }

    block->next = new_block;
    block->size = size;
}

static void merge_with_next(heap_block_t* block)
{
    heap_block_t* next = block->next;

    if (!next || !next->free) {
        return;
    }

    block->size +=
            sizeof(heap_block_t) + next->size;

    block->next = next->next;

    if (block->next) {
        block->next->prev = block;
    }
}

static void heap_rollback(uint32_t start, uint32_t page_count)
{
    for (uint32_t i = page_count; i > 0; i--) {
        uint32_t virtual_address =
                start + (i - 1) * HEAP_PAGE_SIZE;

        uint32_t pd_idx =
                PAGE_DIRECTORY_INDEX(virtual_address);

        uint32_t pt_idx =
                PAGE_TABLE_INDEX(virtual_address);

        uint32_t* pd =
                (uint32_t*)VMM_PAGE_DIR_BASE;

        if (!(pd[pd_idx] & PAGE_PRESENT)) {
            continue;
        }

        uint32_t* pt =
                (uint32_t*)(VMM_PAGE_TABLE_BASE + pd_idx * 4096);

        uint32_t entry = pt[pt_idx];

        if (!(entry & PAGE_PRESENT)) {
            continue;
        }

        void* physical =
                (void*)(entry & ~0xFFFU);

        if (unmap_page((void*)virtual_address)) {
            pmm_free_block(physical);
        }
    }
}

static int heap_grow(uint32_t minimum_size)
{
    if (minimum_size > 0xFFFFFFFFU - sizeof(heap_block_t)) {
        return 0;
    }

    uint32_t required =
            align_up(
                    minimum_size + sizeof(heap_block_t),
                    HEAP_PAGE_SIZE
            );

    if (required < minimum_size ||
        heap_current_end > KERNEL_HEAP_END ||
        required > KERNEL_HEAP_END - heap_current_end) {
        return 0;
    }

    uint32_t start = heap_current_end;
    uint32_t mapped_pages = 0;

    for (uint32_t offset = 0;
         offset < required;
         offset += HEAP_PAGE_SIZE) {

        void* physical = pmm_alloc_block();

        if (!physical) {
            heap_rollback(start, mapped_pages);
            return 0;
        }

        if (!map_page(
                physical,
                (void*)(start + offset),
                PAGE_PRESENT | PAGE_RW
        )) {
            pmm_free_block(physical);
            heap_rollback(start, mapped_pages);
            return 0;
        }

        mapped_pages++;
    }

    heap_block_t* block =
            (heap_block_t*)start;

    block->size =
            required - sizeof(heap_block_t);

    block->free = 1;
    block->next = 0;
    block->prev = 0;

    heap_current_end += required;

    if (!heap_head) {
        heap_head = block;
        return 1;
    }

    heap_block_t* last = heap_head;

    while (last->next) {
        last = last->next;
    }

    last->next = block;
    block->prev = last;

    if (last->free) {
        merge_with_next(last);
    }

    return 1;
}

void heap_init(void)
{
    heap_head = 0;
    heap_current_end = KERNEL_HEAP_START;

    //Start with one page
    if (!heap_grow(HEAP_PAGE_SIZE)) {
        kprint("HEAP: failed to initialize\n");
        return;
    }

    kprint("Kernel heap online\n");
}

void* kmalloc(uint32_t size)
{
    if (size == 0) {
        return 0;
    }

    size = align_up(size, 8);

    heap_block_t* block = heap_head;

    while (block) {
        if (block->free && block->size >= size) {
            split_block(block, size);

            block->free = 0;

            return (void*)((uint32_t)block +
                           sizeof(heap_block_t));
        }

        block = block->next;
    }

    if (!heap_grow(size)) {
        return 0;
    }

    block = heap_head;

    while (block) {
        if (block->free && block->size >= size) {
            split_block(block, size);

            block->free = 0;

            return (void*)((uint32_t)block +
                           sizeof(heap_block_t));
        }

        block = block->next;
    }

    return 0;
}

void kfree(void* ptr)
{
    if (!ptr) {
        return;
    }

    heap_block_t* block = heap_head;

    while (block) {
        void* block_ptr =
                (void*)((uint32_t)block +
                        sizeof(heap_block_t));

        if (ptr == block_ptr) {
            break;
        }

        block = block->next;
    }

    if (!block) {
        kprint("HEAP: invalid pointer\n");
        return;
    }

    if (block->free) {
        kprint("HEAP: double free detected\n");
        return;
    }

    block->free = 1;

    if (block->next && block->next->free) {
        merge_with_next(block);
    }

    if (block->prev && block->prev->free) {
        merge_with_next(block->prev);
    }
}
