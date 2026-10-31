
#include "vmm.h"
#include "pmm.h"
#include "../arch/x86_64/paging.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#define PAGE_ENTRIES 512
#define PAGE_SIZE_2M (1ULL << 21)
#define VMM_HUGE_PAGE (1ULL << 7)

static uint64_t hhdm_offset;
static uint64_t pml4_physical;
static uint64_t reserved_virtual_base;
static uint16_t reserved_pml4_index;
static bool initialized = false;

static uint64_t *table_ptr(uint64_t physical) {
    return (uint64_t *)(uintptr_t)(physical + hhdm_offset);
}

static void clear_table(uint64_t physical) {
    uint64_t *table = table_ptr(physical);

    for (size_t i = 0; i < PAGE_ENTRIES; i++) {
        table[i] = 0;
    }
}

static uint64_t index_at(uint64_t address, unsigned shift) {
    return (address >> shift) & 0x1FFULL;
}

static bool table_is_empty(uint64_t *table) {
    for (size_t i = 0; i < PAGE_ENTRIES; i++) {
        if (table[i] & VMM_PRESENT) {
            return false;
        }
    }

    return true;
}

static bool address_allowed(uint64_t virtual_address) {
    return initialized &&
           index_at(virtual_address, 39) == reserved_pml4_index;
}

static void invalidate_page(uint64_t virtual_address) {
    __asm__ volatile(
        "invlpg (%0)"
        :
        : "r"((void *)(uintptr_t)virtual_address)
        : "memory"
    );
}

static uint64_t allocate_table(void) {
    uint64_t physical = pmm_alloc_page();

    if (physical != 0) {
        clear_table(physical);
    }

    return physical;
}

bool vmm_init(uint64_t offset) {
    initialized = false;
    hhdm_offset = offset;

    // This implementation requires 4-level paging.
    uint64_t cr4;

    __asm__ volatile(
        "mov %%cr4, %0"
        : "=r"(cr4)
    );

    if (cr4 & (1ULL << 12)) {
        return false;
    }

    pml4_physical = paging_get_pml4_address();

    if (pml4_physical == 0) {
        return false;
    }

    uint64_t *pml4 = table_ptr(pml4_physical);

    // Prefer an unused canonical high-half PML4 slot.
    for (uint16_t i = 256; i < 512; i++) {
        if (!(pml4[i] & VMM_PRESENT)) {
            reserved_pml4_index = i;

            reserved_virtual_base =
                0xFFFF000000000000ULL |
                ((uint64_t)i << 39);

            initialized = true;
            return true;
        }
    }

    return false;
}

uint64_t vmm_get_test_address(void) {
    return initialized ? reserved_virtual_base : 0;
}

bool vmm_map_page(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
) {
    if (!address_allowed(virtual_address) ||
        (virtual_address & (PAGE_SIZE - 1)) != 0 ||
        (physical_address & (PAGE_SIZE - 1)) != 0 ||
        physical_address == 0) {
        return false;
    }

    uint64_t *pml4 = table_ptr(pml4_physical);
    uint64_t pml4_index = reserved_pml4_index;

    uint64_t *tables[3];
    uint64_t physical_tables[3] = {0, 0, 0};
    uint64_t *parents[3];
    uint64_t parent_indices[3];

    uint64_t *current = pml4;
    unsigned shifts[3] = {39, 30, 21};

    for (size_t level = 0; level < 3; level++) {
        uint64_t index = index_at(virtual_address, shifts[level]);
        uint64_t entry = current[index];

        parents[level] = current;
        parent_indices[level] = index;

        if (entry & VMM_PRESENT) {
            if (entry & VMM_HUGE_PAGE) {
                goto rollback;
            }

            current = table_ptr(entry & PAGE_ADDR_MASK);
        } else {
            uint64_t new_table = allocate_table();

            if (new_table == 0) {
                goto rollback;
            }

            physical_tables[level] = new_table;

            current[index] =
                new_table | VMM_PRESENT | VMM_WRITABLE;

            current = table_ptr(new_table);
        }

        tables[level] = current;
    }

    uint64_t pt_index = index_at(virtual_address, 12);

    if (current[pt_index] & VMM_PRESENT) {
        goto rollback;
    }

    current[pt_index] =
        (physical_address & PAGE_ADDR_MASK) |
        (flags & (VMM_WRITABLE | VMM_NX)) |
        VMM_PRESENT;

    invalidate_page(virtual_address);
    return true;

rollback:
    // Remove only page tables created by this operation.
    for (int level = 2; level >= 0; level--) {
        if (physical_tables[level] != 0) {
            parents[level][parent_indices[level]] = 0;
            pmm_free_page(physical_tables[level]);
        }
    }

    invalidate_page(virtual_address);
    return false;
}

bool vmm_unmap_page(uint64_t virtual_address) {
    if (!address_allowed(virtual_address) ||
        (virtual_address & (PAGE_SIZE - 1)) != 0) {
        return false;
    }

    uint64_t *pml4 = table_ptr(pml4_physical);
    uint64_t i4 = reserved_pml4_index;

    uint64_t e4 = pml4[i4];

    if (!(e4 & VMM_PRESENT)) {
        return false;
    }

    uint64_t *pdpt = table_ptr(e4 & PAGE_ADDR_MASK);
    uint64_t i3 = index_at(virtual_address, 30);
    uint64_t e3 = pdpt[i3];

    if (!(e3 & VMM_PRESENT) || (e3 & VMM_HUGE_PAGE)) {
        return false;
    }

    uint64_t *pd = table_ptr(e3 & PAGE_ADDR_MASK);
    uint64_t i2 = index_at(virtual_address, 21);
    uint64_t e2 = pd[i2];

    if (!(e2 & VMM_PRESENT) || (e2 & VMM_HUGE_PAGE)) {
        return false;
    }

    uint64_t *pt = table_ptr(e2 & PAGE_ADDR_MASK);
    uint64_t i1 = index_at(virtual_address, 12);

    if (!(pt[i1] & VMM_PRESENT)) {
        return false;
    }

    pt[i1] = 0;
    invalidate_page(virtual_address);

    if (table_is_empty(pt)) {
        pd[i2] = 0;
        pmm_free_page(e2 & PAGE_ADDR_MASK);

        if (table_is_empty(pd)) {
            pdpt[i3] = 0;
            pmm_free_page(e3 & PAGE_ADDR_MASK);

            if (table_is_empty(pdpt)) {
                pml4[i4] = 0;
                pmm_free_page(e4 & PAGE_ADDR_MASK);
            }
        }
    }

    return true;
}
