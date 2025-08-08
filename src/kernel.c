#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* Compiler checks */
#if defined(__linux__)
#error "Use a cross-compiler (ix86-elf)"
#endif

#if !defined(__i386__)
#error "Compile with ix86-elf compiler"
#endif

/* --- Forward Declarations --- */
size_t strlen(const char* str);
int strcmp(const char* a, const char* b);
int sscanf(const char* str, const char* fmt, ...);
void strlower(char *str);
uint8_t parse_color(const char *name);
void memmove(void* dest, const void* src, size_t n);

/* --- Terminal Functions --- */
void terminal_putchar(char c);
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);
void terminal_initialize(void);
void terminal_setcolor(uint8_t color);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);

/* --- VGA Constants --- */
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

/* --- VGA Helpers --- */
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

/* --- Terminal Config --- */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define INPUT_BUFFER_SIZE 256
#define HISTORY_SIZE 10

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer = (uint16_t*) VGA_MEMORY;

/* --- Port I/O --- */
void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void io_wait() {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

/* --- Cursor Control --- */
void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void disable_cursor() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* --- String Functions --- */
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

void strlower(char *str) {
    for (; *str; str++) {
        if (*str >= 'A' && *str <= 'Z') {
            *str += 32;
        }
    }
}

void memmove(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
}

uint8_t parse_color(const char *name) {
    if (strcmp(name, "black") == 0) return VGA_COLOR_BLACK;
    if (strcmp(name, "blue") == 0) return VGA_COLOR_BLUE;
    if (strcmp(name, "green") == 0) return VGA_COLOR_GREEN;
    if (strcmp(name, "cyan") == 0) return VGA_COLOR_CYAN;
    if (strcmp(name, "red") == 0) return VGA_COLOR_RED;
    if (strcmp(name, "magenta") == 0) return VGA_COLOR_MAGENTA;
    if (strcmp(name, "brown") == 0) return VGA_COLOR_BROWN;
    if (strcmp(name, "light_grey") == 0) return VGA_COLOR_LIGHT_GREY;
    if (strcmp(name, "dark_grey") == 0) return VGA_COLOR_DARK_GREY;
    if (strcmp(name, "light_blue") == 0) return VGA_COLOR_LIGHT_BLUE;
    if (strcmp(name, "light_green") == 0) return VGA_COLOR_LIGHT_GREEN;
    if (strcmp(name, "light_cyan") == 0) return VGA_COLOR_LIGHT_CYAN;
    if (strcmp(name, "light_red") == 0) return VGA_COLOR_LIGHT_RED;
    if (strcmp(name, "light_magenta") == 0) return VGA_COLOR_LIGHT_MAGENTA;
    if (strcmp(name, "light_brown") == 0) return VGA_COLOR_LIGHT_BROWN;
    if (strcmp(name, "white") == 0) return VGA_COLOR_WHITE;
    return VGA_COLOR_LIGHT_GREY;
}

int sscanf(const char *str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    int count = 0;
    while (*fmt && *str) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's') {
                char *out = va_arg(args, char*);
                while (*str && *str != ' ' && *str != '\t') {
                    *out++ = *str++;
                }
                *out = '\0';
                count++;
            }
            fmt++;
        } else {
            if (*fmt != *str) break;
            fmt++;
            str++;
        }
    }
    va_end(args);
    return count;
}

/* --- Terminal Functions --- */
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    // Don't reset color here - keep current terminal_color

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
        }
    }
    
    enable_cursor(14, 15);
    update_cursor(0, 0);
}

void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    terminal_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            for (size_t y = 1; y < VGA_HEIGHT; y++) {
                for (size_t x = 0; x < VGA_WIDTH; x++) {
                    terminal_buffer[(y-1)*VGA_WIDTH + x] = terminal_buffer[y*VGA_WIDTH + x];
                }
            }
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                terminal_putentryat(' ', terminal_color, x, VGA_HEIGHT-1);
            }
            terminal_row = VGA_HEIGHT-1;
        }
    } else {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            if (++terminal_row == VGA_HEIGHT) {
                for (size_t y = 1; y < VGA_HEIGHT; y++) {
                    for (size_t x = 0; x < VGA_WIDTH; x++) {
                        terminal_buffer[(y-1)*VGA_WIDTH + x] = terminal_buffer[y*VGA_WIDTH + x];
                    }
                }
                for (size_t x = 0; x < VGA_WIDTH; x++) {
                    terminal_putentryat(' ', terminal_color, x, VGA_HEIGHT-1);
                }
                terminal_row = VGA_HEIGHT-1;
            }
        }
    }
    update_cursor(terminal_column, terminal_row);
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

/* --- Keyboard --- */
const char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const char keyboard_map_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Special key codes */
#define KEY_UP    0x48
#define KEY_DOWN  0x50
#define KEY_LEFT  0x4B
#define KEY_RIGHT 0x4D
#define KEY_LSHIFT 0x2A
#define KEY_RSHIFT 0x36
#define KEY_CAPS   0x3A

/* Keyboard state */
bool shift_pressed = false;
bool caps_lock = false;

char get_key() {
    while ((inb(0x64) & 0x01) == 0) io_wait();
    uint8_t scancode = inb(0x60);
    
    if (scancode == 0xE0) { // Extended key prefix
        while ((inb(0x64) & 0x01) == 0) io_wait();
        scancode = inb(0x60);
        
        switch(scancode) {
            case KEY_UP:    return '\x11'; // Ctrl+Q
            case KEY_DOWN:  return '\x12'; // Ctrl+R
            case KEY_LEFT:  return '\x13'; // Ctrl+S
            case KEY_RIGHT: return '\x14'; // Ctrl+T
            default:       return 0;
        }
    }
    
    // Handle key releases
    if (scancode & 0x80) {
        uint8_t released_key = scancode & 0x7F;
        if (released_key == KEY_LSHIFT || released_key == KEY_RSHIFT) {
            shift_pressed = false;
        }
        return 0;
    }
    
    // Handle key presses
    if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
        shift_pressed = true;
        return 0;
    }
    
    if (scancode == KEY_CAPS) {
        caps_lock = !caps_lock;
        return 0;
    }
    
    if (scancode >= sizeof(keyboard_map)) return 0;
    
    // Determine which character to return based on shift and caps state
    bool uppercase = (shift_pressed != caps_lock); // XOR
    if (uppercase) {
        return keyboard_map_shift[scancode];
    } else {
        return keyboard_map[scancode];
    }
}

/* --- Command History --- */
char command_history[HISTORY_SIZE][INPUT_BUFFER_SIZE];
int history_count = 0;
int history_pos = -1;

void add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    
    // Don't add duplicate consecutive commands
    if (history_count > 0 && strcmp(command_history[history_count-1], cmd) == 0) {
        return;
    }

    if (history_count < HISTORY_SIZE) {
        memmove(command_history[history_count], cmd, strlen(cmd)+1);
        history_count++;
    } else {
        // Shift history up
        for (int i = 0; i < HISTORY_SIZE-1; i++) {
            memmove(command_history[i], command_history[i+1], INPUT_BUFFER_SIZE);
        }
        memmove(command_history[HISTORY_SIZE-1], cmd, strlen(cmd)+1);
    }
    history_pos = -1;
}

/* --- Input Handling --- */
char input_buffer[INPUT_BUFFER_SIZE];
size_t input_index = 0;

void clear_line() {
    // Move cursor to start of line (after prompt and space)
    terminal_column = 3;
    update_cursor(terminal_column, terminal_row);
    
    // Clear the line from the prompt onward
    for (int i = 3; i < VGA_WIDTH; i++) {
        terminal_putentryat(' ', terminal_color, i, terminal_row);
    }
    
    // Reset cursor
    terminal_column = 3;
    update_cursor(terminal_column, terminal_row);
}

void redraw_line() {
    clear_line();
    for (size_t i = 0; i < strlen(input_buffer); i++) {
        terminal_putchar(input_buffer[i]);
    }
    input_index = strlen(input_buffer);
}

void read_line() {
    input_index = 0;
    input_buffer[0] = '\0';
    
    while (1) {
        char c = get_key();
        if (!c) continue;

        uint16_t cursor_pos = terminal_row * VGA_WIDTH + terminal_column;
        uint16_t saved_char = terminal_buffer[cursor_pos];
        
        // Remove cursor feedback before processing key
        terminal_buffer[cursor_pos] = saved_char;

        switch(c) {
            case '\x11': // Up arrow
                if (history_pos < history_count-1) {
                    history_pos++;
                    memmove(input_buffer, command_history[history_count-1 - history_pos], 
                          strlen(command_history[history_count-1 - history_pos])+1);
                    redraw_line();
                }
                break;
                
            case '\x12': // Down arrow
                if (history_pos > 0) {
                    history_pos--;
                    memmove(input_buffer, command_history[history_count-1 - history_pos], 
                          strlen(command_history[history_count-1 - history_pos])+1);
                    redraw_line();
                } else if (history_pos == 0) {
                    history_pos = -1;
                    input_buffer[0] = '\0';
                    redraw_line();
                }
                break;
                
            case '\x13': // Left arrow
                if (input_index > 0) {
                    input_index--;
                    if (terminal_column > 3) {
                        terminal_column--;
                    }
                }
                break;
                
            case '\x14': // Right arrow
                if (input_index < strlen(input_buffer)) {
                    input_index++;
                    if (terminal_column < VGA_WIDTH-1) {
                        terminal_column++;
                    }
                }
                break;
                
            case '\n': // Enter
                terminal_putchar('\n');
                if (strlen(input_buffer) > 0) {
                    add_to_history(input_buffer);
                }
                return;
                
            case '\b': // Backspace
                if (input_index > 0) {
                    memmove(&input_buffer[input_index-1], &input_buffer[input_index], 
                           strlen(&input_buffer[input_index])+1);
                    input_index--;
                    redraw_line();
                }
                break;
                
            default: // Normal character
                if (input_index < INPUT_BUFFER_SIZE-1) {
                    // Make space for new character
                    memmove(&input_buffer[input_index+1], &input_buffer[input_index], 
                           strlen(&input_buffer[input_index])+1);
                    input_buffer[input_index++] = c;
                    redraw_line();
                }
        }
        
        // Add cursor feedback after processing key
        cursor_pos = terminal_row * VGA_WIDTH + terminal_column;
        saved_char = terminal_buffer[cursor_pos];
        terminal_buffer[cursor_pos] ^= 0x700;
        
        update_cursor(terminal_column, terminal_row);
    }
}

/* --- Shell --- */
void shell_loop() {
    enable_cursor(14, 15);
    update_cursor(0, 0);
    
    while (1) {
        terminal_column = 0;
        terminal_writestring("\n-> ");
        terminal_putchar(' '); // Add extra space after prompt
        input_buffer[0] = '\0';
        input_index = 0;
        
        read_line();

        // Parse command and arguments
        char cmd[32] = {0};
        char arg1[32] = {0};
        char arg2[32] = {0};
        int args = sscanf(input_buffer, "%s %s %s", cmd, arg1, arg2);

        if (strcmp(cmd, "help") == 0) {
            terminal_writestring("Available commands:\n");
            terminal_writestring("  help - Show this help\n");
            terminal_writestring("  about - Show OS info\n");
            terminal_writestring("  clear - Clear screen\n");
            terminal_writestring("  color <fg> [bg] - Change text color\n");
            terminal_writestring("  history - Show command history\n");
            terminal_writestring("  caps - Toggle caps lock state\n");
            terminal_writestring("Colors: black, blue, green, cyan, red, magenta,\n");
            terminal_writestring("brown, light_grey, dark_grey, light_blue,\n");
            terminal_writestring("light_green, light_cyan, light_red,\n");
            terminal_writestring("light_magenta, light_brown, white\n");
        }
        else if (strcmp(cmd, "color") == 0) {
            if (args < 2) {
                terminal_writestring("Usage: color <foreground> [background]\n");
            } else {
                strlower(arg1);
                uint8_t fg = parse_color(arg1);
                uint8_t bg = (args >= 3) ? parse_color(arg2) : VGA_COLOR_BLACK;
                terminal_setcolor(vga_entry_color(fg, bg));
                terminal_writestring("Text color changed!\n");
            }
        }
        else if (strcmp(cmd, "about") == 0) {
            terminal_writestring("FoxOS v0.1 - Now with caps lock and shift support!\n");
        }
        else if (strcmp(cmd, "clear") == 0) {
            terminal_initialize();
        }
        else if (strcmp(cmd, "history") == 0) {
            for (int i = 0; i < history_count; i++) {
                terminal_writestring("  ");
                terminal_writestring(command_history[i]);
                terminal_writestring("\n");
            }
        }
        else if (strcmp(cmd, "caps") == 0) {
            caps_lock = !caps_lock;
            terminal_writestring(caps_lock ? "Caps Lock ON\n" : "Caps Lock OFF\n");
        }
        else if (strlen(cmd) > 0) {
            terminal_writestring("Unknown command. Type 'help'\n");
        }
        
        update_cursor(terminal_column, terminal_row);
    }
}

/* --- Kernel Main --- */
void kernel_main() {
    terminal_initialize();
    // Set initial color (can be changed later with 'color' command)
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    terminal_writestring("FoxOS [Version 0.1]\n");
    terminal_writestring("Now with caps lock and shift support! Type 'help'\n");
    shell_loop();
}