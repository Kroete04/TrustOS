
#include "pmm.h"
#include <stddef.h>
#include <stdbool.h>

static uint8_t *bitmap;
static uint64_t page_count;
static uint64_t free_pages;
static uint64_t next_page;

static bool initialized;

static bool bit_get(uint64_t page) {
    return (bitmap[page / 8] >> (page % 8)) & 1;
}

static void bit_set(uint64_t page) {
    bitmap[page / 8] |= (uint8_t)(1u << (page % 8));
}

static void bit_clear(uint64_t page) {
    bitmap[page / 8] &= (uint8_t)~(1u << (page % 8));
}

static uint64_t align_up(uint64_t value) {
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static uint64_t align_down(uint64_t value) {
    return value & ~(PAGE_SIZE - 1);
}

void pmm_init(
    struct limine_memmap_response *memmap,
    uint64_t hhdm_offset
) {
    initialized = false;
    bitmap = NULL;
    page_count = 0;
    free_pages = 0;
    next_page = 0;

    // Get highest physical adress
    uint64_t highest_address = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->base > UINT64_MAX - entry->length) {
            return;
        }

        uint64_t end = entry->base + entry->length;

        if (end > highest_address) {
            highest_address = end;
        }
    }

    if (highest_address > UINT64_MAX - (PAGE_SIZE - 1)) {
        return;
    }

    page_count = align_up(highest_address) / PAGE_SIZE;

    if (page_count == 0 || page_count > UINT64_MAX - 7) {
        return;
    }

    uint64_t bitmap_bytes = (page_count + 7) / 8;

    if (bitmap_bytes > UINT64_MAX - (PAGE_SIZE - 1)) {
        return;
    }

    uint64_t bitmap_size = align_up(bitmap_bytes);

    // Find usable space for Bitmap
    uint64_t bitmap_physical = 0;
    bool found = false;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        if (entry->base > UINT64_MAX - (PAGE_SIZE - 1)) {
            continue;
        }

        uint64_t start = align_up(entry->base);
        uint64_t end = align_down(entry->base + entry->length);

        if (start < end && end - start >= bitmap_size) {
            bitmap_physical = start;
            found = true;
            break;
        }
    }

    if (!found || bitmap_physical > UINT64_MAX - hhdm_offset) {
        return;
    }

    bitmap = (uint8_t *)(uintptr_t)(
        bitmap_physical + hhdm_offset
    );

    // mark all pages as used
    for (uint64_t i = 0; i < bitmap_bytes; i++) {
        bitmap[i] = 0xFF;
    }

    // only allow usable, fully contained pages
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uint64_t start = align_up(entry->base) / PAGE_SIZE;
        uint64_t end =
            align_down(entry->base + entry->length) / PAGE_SIZE;

        for (uint64_t page = start; page < end; page++) {
            if (bit_get(page)) {
                bit_clear(page);
                free_pages++;
            }
        }
    }

    // the bitmap should not overwrite itself :P
    uint64_t bitmap_start = bitmap_physical / PAGE_SIZE;
    uint64_t bitmap_pages = bitmap_size / PAGE_SIZE;

    for (uint64_t i = 0; i < bitmap_pages; i++) {
        uint64_t page = bitmap_start + i;

        if (!bit_get(page)) {
            bit_set(page);
            free_pages--;
        }
    }

    if (page_count > 0 && !bit_get(0)) {
        bit_set(0);
        free_pages--;
    }

    next_page = 1;
    initialized = true;
}

uint64_t pmm_alloc_page(void) {
    if (!initialized || free_pages == 0) {
        return 0;
    }

    for (uint64_t scanned = 0; scanned < page_count; scanned++) {
        uint64_t page = next_page;

        next_page++;
        if (next_page == page_count) {
            next_page = 0;
        }

        if (!bit_get(page)) {
            bit_set(page);
            free_pages--;
            return page * PAGE_SIZE;
        }
    }

    return 0;
}

void pmm_free_page(uint64_t physical_address) {
    if (!initialized || physical_address % PAGE_SIZE != 0) {
        return;
    }

    uint64_t page = physical_address / PAGE_SIZE;

    if (page >= page_count || !bit_get(page)) {
        return;
    }

    // only pages from usable areas should be freed
    // this initial version also requires the caller
    // to exclusively free their own allocations

    bit_clear(page);
    free_pages++;
}

uint64_t pmm_get_free_pages(void) {
    return free_pages;
}