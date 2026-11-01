
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

    static const uint8_t lowercase[26][7] = {
        {0, 0, 14, 1, 15, 17, 15},  // a
        {16, 16, 30, 17, 17, 17, 30}, // b
        {0, 0, 14, 17, 16, 17, 14}, // c
        {1, 1, 15, 17, 17, 17, 15}, // d
        {0, 0, 14, 17, 31, 16, 14}, // e
        {6, 9, 8, 28, 8, 8, 8},     // f
        {0, 15, 17, 17, 15, 1, 14}, // g
        {16, 16, 30, 17, 17, 17, 17}, // h
        {4, 0, 12, 4, 4, 4, 14},    // i
        {2, 0, 6, 2, 2, 18, 12},    // j
        {16, 16, 18, 20, 24, 20, 18}, // k
        {12, 4, 4, 4, 4, 4, 14},    // l
        {0, 0, 26, 21, 21, 21, 21}, // m
        {0, 0, 30, 17, 17, 17, 17}, // n
        {0, 0, 14, 17, 17, 17, 14}, // o
        {0, 30, 17, 17, 30, 16, 16}, // p
        {0, 15, 17, 17, 15, 1, 1}, // q
        {0, 0, 22, 25, 16, 16, 16}, // r
        {0, 0, 15, 16, 14, 1, 30}, // s
        {8, 8, 28, 8, 8, 9, 6},    // t
        {0, 0, 17, 17, 17, 19, 13}, // u
        {0, 0, 17, 17, 17, 10, 4}, // v
        {0, 0, 17, 17, 21, 21, 10}, // w
        {0, 0, 17, 10, 4, 10, 17}, // x
        {0, 17, 17, 17, 15, 1, 14}, // y
        {0, 0, 31, 2, 4, 8, 31}    // z
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
    static const uint8_t dot[7]        = {0, 0, 0, 0, 0, 12, 12};
    static const uint8_t comma[7]      = {0, 0, 0, 0, 12, 12, 8};
    static const uint8_t colon[7]      = {0, 12, 12, 0, 12, 12, 0};
    static const uint8_t semicolon[7]  = {0, 12, 12, 0, 12, 12, 8};
    static const uint8_t dash[7]       = {0, 0, 0, 31, 0, 0, 0};
    static const uint8_t underscore[7] = {0, 0, 0, 0, 0, 0, 31};
    static const uint8_t bang[7]       = {4, 4, 4, 4, 4, 0, 4};
    static const uint8_t question[7]   = {14, 17, 1, 2, 4, 0, 4};

    static const uint8_t apostrophe[7] = {4, 4, 2, 0, 0, 0, 0};
    static const uint8_t quote[7]      = {10, 10, 0, 0, 0, 0, 0};
    static const uint8_t backtick[7]   = {8, 4, 0, 0, 0, 0, 0};

    static const uint8_t plus[7]       = {0, 4, 4, 31, 4, 4, 0};
    static const uint8_t equals[7]     = {0, 0, 31, 0, 31, 0, 0};
    static const uint8_t asterisk[7]   = {0, 21, 14, 31, 14, 21, 0};
    static const uint8_t slash[7]      = {1, 2, 2, 4, 8, 8, 16};
    static const uint8_t backslash[7]  = {16, 8, 8, 4, 2, 2, 1};
    static const uint8_t percent[7]    = {17, 18, 2, 4, 8, 9, 17};
    static const uint8_t ampersand[7]  = {12, 18, 20, 8, 21, 18, 13};
    static const uint8_t hash[7]       = {10, 10, 31, 10, 31, 10, 10};
    static const uint8_t dollar[7]     = {4, 15, 20, 14, 5, 30, 4};
    static const uint8_t at[7]         = {14, 17, 23, 21, 23, 16, 14};
    static const uint8_t caret[7]      = {4, 10, 17, 0, 0, 0, 0};
    static const uint8_t tilde[7]      = {0, 0, 9, 22, 0, 0, 0};

    static const uint8_t lparen[7]     = {2, 4, 8, 8, 8, 4, 2};
    static const uint8_t rparen[7]     = {8, 4, 2, 2, 2, 4, 8};
    static const uint8_t lbracket[7]   = {14, 8, 8, 8, 8, 8, 14};
    static const uint8_t rbracket[7]   = {14, 2, 2, 2, 2, 2, 14};
    static const uint8_t lbrace[7]     = {3, 4, 4, 8, 4, 4, 3};
    static const uint8_t rbrace[7]     = {24, 4, 4, 2, 4, 4, 24};

    static const uint8_t pipe[7]       = {4, 4, 4, 4, 4, 4, 4};
    static const uint8_t less[7]       = {2, 4, 8, 16, 8, 4, 2};
    static const uint8_t greater[7]    = {8, 4, 2, 1, 2, 4, 8};

    if (ch >= 'a' && ch <= 'z') {
        return lowercase[ch - 'a'];
    }

    if (ch >= 'A' && ch <= 'Z') {
        return letters[ch - 'A'];
    }

    if (ch >= '0' && ch <= '9') {
       return digits[ch - '0'];
    }

    switch (ch) {
        case ' ':  return space;
        case '.':  return dot;
        case ',':  return comma;
        case ':':  return colon;
        case ';':  return semicolon;
        case '-':  return dash;
        case '_':  return underscore;
        case '!':  return bang;
        case '?':  return question;
        case '\'': return apostrophe;
        case '"':  return quote;
        case '`':  return backtick;
        case '+':  return plus;
        case '=':  return equals;
        case '*':  return asterisk;
        case '/':  return slash;
        case '\\': return backslash;
        case '%':  return percent;
        case '&':  return ampersand;
        case '#':  return hash;
        case '$':  return dollar;
        case '@':  return at;
        case '^':  return caret;
        case '~':  return tilde;
        case '(':  return lparen;
        case ')':  return rparen;
        case '[':  return lbracket;
        case ']':  return rbracket;
        case '{':  return lbrace;
        case '}':  return rbrace;
        case '|':  return pipe;
        case '<':  return less;
        case '>':  return greater;
        default:   return question;
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
