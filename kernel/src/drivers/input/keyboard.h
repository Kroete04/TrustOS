
#ifndef TRUSTOS_KEYBOARD_H
#define TRUSTOS_KEYBOARD_H

#include <stdbool.h>

// Returns true when a printable key was received.
// Stores the translated character in *character.
bool keyboard_poll_char(char *character);

#endif
