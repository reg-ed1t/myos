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

static int heap_grow(uint32_t minimum_size)
{
    uint32_t required =
            align_up(
                    minimum_size + sizeof(heap_block_t),
                    HEAP_PAGE_SIZE
            );

    if (heap_current_end + required > KERNEL_HEAP_END) {
        return 0;
    }

    uint32_t start = heap_current_end;

    for (uint32_t offset = 0;
         offset < required;
         offset += HEAP_PAGE_SIZE) {

        void* physical = pmm_alloc_block();

        if (!physical) {
            return 0;
        }

        if (!map_page(
                physical,
                (void*)(start + offset),
                PAGE_PRESENT | PAGE_RW
        )) {
            pmm_free_block(physical);
            return 0;
        }
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

    /*
     * Start with one page.
     */
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

    uint32_t address = (uint32_t)ptr;

    if (address < KERNEL_HEAP_START ||
        address >= KERNEL_HEAP_END) {
        kprint("HEAP: invalid pointer\n");
        return;
    }

    heap_block_t* block =
            (heap_block_t*)(address -
                            sizeof(heap_block_t));

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
