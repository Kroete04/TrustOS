
#include "keyboard.h"

#include <stdint.h>
#include <stdbool.h>

#define KEYBOARD_QUEUE_SIZE 64

static inline uint8_t inb(uint16_t port) {
    uint8_t value;

    __asm__ volatile(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static bool keyboard_wait_input_ready(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (!(inb(0x64) & 0x02)) {
            return true;
        }
        __asm__ volatile("pause");
    }
    return false;
}

bool keyboard_enable_irq(void) {
    // Read PS/2 controller configuration byte
    if (!keyboard_wait_input_ready()) {
        return false;
    }
    outb(0x64, 0x20);

    // Wait for controller response
    bool ready = false;
    for (uint32_t i = 0; i < 100000; i++) {
        if (inb(0x64) & 0x01) {
            ready = true;
            break;
        }
        __asm__ volatile("pause");
    }
    if (!ready) {
        return false;
    }

    uint8_t config = inb(0x60);

    // Enable keyboard IRQ1
    config |= (1U << 0);

    // Enable PS/2 controller translation:
    // Scancode Set 2 -> Set 1
    config |= (1U << 6);

    // Enable first PS/2 port clock
    config &= (uint8_t)~(1U << 4);

    if (!keyboard_wait_input_ready()) {
        return false;
    }
    outb(0x64, 0x60);

    if (!keyboard_wait_input_ready()) {
        return false;
    }
    outb(0x60, config);

    return true;
}

static bool left_shift = false;
static bool right_shift = false;
static bool caps_lock = false;
static bool extended = false;
static bool altgr = false;
static bool left_ctrl = false;
static bool left_alt = false;

static const char german_normal[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3',
    [0x05] = '4', [0x06] = '5', [0x07] = '6',
    [0x08] = '7', [0x09] = '8', [0x0A] = '9',
    [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',

    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e',
    [0x13] = 'r', [0x14] = 't', [0x15] = 'z',
    [0x16] = 'u', [0x17] = 'i', [0x18] = 'o',
    [0x19] = 'p',

    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd',
    [0x21] = 'f', [0x22] = 'g', [0x23] = 'h',
    [0x24] = 'j', [0x25] = 'k', [0x26] = 'l',

    [0x2C] = 'y', [0x2D] = 'x', [0x2E] = 'c',
    [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n',
    [0x32] = 'm',

    [0x29] = '^',
    [0x1A] = 0,
    [0x1B] = '+',
    [0x27] = 0,
    [0x28] = '#',
    [0x2B] = '#',
    [0x33] = ',',
    [0x34] = '.',
    [0x35] = '-',
    [0x39] = ' ',
    [0x56] = '<'
};

static const char german_shifted[128] = {
    [0x02] = '!', [0x03] = '"', [0x04] = '#',
    [0x05] = '$', [0x06] = '%', [0x07] = '&',
    [0x08] = '/', [0x09] = '(', [0x0A] = ')',
    [0x0B] = '=', [0x0C] = '?', [0x0D] = '`',

    [0x29] = 0,
    [0x1B] = '*',
    [0x28] = '\'',
    [0x2B] = '\'',
    [0x33] = ';',
    [0x34] = ':',
    [0x35] = '_',
    [0x39] = ' ',
    [0x56] = '>'
};

static const char german_altgr[128] = {
    [0x03] = 0,
    [0x08] = '{',
    [0x09] = '[',
    [0x0A] = ']',
    [0x0B] = '}',
    [0x0C] = '\\',
    [0x10] = '@',
    [0x12] = 0,
    [0x1B] = '~',
    [0x56] = '|',
    [0x32] = 0
};

static bool is_letter(char character) {
    return character >= 'a' && character <= 'z';
}

static volatile uint8_t keyboard_queue[KEYBOARD_QUEUE_SIZE];
static volatile uint32_t queue_head = 0;
static volatile uint32_t queue_tail = 0;

void keyboard_irq_handler(void) {
    uint8_t status = inb(0x64);

    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t scancode = inb(0x60);

        uint32_t next =
            (queue_head + 1) % KEYBOARD_QUEUE_SIZE;

        if (next != queue_tail) {
            keyboard_queue[queue_head] = scancode;
            queue_head = next;
        }
    }

    // End of interrupt to the master PIC.
    outb(0x20, 0x20);
}

bool keyboard_poll_event(keyboard_event_t *event) {
    if (event == 0) {
        return false;
    }

    event->type = KEYBOARD_EVENT_NONE;
    event->character = '\0';


    uint32_t head;
    uint32_t tail;

    // Keep IRQ1 from modifying the queue during dequeue.
    __asm__ volatile("pushfq; cli" ::: "memory");
    tail = queue_tail;
    head = queue_head;

    if (tail == head) {
        __asm__ volatile("popfq" ::: "memory");
        return false;
    }

    uint8_t scancode = keyboard_queue[tail];
    queue_tail = (tail + 1) % KEYBOARD_QUEUE_SIZE;
    __asm__ volatile("popfq" ::: "memory");

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

    // Right Alt (AltGr) uses the extended scancode E0 38.
    if ((scancode & 0x7F) == 0x38) {
        altgr = (scancode & 0x80) == 0;
    }

    return false;
}

    bool released = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7F;

    // Left Ctrl
    if (key == 0x1D) {
        left_ctrl = !released;
        return false;
    }

    // Left Alt
    if (key == 0x38) {
        left_alt = !released;
        return false;
    }

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

bool shift = left_shift || right_shift;
char character;

if (altgr || (left_ctrl && left_alt)) {
    character = german_altgr[key];
} else {
    character = german_normal[key];

    if (is_letter(character)) {
        if (shift != caps_lock) {
            character = character - 'a' + 'A';
        }
    } else if (shift && german_shifted[key] != '\0') {
        character = german_shifted[key];
    }
}

    if (character == '\0') {
        return false;
    }

    event->type = KEYBOARD_EVENT_CHAR;
    event->character = character;

    return true;
}
