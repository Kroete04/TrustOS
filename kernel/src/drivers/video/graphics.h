#ifndef TRUSTOS_GRAPHICS_H
#define TRUSTOS_GRAPHICS_H

#include <limine.h>
#include <stdint.h>

void graphics_init(struct limine_framebuffer *fb);
void graphics_clear(uint8_t r, uint8_t g, uint8_t b);
void graphics_print(const char *text, uint8_t r, uint8_t g, uint8_t b);

#endif
