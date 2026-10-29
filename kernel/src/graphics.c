
#include "graphics.h"

#include <stddef.h>

static struct limine_framebuffer *screen;
static size_t cursor_x = 32;
static size_t cursor_y = 32;

static uint32_t channel(uint8_t value, uint8_t bits, uint8_t shift) {
    uint64_t max = ((uint64_t)1 << bits) - 1;
    return (uint32_t)((value * max / 255) << shift);
}

static uint32_t color(uint8_t r, uint8_t g, uint8_t b) {
    return channel(r, screen->red_mask_size, screen->red_mask_shift)
         | channel(g, screen->green_mask_size, screen->green_mask_shift)
         | channel(b, screen->blue_mask_size, screen->blue_mask_shift);
}

static void pixel(size_t x, size_t y, uint32_t c) { // Soll einzelnen Pixel setzen
    if (x >= screen->width || y >= screen->height) return;

    volatile uint32_t *row =
        (volatile uint32_t *)((volatile uint8_t *)screen->address
        + y * screen->pitch);

    row[x] = c;
}

static const uint8_t *glyph(char ch) {
    static const uint8_t letters[26][7] = {
        {14,17,17,31,17,17,17}, // A
        {30,17,17,30,17,17,30}, // B
        {14,17,16,16,16,17,14}, // C
        {30,17,17,17,17,17,30}, // D
        {31,16,16,30,16,16,31}, // E
        {31,16,16,30,16,16,16}, // F
        {14,17,16,23,17,17,14}, // G
        {17,17,17,31,17,17,17}, // H
        {31,4,4,4,4,4,31},       // I
        {7,2,2,2,18,18,12},     // J
        {17,18,20,24,20,18,17}, // K
        {16,16,16,16,16,16,31}, // L
        {17,27,21,21,17,17,17}, // M
        {17,25,21,19,17,17,17}, // N
        {14,17,17,17,17,17,14}, // O
        {30,17,17,30,16,16,16}, // P
        {14,17,17,17,21,18,13}, // Q
        {30,17,17,30,20,18,17}, // R
        {15,16,16,14,1,1,30},   // S
        {31,4,4,4,4,4,4},        // T
        {17,17,17,17,17,17,14}, // U
        {17,17,17,17,17,10,4},  // V
        {17,17,17,21,21,21,10}, // W
        {17,17,10,4,10,17,17},  // X
        {17,17,10,4,4,4,4},     // Y
        {31,1,2,4,8,16,31}     // Z
    };

    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14},
        {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2},
        {31,16,30,1,1,17,14},
        {6,8,16,30,17,17,14},
        {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14},
        {14,17,17,15,1,2,12}
    };

    static const uint8_t space[7] = {0};
    static const uint8_t dot[7]   = {0,0,0,0,0,12,12};
    static const uint8_t colon[7] = {0,12,12,0,12,12,0};
    static const uint8_t dash[7]  = {0,0,0,31,0,0,0};
    static const uint8_t bang[7]  = {4,4,4,4,4,0,4};
    static const uint8_t quote[7] = {10,10,0,0,0,0,0};
    static const uint8_t question[7] = {14,17,1,2,4,0,4};

    if (ch >= 'a' && ch <= 'z') ch -= 32;
    if (ch >= 'A' && ch <= 'Z') return letters[ch - 'A'];
    if (ch >= '0' && ch <= '9') return digits[ch - '0'];

    switch (ch) {
        case '.': return dot;
        case ':': return colon;
        case '-': return dash;
        case '!': return bang;
        case '\'': return quote;
        case ' ': return space;
        default: return question;
    }
}

static void draw_char(char ch, uint32_t c) { // Soll Buchstaben aus Pixeln zeichnen
    const uint8_t *bitmap = glyph(ch);
    const size_t scale = 3;

    for (size_t y = 0; y < 7; y++) {
        for (size_t x = 0; x < 5; x++) {
            if (bitmap[y] & (1u << (4 - x))) {
                for (size_t dy = 0; dy < scale; dy++) {
                    for (size_t dx = 0; dx < scale; dx++) {
                        pixel(cursor_x + x * scale + dx,
                              cursor_y + y * scale + dy, c);
                    }
                }
            }
        }
    }

    cursor_x += 6 * scale;
}

void graphics_init(struct limine_framebuffer *fb) { // Soll Framebuffer registrieren
    screen = fb;
    cursor_x = 32;
    cursor_y = 32;
}

void graphics_clear(uint8_t r, uint8_t g, uint8_t b) { // Soll Bildschirm löschen
    uint32_t c = color(r, g, b);

    for (size_t y = 0; y < screen->height; y++) {
        for (size_t x = 0; x < screen->width; x++) {
            pixel(x, y, c);
        }
    }

    cursor_x = 32;
    cursor_y = 32;
}

void graphics_print(const char *text, uint8_t r, uint8_t g, uint8_t b) { // Soll Zeichenketten ausgeben
    uint32_t c = color(r, g, b);

    while (*text) {
        if (*text == '\n') {
            cursor_x = 32;
            cursor_y += 30;
        } else {
            if (cursor_x + 18 > screen->width) {
                cursor_x = 32;
                cursor_y += 30;
            }

            draw_char(*text, c);
        }

        text++;
    }
}
