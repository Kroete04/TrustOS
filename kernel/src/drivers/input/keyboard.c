
#include "keyboard.h"

#include <stdint.h>
#include <stdbool.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t value;

    __asm__ volatile(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static bool left_shift = false;
static bool right_shift = false;
static bool caps_lock = false;
static bool extended = false;

static const char german_normal[128] = {
    [0x02] = '1',
    [0x03] = '2',
    [0x04] = '3',
    [0x05] = '4',
    [0x06] = '5',
    [0x07] = '6',
    [0x08] = '7',
    [0x09] = '8',
    [0x0A] = '9',
    [0x0B] = '0',
    [0x0C] = '-',
    [0x0D] = '=',

    [0x10] = 'q',
    [0x11] = 'w',
    [0x12] = 'e',
    [0x13] = 'r',
    [0x14] = 't',
    [0x15] = 'z',
    [0x16] = 'u',
    [0x17] = 'i',
    [0x18] = 'o',
    [0x19] = 'p',

    [0x1E] = 'a',
    [0x1F] = 's',
    [0x20] = 'd',
    [0x21] = 'f',
    [0x22] = 'g',
    [0x23] = 'h',
    [0x24] = 'j',
    [0x25] = 'k',
    [0x26] = 'l',

    [0x2C] = 'y',
    [0x2D] = 'x',
    [0x2E] = 'c',
    [0x2F] = 'v',
    [0x30] = 'b',
    [0x31] = 'n',
    [0x32] = 'm',

    [0x33] = ',',
    [0x34] = '.',
    [0x35] = '-',
    [0x39] = ' '
};

static const char german_shifted[128] = {
    [0x02] = '!',
    [0x03] = '"',
    [0x04] = '#',
    [0x05] = '$',
    [0x06] = '%',
    [0x07] = '&',
    [0x08] = '/',
    [0x09] = '(',
    [0x0A] = ')',
    [0x0B] = '=',
    [0x0C] = '?',
    [0x0D] = '`',

    [0x33] = ';',
    [0x34] = ':',
    [0x35] = '_',
    [0x39] = ' '
};

static bool is_letter(char character) {
    return character >= 'a' && character <= 'z';
}

bool keyboard_poll_event(keyboard_event_t *event) {
    if (event == 0) {
        return false;
    }

    event->type = KEYBOARD_EVENT_NONE;
    event->character = '\0';

    uint8_t status = inb(0x64);

    if (!(status & 0x01)) {
        return false;
    }

    uint8_t scancode = inb(0x60);

    if (status & 0x20) {
        return false;
    }

    if (scancode == 0xE0) {
        extended = true;
        return false;
    }

    if (scancode == 0xE1) {
        extended = false;
        return false;
    }

    if (extended) {
        extended = false;
        return false;
    }

    bool released = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7F;

    if (key == 0x2A) {
        left_shift = !released;
        return false;
    }

    if (key == 0x36) {
        right_shift = !released;
        return false;
    }

    if (key == 0x3A) {
        if (!released) {
            caps_lock = !caps_lock;
        }

        return false;
    }

    if (released) {
        return false;
    }

    if (key == 0x0E) {
        event->type = KEYBOARD_EVENT_BACKSPACE;
        return true;
    }

    if (key == 0x1C) {
        event->type = KEYBOARD_EVENT_ENTER;
        event->character = '\n';
        return true;
    }

    char character = german_normal[key];
    bool shift = left_shift || right_shift;

    if (is_letter(character)) {
        if (shift != caps_lock) {
            character = character - 'a' + 'A';
        }
    } else if (shift && german_shifted[key] != '\0') {
        character = german_shifted[key];
    }

    if (character == '\0') {
        return false;
    }

    event->type = KEYBOARD_EVENT_CHAR;
    event->character = character;

    return true;
}
