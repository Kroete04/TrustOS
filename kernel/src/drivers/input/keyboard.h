
#ifndef TRUSTOS_KEYBOARD_H
#define TRUSTOS_KEYBOARD_H

#include <stdbool.h>

typedef enum {
    KEYBOARD_EVENT_NONE,
    KEYBOARD_EVENT_CHAR,
    KEYBOARD_EVENT_BACKSPACE,
    KEYBOARD_EVENT_ENTER
} keyboard_event_type_t;

typedef struct {
    keyboard_event_type_t type;
    char character;
} keyboard_event_t;

bool keyboard_poll_event(keyboard_event_t *event);

#endif
