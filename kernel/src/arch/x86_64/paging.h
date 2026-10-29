
#ifndef TRUSTOS_PAGING_H
#define TRUSTOS_PAGING_H

#include <stdint.h>

uint64_t paging_get_cr3(void);
uint64_t paging_get_pml4_address(void);

#endif
