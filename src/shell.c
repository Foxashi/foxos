#include "shell.h"
#include "fs.h"
#include "vga.h"
#include "string.h"
#include "keyboard.h"
#include "stdlib.h"
#include <stdbool.h>

#define INPUT_BUFFER_SIZE 256
#define HISTORY_SIZE 10
#define CURSOR_BLINK_DELAY 300000

/* Shell state */
static char command_history[HISTORY_SIZE][INPUT_BUFFER_SIZE];
static int history_count = 0;
static int history_pos = -1;
static char input_buffer[INPUT_BUFFER_SIZE];
static size_t input_index = 0;

/* Forward declarations */
static void print_prompt(void);
static void clear_shell_line(void);
static void redraw_line(void);
static void show_cursor(bool visible);
static void add_to_history(const char* cmd);
static void parse_input(char* input, char* cmd, char* arg1, char* arg2);
static uint8_t parse_color(const char* color);

/* Helper Functions */
static void print_prompt(void) {
    char path_buf[MAX_PATH_LEN];
    if(fs_is_initialized()) {
        fs_get_current_path(path_buf, sizeof(path_buf));
        terminal_writestring("[");
        terminal_writestring(path_buf);
        terminal_writestring("] -> ");
    } else {
        terminal_writestring("-> ");
    }
}

static void clear_shell_line() {
    char path_buf[MAX_PATH_LEN];
    fs_get_current_path(path_buf, sizeof(path_buf));
    size_t prompt_len = strlen(path_buf) + 5; // "[path] -> "
    
    // Clear from prompt onward
    for (size_t i = prompt_len; i < VGA_WIDTH; i++) {
        terminal_putentryat(' ', terminal_color, i, terminal_row);
    }
    
    // Reset cursor
    terminal_column = prompt_len;
    update_cursor(terminal_column, terminal_row);
}

static void redraw_line() {
    // Clear the entire current row
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_putentryat(' ', terminal_color, x, terminal_row);
    }

    // Move cursor to start of line and print prompt
    terminal_column = 0;
    update_cursor(terminal_column, terminal_row);
    print_prompt();

    // Print the input buffer contents
    for (size_t i = 0; i < strlen(input_buffer); i++) {
        terminal_putchar(input_buffer[i]);
    }

    // Update input index and cursor position
    input_index = strlen(input_buffer);
    terminal_column = strlen(input_buffer) + terminal_column;
    update_cursor(terminal_column, terminal_row);
}

static void show_cursor(bool visible) {
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
    
    if (visible) {
        // Draw a solid rectangle cursor (reverse video)
        uint16_t entry = terminal_buffer[pos];
        uint8_t fg = (entry >> 8) & 0x0F;
        uint8_t bg = (entry >> 12) & 0x0F;
        terminal_buffer[pos] = vga_entry((entry & 0xFF), vga_entry_color(bg, fg));
    } else {
        // Restore original character
        if (input_index < strlen(input_buffer)) {
            terminal_putentryat(input_buffer[input_index], terminal_color, terminal_column, terminal_row);
        } else {
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        }
    }
}

static void add_to_history(const char* cmd) {
    if (strlen(cmd) == 0) return;
    
    // Don't add duplicate consecutive commands
    if (history_count > 0 && strcmp(command_history[history_count-1], cmd) == 0) {
        return;
    }

    if (history_count < HISTORY_SIZE) {
        strcpy(command_history[history_count], cmd);
        history_count++;
    } else {
        // Shift history up
        for (int i = 0; i < HISTORY_SIZE-1; i++) {
            strcpy(command_history[i], command_history[i+1]);
        }
        strcpy(command_history[HISTORY_SIZE-1], cmd);
    }
    history_pos = -1;
}

/* Input parser */
static void parse_input(char* input, char* cmd, char* arg1, char* arg2) {
    cmd[0] = arg1[0] = arg2[0] = '\0';
    int count = 0;
    char* tokens[3] = {cmd, arg1, arg2};
    char* token = strtok(input, " ");
    
    while (token && count < 3) {
        strncpy(tokens[count], token, 32);
        tokens[count][31] = '\0';
        count++;
        token = strtok(NULL, " ");
    }
}

/* Color parser */
static uint8_t parse_color(const char* color) {
    // Create a lowercase copy for case-insensitive comparison
    char lower_color[32];
    strncpy(lower_color, color, sizeof(lower_color));
    strlower(lower_color);
    
    if (strcmp(lower_color, "black") == 0) return VGA_COLOR_BLACK;
    if (strcmp(lower_color, "blue") == 0) return VGA_COLOR_BLUE;
    if (strcmp(lower_color, "green") == 0) return VGA_COLOR_GREEN;
    if (strcmp(lower_color, "cyan") == 0) return VGA_COLOR_CYAN;
    if (strcmp(lower_color, "red") == 0) return VGA_COLOR_RED;
    if (strcmp(lower_color, "magenta") == 0) return VGA_COLOR_MAGENTA;
    if (strcmp(lower_color, "brown") == 0) return VGA_COLOR_BROWN;
    if (strcmp(lower_color, "light_grey") == 0) return VGA_COLOR_LIGHT_GREY;
    if (strcmp(lower_color, "dark_grey") == 0) return VGA_COLOR_DARK_GREY;
    if (strcmp(lower_color, "light_blue") == 0) return VGA_COLOR_LIGHT_BLUE;
    if (strcmp(lower_color, "light_green") == 0) return VGA_COLOR_LIGHT_GREEN;
    if (strcmp(lower_color, "light_cyan") == 0) return VGA_COLOR_LIGHT_CYAN;
    if (strcmp(lower_color, "light_red") == 0) return VGA_COLOR_LIGHT_RED;
    if (strcmp(lower_color, "light_magenta") == 0) return VGA_COLOR_LIGHT_MAGENTA;
    if (strcmp(lower_color, "yellow") == 0) return VGA_COLOR_LIGHT_BROWN;
    if (strcmp(lower_color, "white") == 0) return VGA_COLOR_WHITE;
    return VGA_COLOR_WHITE; // default
}

/* Input Handling */
void read_line() {
    input_index = 0;
    input_buffer[0] = '\0';

    bool cursor_visible = true;
    uint32_t last_blink = 0;
    show_cursor(true);

    while (1) {
        // Handle cursor blinking
        // TODO: Implement proper timing
        uint32_t current_time = 0;
        if (current_time - last_blink > CURSOR_BLINK_DELAY) {
            cursor_visible = !cursor_visible;
            show_cursor(cursor_visible);
            last_blink = current_time;
        }

        char c = get_key();
        if (!c) {
            // Small delay to prevent CPU hogging
            for (volatile int i = 0; i < 1000; i++);
            continue;
        }

        // Hide cursor before processing key
        show_cursor(false);
        cursor_visible = false;

        switch(c) {
            case '\x11': // Up arrow
                if (history_pos < history_count-1) {
                    history_pos++;
                    strcpy(input_buffer, command_history[history_count-1 - history_pos]);
                    redraw_line();
                }
                break;

            case '\x12': // Down arrow
                if (history_pos > 0) {
                    history_pos--;
                    strcpy(input_buffer, command_history[history_count-1 - history_pos]);
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
                    terminal_column--;
                    update_cursor(terminal_column, terminal_row);
                }
                break;
            
            case '\x14': // Right arrow
                if (input_index < strlen(input_buffer)) {
                    input_index++;
                    terminal_column++;
                    update_cursor(terminal_column, terminal_row);
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
                           strlen(&input_buffer[input_index]) + 1);
                    input_index--;
                    redraw_line();
                }
                break;
                
            default: // Normal character
                if (input_index < INPUT_BUFFER_SIZE-1 && c >= 32 && c <= 126) {
                    // Make space for new character
                    memmove(&input_buffer[input_index+1], &input_buffer[input_index], 
                           strlen(&input_buffer[input_index]) + 1);
                    input_buffer[input_index] = c;
                    input_index++;
                    redraw_line();
                }
        }
        
        // Show cursor again after processing
        show_cursor(true);
        cursor_visible = true;
        last_blink = current_time;
    }
}

/* Command Processing */
void shell_filesystem_commands(const char* cmd, const char* arg1, const char* arg2, int args) {
    char path_buf[MAX_PATH_LEN];
    
    if (strcmp(cmd, "format") == 0) {
        int result = fs_format();
        if (result == FS_OK) {
            terminal_writestring("Filesystem formatted successfully\n");
        } else {
            terminal_writestring("Format failed\n");
        }
    }
    else if (strcmp(cmd, "mkfile") == 0) {
        if (args < 2) {
            terminal_writestring("Usage: mkfile <filename>\n");
        } else {
            int result = fs_create(arg1, FS_ATTR_FILE);
            if (result == FS_OK) {
                terminal_writestring("File created\n");
            } else if (result == FS_EXISTS) {
                terminal_writestring("File already exists\n");
            } else {
                terminal_writestring("Failed to create file\n");
            }
        }
    }
    else if (strcmp(cmd, "mkdir") == 0) {
        if (args < 2) {
            terminal_writestring("Usage: mkdir <dirname>\n");
        } else {
            int result = fs_create(arg1, FS_ATTR_DIR);
            if (result == FS_OK) {
                terminal_writestring("Directory created\n");
            } else if (result == FS_EXISTS) {
                terminal_writestring("Directory already exists\n");
            } else {
                terminal_writestring("Failed to create directory\n");
            }
        }
    }
    else if (strcmp(cmd, "write") == 0) {
        if (args < 3) {
            terminal_writestring("Usage: write <filename> <text>\n");
        } else {
            int result = fs_write(arg1, arg2, strlen(arg2)+1);
            if (result == FS_OK) {
                terminal_writestring("Write successful\n");
            } else if (result == FS_NOT_FOUND) {
                terminal_writestring("File not found\n");
            } else {
                terminal_writestring("Write failed\n");
            }
        }
    }
    else if (strcmp(cmd, "read") == 0) {
        if (args < 2) {
            terminal_writestring("Usage: read <filename>\n");
        } else {
            char buffer[FS_BLOCK_SIZE];
            int result = fs_read(arg1, buffer, sizeof(buffer));
            if (result == FS_OK) {
                terminal_writestring("File contents: ");
                terminal_writestring(buffer);
                terminal_writestring("\n");
            } else if (result == FS_NOT_FOUND) {
                terminal_writestring("File not found\n");
            } else {
                terminal_writestring("Read failed\n");
            }
        }
    }
    else if (strcmp(cmd, "ls") == 0) {
        fs_list();
    }
    else if (strcmp(cmd, "rm") == 0) {
        if (args < 2) {
            terminal_writestring("Usage: rm <filename>\n");
        } else {
            int result = fs_delete(arg1);
            if (result == FS_OK) {
                terminal_writestring("File deleted\n");
            } else if (result == FS_NOT_FOUND) {
                terminal_writestring("File not found\n");
            } else {
                terminal_writestring("Delete failed\n");
            }
        }
    }
    else if (strcmp(cmd, "cd") == 0) {
        if (args < 2) {
            // No argument - go to root
            fs_set_current_path("/");
            terminal_writestring("Changed to root directory\n");
        } else {
            if (strcmp(arg1, "/") == 0) {
                fs_set_current_path("/");
                terminal_writestring("Changed to root directory\n");
            } else if (strcmp(arg1, ".") == 0) {
                terminal_writestring("Remaining in current directory\n");
            } else if (strcmp(arg1, "..") == 0) {
                fs_get_current_path(path_buf, sizeof(path_buf));
                if (strcmp(path_buf, "/") != 0) {
                    fs_set_current_path("/");
                    terminal_writestring("Changed to root directory\n");
                } else {
                    terminal_writestring("Already at root directory\n");
                }
            } else {
                terminal_writestring("Directory navigation not fully implemented yet\n");
                terminal_writestring("Only '/', '.', and '..' are supported\n");
            }
        }
    }
}

/* Main Shell Loop */
void shell_loop() {
    enable_cursor(14, 15);
    update_cursor(0, 0);
    
    char cmd[32];
    char arg1[32];
    char arg2[32];
    
    while (1) {
        print_prompt();
        input_buffer[0] = '\0';
        input_index = 0;
        
        read_line();

        // Parse command and arguments
        cmd[0] = arg1[0] = arg2[0] = '\0';
        parse_input(input_buffer, cmd, arg1, arg2);
        int args = (cmd[0] ? 1 : 0) + (arg1[0] ? 1 : 0) + (arg2[0] ? 1 : 0);

        if (strcmp(cmd, "help") == 0) {
            terminal_writestring("Available commands:\n");
            terminal_writestring("  help - Show this help\n");
            terminal_writestring("  about - Show OS info\n");
            terminal_writestring("  clear - Clear screen\n");
            terminal_writestring("  color <fg> [bg] - Change text color\n");
            terminal_writestring("  history - Show command history\n");
            terminal_writestring("Filesystem commands:\n");
            terminal_writestring("  format - Format filesystem\n");
            terminal_writestring("  mkfile <name> - Create file\n");
            terminal_writestring("  mkdir <name> - Create directory\n");
            terminal_writestring("  write <file> <text> - Write to file\n");
            terminal_writestring("  read <file> - Read file\n");
            terminal_writestring("  ls - List files\n");
            terminal_writestring("  rm <file> - Delete file\n");
            terminal_writestring("  cd [dir] - Change directory\n");
        }
        else if (strcmp(cmd, "color") == 0) {
            if (args < 2) {
                terminal_writestring("Usage: color <foreground> [background]\n");
            } else {
                uint8_t fg = parse_color(arg1);
                uint8_t bg = (args >= 3) ? parse_color(arg2) : VGA_COLOR_BLACK;
                terminal_setcolor(vga_entry_color(fg, bg));
                terminal_writestring("Text color changed!\n");
            }
        }
        else if (strcmp(cmd, "about") == 0) {
            terminal_writestring("FoxOS v0.1\n");
            terminal_writestring("A simple operating system project\n");
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
        else if (cmd[0]) {
            // Handle filesystem commands
            shell_filesystem_commands(cmd, arg1, arg2, args);
        }
    }
}