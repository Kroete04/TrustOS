#ifndef TRUSTOS_INTERRUPTS_H
#define TRUSTOS_INTERRUPTS_H

#include <stdint.h>

void interrupts_init(void);
void exception_handler(uint64_t vector);

#endif
