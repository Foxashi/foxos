#include <stdbool.h> 
#include "keyboard.h"
#include "io.h"
#include "vga.h"

/* Keyboard state */
static bool shift_pressed = false;
static bool caps_lock = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;

/* Scancode maps */
static const char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char keyboard_map_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Special key codes */
#define KEY_ESC        0x01
#define KEY_BACKSPACE  0x0E
#define KEY_TAB        0x0F
#define KEY_ENTER      0x1C
#define KEY_LSHIFT     0x2A
#define KEY_RSHIFT     0x36
#define KEY_LCTRL      0x1D
#define KEY_LALT       0x38
#define KEY_CAPS       0x3A
#define KEY_F1         0x3B
#define KEY_F2         0x3C
#define KEY_F3         0x3D
#define KEY_F4         0x3E
#define KEY_F5         0x3F
#define KEY_F6         0x40
#define KEY_F7         0x41
#define KEY_F8         0x42
#define KEY_F9         0x43
#define KEY_F10        0x44
#define KEY_NUMLOCK    0x45
#define KEY_SCRLOCK    0x46
#define KEY_HOME       0x47
#define KEY_UP         0x48
#define KEY_PGUP       0x49
#define KEY_LEFT       0x4B
#define KEY_RIGHT      0x4D
#define KEY_END        0x4F
#define KEY_DOWN       0x50
#define KEY_PGDN       0x51
#define KEY_INS        0x52
#define KEY_DEL        0x53
#define KEY_F11        0x57
#define KEY_F12        0x58

/* Keyboard initialization */
void keyboard_init(void) {
    // Enable keyboard interrupts (would be used with actual IRQ handler)
    // outb(0x21, inb(0x21) & 0xFD); // Enable IRQ1 (keyboard)
}

/* Check if keyboard has data ready */
bool keyboard_has_data(void) {
    return (inb(0x64) & 0x01);
}

/* Get a single key press */
char get_key(void) {
    while (!keyboard_has_data()) {
        io_wait();
    }
    
    uint8_t scancode = inb(0x60);
    
    // Handle key releases (break codes)
    if (scancode & 0x80) {
        uint8_t released_key = scancode & 0x7F;
        
        switch (released_key) {
            case KEY_LSHIFT:
            case KEY_RSHIFT:
                shift_pressed = false;
                break;
            case KEY_LCTRL:
                ctrl_pressed = false;
                break;
            case KEY_LALT:
                alt_pressed = false;
                break;
        }
        return 0;
    }
    
    // Handle special keys
    switch (scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            shift_pressed = true;
            return 0;
        case KEY_LCTRL:
            ctrl_pressed = true;
            return 0;
        case KEY_LALT:
            alt_pressed = true;
            return 0;
        case KEY_CAPS:
            caps_lock = !caps_lock;
            // Update caps lock LED
            {
                uint8_t leds = 0;
                if (caps_lock) leds |= 0x04;
                while (inb(0x64) & 0x02); // Wait for input buffer to be clear
                outb(0x60, 0xED); // LED command
                while (inb(0x64) & 0x02);
                outb(0x60, leds);
            }
            return 0;
    }
    
    // Handle extended keys (0xE0 prefix)
    if (scancode == 0xE0) {
        while (!keyboard_has_data()) {
            io_wait();
        }
        scancode = inb(0x60);
        
        switch(scancode) {
            case KEY_UP:    return '\x11'; // Ctrl+Q
            case KEY_DOWN:  return '\x12'; // Ctrl+R
            case KEY_LEFT:  return '\x13'; // Ctrl+S
            case KEY_RIGHT: return '\x14'; // Ctrl+T
            default:        return 0;
        }
    }
    
    // Check for valid scancode range
    if (scancode >= sizeof(keyboard_map)) {
        return 0;
    }
    
    // Handle normal keys
    bool uppercase = (shift_pressed != caps_lock); // XOR
    
    if (ctrl_pressed) {
        // Return control codes for Ctrl+letter combinations
        if (scancode >= 0x10 && scancode <= 0x1C) { // Q-W-E-R-T-Y etc.
            return scancode - 0x10 + 1; // Ctrl+Q = 0x11, etc.
        }
        return 0;
    }
    
    if (uppercase) {
        return keyboard_map_shift[scancode];
    } else {
        return keyboard_map[scancode];
    }
}

/* Get current keyboard modifiers */
keyboard_modifiers_t get_keyboard_modifiers(void) {
    keyboard_modifiers_t mods = {
        .shift = shift_pressed,
        .ctrl = ctrl_pressed,
        .alt = alt_pressed,
        .caps_lock = caps_lock
    };
    return mods;
}

/* Wait for a specific key press */
void wait_for_key(uint8_t key) {
    while (get_key() != key) {
        io_wait();
    }
}