#include "vga.h"
#include "fs.h"
#include "shell.h"
#include "keyboard.h"
#include "stdlib.h"
#include "io.h"

/* Forward declarations */
void display_boot_messages(void);

/* Kernel entry point */
void kernel_main(void) {
    // Initialize terminal with default colors
    terminal_initialize();
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    // Display boot messages
    display_boot_messages();
    
    // Initialize keyboard
    keyboard_init();
    
    // Start the shell
    shell_loop();
    
    // shell_loop() is infinite, so we never reach here
}

/* Boot sequence messages */
void display_boot_messages(void) {
    terminal_writestring("-- FoxOS [Version 0.1] --\n");
    delay(5000000);
    
    terminal_writestring("<");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("BOOT");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("> Booting system...\n");
    delay(3000000);
    
    terminal_writestring("<");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("CHECK");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("> Checking disks...\n");
    delay(2000000);
    
    if (disk_detected()) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("<OK> Disk found\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        
        terminal_writestring("<");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("MOUNT");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("> Checking filesystem...\n");
        
        int fs_status = fs_init();
        if (fs_status == FS_OK) {
            terminal_writestring("<OK>\n");
        } else {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            terminal_writestring("<FAIL> Filesystem error: ");
            
            // Print specific error message
            switch(fs_status) {
                case FS_NOT_FOUND:
                    terminal_writestring("No filesystem found!\n");
                    break;
                case FS_IO_ERROR:
                    terminal_writestring("Disk I/O error!\n");
                    break;
                default:
                    terminal_writestring("Unknown error!\n");
            }
            
            terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            terminal_writestring("Run 'format' to create a new filesystem\n");
        }
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("<FAIL> No disk found\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
    
    terminal_writestring("Type 'help' for commands\n\n");
}

/* Simple delay function */
void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++);
}

/* Basic integer to string conversion */
void itoa(int value, char* str, int base) {
    char* ptr = str;
    bool negative = false;
    unsigned int u;
    
    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }
    
    if (value < 0 && base == 10) {
        negative = true;
        u = (unsigned int)(-value);
    } else {
        u = (unsigned int)value;
    }

    while (u) {
        unsigned int digit = u % base;
        *ptr++ = "0123456789abcdef"[digit];
        u /= base;
    }
    
    if (negative) *ptr++ = '-';
    *ptr = '\0';

    // Reverse the string
    char *p1 = str, *p2 = ptr - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++; 
        p2--;
    }
}