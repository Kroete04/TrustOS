
#include "vmm.h"
#include "pmm.h"
#include "../arch/x86_64/paging.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#define PAGE_ENTRIES 512

static uint64_t hhdm_offset;
static uint64_t pml4_physical;
static bool initialized = false;

static uint64_t *physical_to_virtual(uint64_t physical) {
    return (uint64_t *)(uintptr_t)(physical + hhdm_offset);
}

static void zero_page(uint64_t physical) {
    uint64_t *page = physical_to_virtual(physical);

    for (size_t i = 0; i < PAGE_ENTRIES; i++) {
        page[i] = 0;
    }
}

static uint64_t get_index(uint64_t address, unsigned shift) {
    return (address >> shift) & 0x1FFULL;
}

static uint64_t *get_next_table(
    uint64_t *table,
    uint64_t index,
    bool create
) {
    uint64_t entry = table[index];

    if (entry & VMM_PRESENT) {
        // Large pages are not supported by this first version
        if (entry & (1ULL << 7)) {
            return NULL;
        }

        return physical_to_virtual(entry & PAGE_ADDR_MASK);
    }

    if (!create) {
        return NULL;
    }

    uint64_t new_table = pmm_alloc_page();

    if (new_table == 0) {
        return NULL;
    }

    zero_page(new_table);

    table[index] = new_table | VMM_PRESENT | VMM_WRITABLE;

    return physical_to_virtual(new_table);
}

bool vmm_init(uint64_t offset) {
    hhdm_offset = offset;
    pml4_physical = paging_get_pml4_address();

    initialized = (pml4_physical != 0);
    return initialized;
}

bool vmm_map_page(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
) {
    if (!initialized ||
        (virtual_address & (PAGE_SIZE - 1)) != 0 ||
        (physical_address & (PAGE_SIZE - 1)) != 0 ||
        physical_address == 0) {
        return false;
    }

    uint64_t *pml4 = physical_to_virtual(pml4_physical);

    uint64_t *pdpt = get_next_table(
        pml4, get_index(virtual_address, 39), true
    );

    if (pdpt == NULL) {
        return false;
    }

    uint64_t *pd = get_next_table(
        pdpt, get_index(virtual_address, 30), true
    );

    if (pd == NULL) {
        return false;
    }

    uint64_t *pt = get_next_table(
        pd, get_index(virtual_address, 21), true
    );

    if (pt == NULL) {
        return false;
    }

    uint64_t index = get_index(virtual_address, 12);

    // Never overwrite an existing mapping
    if (pt[index] & VMM_PRESENT) {
        return false;
    }

    pt[index] = (physical_address & PAGE_ADDR_MASK) |
                (flags & (VMM_WRITABLE | VMM_NX)) |
                VMM_PRESENT;

    __asm__ volatile(
        "invlpg (%0)"
        :
        : "r"((void *)(uintptr_t)virtual_address)
        : "memory"
    );

    return true;
}

bool vmm_unmap_page(uint64_t virtual_address) {
    if (!initialized ||
        (virtual_address & (PAGE_SIZE - 1)) != 0) {
        return false;
    }

    uint64_t *pml4 = physical_to_virtual(pml4_physical);

    uint64_t *pdpt = get_next_table(
        pml4, get_index(virtual_address, 39), false
    );

    if (pdpt == NULL) {
        return false;
    }

    uint64_t *pd = get_next_table(
        pdpt, get_index(virtual_address, 30), false
    );

    if (pd == NULL) {
        return false;
    }

    uint64_t *pt = get_next_table(
        pd, get_index(virtual_address, 21), false
    );

    if (pt == NULL) {
        return false;
    }

    uint64_t index = get_index(virtual_address, 12);

    if (!(pt[index] & VMM_PRESENT)) {
        return false;
    }

    pt[index] = 0;

    __asm__ volatile(
        "invlpg (%0)"
        :
        : "r"((void *)(uintptr_t)virtual_address)
        : "memory"
    );

    return true;
}