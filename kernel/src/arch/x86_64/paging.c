
#include "paging.h"

uint64_t paging_get_cr3(void) {
    uint64_t cr3;

    __asm__ volatile(
        "mov %%cr3, %0"
        : "=r"(cr3)
        :
        : "memory"
    );

    return cr3;
}

uint64_t paging_get_pml4_address(void) {
    return paging_get_cr3() & 0x000FFFFFFFFFF000ULL;
}
