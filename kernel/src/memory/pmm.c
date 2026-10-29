
#include "pmm.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static uint8_t *used_bitmap;
static uint8_t *allocated_bitmap;

static uint64_t page_count;
static uint64_t free_pages;
static uint64_t next_page;

static bool initialized = false;

static bool bit_get(const uint8_t *bitmap, uint64_t page) {
    return (bitmap[page / 8] >> (page % 8)) & 1u;
}

static void bit_set(uint8_t *bitmap, uint64_t page) {
    bitmap[page / 8] |= (uint8_t)(1u << (page % 8));
}

static void bit_clear(uint8_t *bitmap, uint64_t page) {
    bitmap[page / 8] &= (uint8_t)~(1u << (page % 8));
}

static uint64_t align_down(uint64_t value) {
    return value & ~(PAGE_SIZE - 1);
}

static bool align_up_checked(uint64_t value, uint64_t *out) {
    if (value > UINT64_MAX - (PAGE_SIZE - 1)) {
        return false;
    }

    *out = (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    return true;
}

bool pmm_init(
    struct limine_memmap_response *memmap,
    uint64_t hhdm_offset
) {
    initialized = false;
    used_bitmap = NULL;
    allocated_bitmap = NULL;
    page_count = 0;
    free_pages = 0;
    next_page = 1;

    if (memmap == NULL || memmap->entry_count == 0) {
        return false;
    }

    uint64_t highest_address = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry == NULL ||
            entry->base > UINT64_MAX - entry->length) {
            return false;
        }

        uint64_t end = entry->base + entry->length;

        if (end > highest_address) {
            highest_address = end;
        }
    }

    if (highest_address == 0 ||
        highest_address > UINT64_MAX - (PAGE_SIZE - 1)) {
        return false;
    }

    uint64_t aligned_highest;
    if (!align_up_checked(highest_address, &aligned_highest)) {
        return false;
    }

    page_count = aligned_highest / PAGE_SIZE;

    if (page_count <= 1 || page_count > UINT64_MAX - 7) {
        return false;
    }

    uint64_t bitmap_bytes = (page_count + 7) / 8;

    if (bitmap_bytes > UINT64_MAX / 2) {
        return false;
    }

    uint64_t total_bitmap_bytes = bitmap_bytes * 2;
    uint64_t bitmap_storage_size;

    if (!align_up_checked(total_bitmap_bytes,
                          &bitmap_storage_size)) {
        return false;
    }

    uint64_t bitmap_physical = 0;
    bool found = false;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uint64_t start;

        if (!align_up_checked(entry->base, &start)) {
            continue;
        }

        uint64_t end =
            align_down(entry->base + entry->length);

        if (start < end &&
            end - start >= bitmap_storage_size) {
            bitmap_physical = start;
            found = true;
            break;
        }
    }

    if (!found ||
        bitmap_physical > UINT64_MAX - hhdm_offset) {
        return false;
    }

    uint8_t *storage = (uint8_t *)(uintptr_t)(
        bitmap_physical + hhdm_offset
    );

    used_bitmap = storage;
    allocated_bitmap = storage + bitmap_bytes;

    for (uint64_t i = 0; i < bitmap_bytes; i++) {
        used_bitmap[i] = 0xFF;
        allocated_bitmap[i] = 0;
    }

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uint64_t start_address;

        if (!align_up_checked(entry->base, &start_address)) {
            continue;
        }

        uint64_t start = start_address / PAGE_SIZE;
        uint64_t end =
            align_down(entry->base + entry->length) / PAGE_SIZE;

        for (uint64_t page = start; page < end; page++) {
            if (bit_get(used_bitmap, page)) {
                bit_clear(used_bitmap, page);
                free_pages++;
            }
        }
    }

    uint64_t bitmap_start = bitmap_physical / PAGE_SIZE;
    uint64_t bitmap_pages = bitmap_storage_size / PAGE_SIZE;

    for (uint64_t i = 0; i < bitmap_pages; i++) {
        uint64_t page = bitmap_start + i;

        if (!bit_get(used_bitmap, page)) {
            bit_set(used_bitmap, page);
            free_pages--;
        }
    }

    // Physische Seite 0 ist unser Fehlerwert.
    if (!bit_get(used_bitmap, 0)) {
        bit_set(used_bitmap, 0);
        free_pages--;
    }

    initialized = true;
    return true;
}

uint64_t pmm_alloc_page(void) {
    if (!initialized || free_pages == 0) {
        return 0;
    }

    for (uint64_t scanned = 0;
         scanned < page_count;
         scanned++) {

        uint64_t page = next_page;

        next_page++;
        if (next_page >= page_count) {
            next_page = 1;
        }

        if (!bit_get(used_bitmap, page)) {
            bit_set(used_bitmap, page);
            bit_set(allocated_bitmap, page);

            free_pages--;
            return page * PAGE_SIZE;
        }
    }

    return 0;
}

bool pmm_free_page(uint64_t physical_address) {
    if (!initialized ||
        physical_address == 0 ||
        physical_address % PAGE_SIZE != 0) {
        return false;
    }

    uint64_t page = physical_address / PAGE_SIZE;

    if (page >= page_count) {
        return false;
    }

    if (!bit_get(allocated_bitmap, page)) {
        return false;
    }

    if (!bit_get(used_bitmap, page)) {
        return false;
    }

    bit_clear(allocated_bitmap, page);
    bit_clear(used_bitmap, page);

    free_pages++;
    return true;
}

uint64_t pmm_get_free_pages(void) {
    return free_pages;
}
