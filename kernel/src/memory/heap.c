
#include "heap.h"
#include "pmm.h"
#include "vmm.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define HEAP_PAGE_COUNT 16
#define HEAP_ALIGNMENT 16
#define HEAP_MAGIC 0x5452555354484541ULL

typedef struct heap_block {
    size_t size;
    struct heap_block *next;
    uint64_t magic;
    bool free;
} heap_block_t;

static heap_block_t *heap_head = NULL;
static bool heap_ready = false;

static uint64_t heap_base = 0;
static uint64_t heap_physical_pages[HEAP_PAGE_COUNT];

static size_t align_size(size_t size) {
    return (size + HEAP_ALIGNMENT - 1) &
           ~(HEAP_ALIGNMENT - 1);
}

static bool valid_block(heap_block_t *block) {
    if (block == NULL || heap_base == 0) {
        return false;
    }

    uintptr_t start = (uintptr_t)heap_base;
    uintptr_t end =
        start + HEAP_PAGE_COUNT * PAGE_SIZE;

    uintptr_t address = (uintptr_t)block;

    if (address < start ||
        address > end - sizeof(heap_block_t)) {
        return false;
    }

    if (block->magic != HEAP_MAGIC) {
        return false;
    }

    if (block->size >
        end - (address + sizeof(heap_block_t))) {
        return false;
    }

    return true;
}

bool heap_init(void) {
    if (heap_ready) {
        return true;
    }

    heap_base = vmm_get_test_address();

    if (heap_base == 0) {
        return false;
    }

    for (size_t i = 0; i < HEAP_PAGE_COUNT; i++) {
        heap_physical_pages[i] = 0;
    }

    size_t mapped_pages = 0;

    for (size_t i = 0; i < HEAP_PAGE_COUNT; i++) {
        uint64_t physical = pmm_alloc_page();

        if (physical == 0) {
            goto rollback;
        }

        uint64_t virtual_address =
            heap_base + i * PAGE_SIZE;

        if (!vmm_map_page(
            virtual_address,
            physical,
            VMM_WRITABLE | VMM_NX
        )) {
            pmm_free_page(physical);
            goto rollback;
        }

        heap_physical_pages[i] = physical;
        mapped_pages++;
    }

    heap_head = (heap_block_t *)(uintptr_t)heap_base;

    heap_head->size =
        HEAP_PAGE_COUNT * PAGE_SIZE -
        sizeof(heap_block_t);

    heap_head->next = NULL;
    heap_head->magic = HEAP_MAGIC;
    heap_head->free = true;

    heap_ready = true;
    return true;

rollback:
    for (size_t i = 0; i < mapped_pages; i++) {
        uint64_t virtual_address =
            heap_base + i * PAGE_SIZE;

        vmm_unmap_page(virtual_address);
        pmm_free_page(heap_physical_pages[i]);
        heap_physical_pages[i] = 0;
    }

    heap_head = NULL;
    heap_base = 0;
    heap_ready = false;

    return false;
}

void *kmalloc(size_t size) {
    if (!heap_ready || size == 0) {
        return NULL;
    }

    if (size > SIZE_MAX - (HEAP_ALIGNMENT - 1)) {
        return NULL;
    }

    size = align_size(size);

    heap_block_t *current = heap_head;

    while (current != NULL) {
        if (!valid_block(current)) {
            return NULL;
        }

        if (current->free && current->size >= size) {
            if (current->size >=
                size + sizeof(heap_block_t) +
                HEAP_ALIGNMENT) {

                heap_block_t *new_block =
                    (heap_block_t *)(
                        (uint8_t *)(current + 1) + size
                    );

                new_block->size =
                    current->size -
                    size -
                    sizeof(heap_block_t);

                new_block->next = current->next;
                new_block->magic = HEAP_MAGIC;
                new_block->free = true;

                current->size = size;
                current->next = new_block;
            }

            current->free = false;
            return (void *)(current + 1);
        }

        current = current->next;
    }

    return NULL;
}

void kfree(void *ptr) {
    if (!heap_ready || ptr == NULL) {
        return;
    }

    heap_block_t *current = heap_head;
    heap_block_t *previous = NULL;

    while (current != NULL) {
        if (!valid_block(current)) {
            return;
        }

        if ((void *)(current + 1) == ptr) {
            // Ignore double-free.
            if (current->free) {
                return;
            }

            current->free = true;

            // Merge following free blocks.
            while (current->next != NULL) {
                heap_block_t *next = current->next;

                if (!valid_block(next) || !next->free) {
                    break;
                }

                uintptr_t expected_next =
                    (uintptr_t)(current + 1) +
                    current->size;

                if ((uintptr_t)next != expected_next) {
                    break;
                }

                current->size +=
                    sizeof(heap_block_t) + next->size;

                current->next = next->next;
            }

            // Merge with previous block if adjacent.
            if (previous != NULL && previous->free) {
                uintptr_t expected_current =
                    (uintptr_t)(previous + 1) +
                    previous->size;

                if (expected_current ==
                    (uintptr_t)current) {

                    previous->size +=
                        sizeof(heap_block_t) +
                        current->size;

                    previous->next = current->next;
                }
            }

            return;
        }

        previous = current;
        current = current->next;
    }
}
