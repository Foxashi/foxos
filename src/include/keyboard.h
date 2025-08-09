#include <stdbool.h>

typedef struct {
    bool shift;
    bool ctrl;
    bool alt;
    bool caps_lock;
} keyboard_modifiers_t;

keyboard_modifiers_t get_keyboard_modifiers(void);

#ifndef _KEYBOARD_H
#define _KEYBOARD_H

char get_key();
void keyboard_init();

#endif