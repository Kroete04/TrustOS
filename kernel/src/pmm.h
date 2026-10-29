
#ifndef TRUSTOS_PMM_H
#define TRUSTOS_PMM_H

#include <stdint.h>
#include <limine.h>

#define PAGE_SIZE 4096

void pmm_init(
    struct limine_memmap_response *memmap,
    uint64_t hhdm_offset
);

uint64_t pmm_alloc_page(void);
void pmm_free_page(uint64_t physical_address);

uint64_t pmm_get_free_pages(void);

#endif