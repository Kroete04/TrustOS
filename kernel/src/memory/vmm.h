
#ifndef TRUSTOS_VMM_H
#define TRUSTOS_VMM_H

#include <stdint.h>
#include <stdbool.h>

#define VMM_PRESENT  (1ULL << 0)
#define VMM_WRITABLE (1ULL << 1)
#define VMM_USER     (1ULL << 2)
#define VMM_NX       (1ULL << 63)

// Initialize VMM using Limine's HHDM offset
bool vmm_init(uint64_t hhdm_offset);

// Map one 4 KiB physical page to a virtual address
bool vmm_map_page(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
);

// Remove a mapping without freeing its physical page
bool vmm_unmap_page(uint64_t virtual_address);

#endif
