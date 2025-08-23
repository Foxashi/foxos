#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* ===== Compiler checks ===== */
#if defined(__linux__)
#error "Use a cross-compiler (ix86-elf)"
#endif

#if !defined(__i386__)
#error "Compile with ix86-elf compiler"
#endif

/* ===== Forward Declarations ===== */
void terminal_writestring(const char* data);
void reboot();
void shutdown();
int fs_create(const char* filename, uint8_t attributes);

/* ===== Custom String Functions ===== */

// Added bounds checking to prevent buffer overflows
size_t strlen(const char* str) {
    if (str == NULL) return 0;
    
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int strcmp(const char* s1, const char* s2) {
    if (s1 == NULL || s2 == NULL) return -1;
    
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)((unsigned char)*s1) - (int)((unsigned char)*s2);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if (s1 == NULL || s2 == NULL || n == 0) return 0;
    
    for (size_t i = 0; i < n; i++) {
        unsigned char c1 = (unsigned char)s1[i];
        unsigned char c2 = (unsigned char)s2[i];
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    if (dest == NULL || src == NULL) return dest;
    
    char* original_dest = dest;
    while ((*dest++ = *src++));
    return original_dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    if (dest == NULL || src == NULL || n == 0) return dest;
    
    char* original_dest = dest;
    size_t i = 0;
    for (; i < n - 1 && src[i] != '\0'; i++) dest[i] = src[i];
    dest[i] = '\0'; // Always null-terminate
    return original_dest;
}

void* memset(void* ptr, int value, size_t num) {
    if (ptr == NULL) return NULL;
    
    unsigned char* p = ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, size_t n) {
    if (dest == NULL || src == NULL) return dest;
    
    char* d = dest;
    const char* s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    if (dest == NULL || src == NULL) return dest;
    
    char* d = dest;
    const char* s = src;
    
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

// Implement strchr function
char* strchr(const char* s, int c) {
    if (s == NULL) return NULL;
    
    while (*s != '\0') {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }
    return NULL;
}

// Implement strrchr function
char* strrchr(const char* s, int c) {
    if (s == NULL) return NULL;
    
    const char *found = NULL;
    while (*s != '\0') {
        if (*s == (char)c) {
            found = s;
        }
        s++;
    }
    return (char*)found;
}

// Implement strcat function
char* strcat(char* dest, const char* src) {
    if (dest == NULL || src == NULL) return dest;
    
    char* ptr = dest + strlen(dest);
    while (*src != '\0') {
        *ptr++ = *src++;
    }
    *ptr = '\0';
    return dest;
}

// Implement tolower function
int tolower(int c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

// Implement strcasecmp function
int strcasecmp(const char* s1, const char* s2) {
    if (s1 == NULL || s2 == NULL) return -1;
    
    while (*s1 && *s2) {
        int diff = tolower(*s1) - tolower(*s2);
        if (diff != 0) return diff;
        s1++;
        s2++;
    }
    return tolower(*s1) - tolower(*s2);
}

/* ===== File System Constants ===== */
#define FS_BLOCK_SIZE 512
#define FS_MAX_BLOCKS 1024        // 512KB total storage
#define FS_MAX_FILES 128
#define FS_FILENAME_LEN 32
#define FS_MAGIC 0x464F5800       // "FOX\0"
#define FS_ROOT_DIR_BLOCK 2       // Block where root directory resides

/* File attributes */
#define FS_ATTR_DIR     0x01
#define FS_ATTR_FILE    0x02
#define FS_ATTR_SYSTEM  0x04
#define FS_ATTR_HIDDEN  0x08

/* Error codes */
#define FS_OK           0
#define FS_ERROR        -1
#define FS_NOT_FOUND    -2
#define FS_EXISTS       -3
#define FS_FULL         -4
#define FS_IO_ERROR     -5
#define FS_INVALID_NAME -6
#define FS_NO_DISK      -7
#define FS_UNFORMATTED  -8

#define MAX_PATH_LEN 256
static char current_path[MAX_PATH_LEN] = "/";

/* ===== File System Structures ===== */
typedef struct {
    uint32_t magic;
    uint32_t block_count;
    uint32_t free_blocks;
    uint32_t root_dir_block;
    uint32_t fat_blocks;
    uint32_t reserved[11];  // Padding to 64 bytes
} fs_superblock_t;

typedef struct {
    uint16_t next_block;  // 0xFFFF for end of file, 0xFFFE for system reserved
} fat_entry_t;

typedef struct {
    char filename[FS_FILENAME_LEN];
    uint32_t size;
    uint32_t first_block;
    uint8_t attributes;
    uint8_t reserved[3];  // Padding
} dir_entry_t;

/* ===== Disk Emulation ===== */
// Simulated disk storage
static uint8_t simulated_disk[FS_MAX_BLOCKS * FS_BLOCK_SIZE];
static bool disk_initialized = false;

/* ===== Disk Driver Interface ===== */
bool disk_read(uint32_t block, void* buffer) {
    if (block >= FS_MAX_BLOCKS) return false;
    
    // Initialize disk if not already done
    if (!disk_initialized) {
        memset(simulated_disk, 0, sizeof(simulated_disk));
        disk_initialized = true;
    }
    
    memcpy(buffer, simulated_disk + (block * FS_BLOCK_SIZE), FS_BLOCK_SIZE);
    return true;
}

bool disk_write(uint32_t block, void* buffer) {
    if (block >= FS_MAX_BLOCKS) return false;
    
    // Initialize disk if not already done
    if (!disk_initialized) {
        memset(simulated_disk, 0, sizeof(simulated_disk));
        disk_initialized = true;
    }
    
    memcpy(simulated_disk + (block * FS_BLOCK_SIZE), buffer, FS_BLOCK_SIZE);
    return true;
}

bool disk_detected() {
    return true;
}

/* ===== File System Global State ===== */
static fat_entry_t fat_table[FS_MAX_BLOCKS];
static dir_entry_t current_dir[FS_MAX_FILES];
static uint32_t current_dir_block = FS_ROOT_DIR_BLOCK;
static fs_superblock_t superblock;
static bool fs_initialized = false;  // Track if filesystem is initialized

/* ===== Utility Functions ===== */
void itoa(int value, char* str, int base) {
    if (str == NULL) return;
    
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

    // Reverse
    char *p1 = str, *p2 = ptr - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++; p2--;
    }
}

void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++);
}

/* ===== File System Core Functions ===== */

// Initialize the file system (read superblock, FAT, and root directory)
int fs_init() {
    // Check if disk is detected first
    if (!disk_detected()) {
        return FS_NO_DISK;
    }

    // Read superblock (block 0)
    if (!disk_read(0, &superblock)) {
        return FS_IO_ERROR;
    }

    // Check if filesystem exists
    if (superblock.magic != FS_MAGIC) {
        return FS_UNFORMATTED;
    }

    // Read FAT (starts at block 1)
    uint32_t fat_size = superblock.fat_blocks;
    for (uint32_t i = 0; i < fat_size; i++) {
        if (!disk_read(1 + i, (uint8_t*)fat_table + i * FS_BLOCK_SIZE)) {
            return FS_IO_ERROR;
        }
    }

    // Read root directory
    if (!disk_read(superblock.root_dir_block, current_dir)) {
        return FS_IO_ERROR;
    }

    current_dir_block = superblock.root_dir_block;
    fs_initialized = true;
    return FS_OK;
}

// Create default directories during formatting
void create_default_directories() {
    const char* default_dirs[] = {"bin", "home", "tmp", "usr", "var"};
    size_t num_dirs = sizeof(default_dirs)/sizeof(default_dirs[0]);
    for (size_t i = 0; i < num_dirs; i++) {
        fs_create(default_dirs[i], FS_ATTR_DIR);
    }
}

// Format a new filesystem
int fs_format() {
    // Check if disk is detected first
    if (!disk_detected()) {
        return FS_NO_DISK;
    }

    // Initialize superblock
    superblock.magic = FS_MAGIC;
    superblock.block_count = FS_MAX_BLOCKS;
    superblock.free_blocks = FS_MAX_BLOCKS - 3;  // Reserve blocks 0-2
    superblock.root_dir_block = FS_ROOT_DIR_BLOCK;
    superblock.fat_blocks = 1;

    // Write superblock
    if (!disk_write(0, &superblock)) {
        return FS_IO_ERROR;
    }

    // Initialize FAT table
    for (int i = 0; i < FS_MAX_BLOCKS; i++) {
        fat_table[i].next_block = 0xFFFF;  // Mark all blocks as free
    }

    // Reserve blocks 0-2 (superblock, FAT, root dir)
    fat_table[0].next_block = 0xFFFE;  // Special marker for system blocks
    fat_table[1].next_block = 0xFFFE;
    fat_table[2].next_block = 0xFFFE;

    // Write FAT table
    for (uint32_t i = 0; i < superblock.fat_blocks; i++) {
        if (!disk_write(1 + i, (uint8_t*)fat_table + i * FS_BLOCK_SIZE)) {
            return FS_IO_ERROR;
        }
    }

    // Initialize root directory
    dir_entry_t root_dir[FS_MAX_FILES] = {0};
    root_dir[0].attributes = FS_ATTR_DIR;
    strcpy(root_dir[0].filename, ".");
    root_dir[0].first_block = FS_ROOT_DIR_BLOCK;
    
    root_dir[1].attributes = FS_ATTR_DIR;
    strcpy(root_dir[1].filename, "..");
    root_dir[1].first_block = FS_ROOT_DIR_BLOCK;

    if (!disk_write(FS_ROOT_DIR_BLOCK, root_dir)) {
        return FS_IO_ERROR;
    }

    // Update current directory in memory
    memcpy(current_dir, root_dir, sizeof(current_dir));
    current_dir_block = FS_ROOT_DIR_BLOCK;
    fs_initialized = true;

    // Create default directories
    create_default_directories();

    return FS_OK;
}

// Find a free block in the FAT
int fs_find_free_block() {
    for (int i = 3; i < FS_MAX_BLOCKS; i++) {
        if (fat_table[i].next_block == 0xFFFF) {
            return i;
        }
    }
    return -1;  // No free blocks
}

// Find a file in the current directory
dir_entry_t* fs_find_file(const char* filename) {
    if (filename == NULL) return NULL;
    
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (current_dir[i].filename[0] != '\0' && 
            strcmp(current_dir[i].filename, filename) == 0) {
            return &current_dir[i];
        }
    }
    return NULL;
}

// Validate filename
bool fs_is_valid_filename(const char* filename) {
    if (filename == NULL || strlen(filename) == 0 || strlen(filename) >= FS_FILENAME_LEN) {
        return false;
    }
    
    // Check for invalid characters
    const char* invalid_chars = "/\\?*:|\"<>";
    for (size_t i = 0; i < strlen(filename); i++) {
        if (strchr(invalid_chars, filename[i]) != NULL) {
            return false;
        }
    }
    
    // Check for reserved names
    const char* reserved_names[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "LPT1", "LPT2", NULL};
    for (int i = 0; reserved_names[i] != NULL; i++) {
        if (strcasecmp(filename, reserved_names[i]) == 0) {
            return false;
        }
    }
    
    return true;
}

// Create a new file or directory (FIXED VERSION)
int fs_create(const char* filename, uint8_t attributes) {
    // Check if disk is detected first
    if (!disk_detected()) {
        return FS_NO_DISK;
    }

    // Validate filename
    if (!fs_is_valid_filename(filename)) {
        return FS_INVALID_NAME;
    }

    // Check if file exists
    if (fs_find_file(filename) != NULL) {
        return FS_EXISTS;
    }

    // Find empty directory entry
    int entry_index = -1;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (current_dir[i].filename[0] == '\0') {
            entry_index = i;
            break;
        }
    }
    if (entry_index == -1) {
        return FS_FULL;
    }

    // Create new entry
    dir_entry_t* entry = &current_dir[entry_index];
    strncpy(entry->filename, filename, FS_FILENAME_LEN);
    entry->size = 0;
    entry->attributes = attributes;

    if (attributes & FS_ATTR_DIR) {
        // Allocate a block for the directory
        int new_block = fs_find_free_block();
        if (new_block == -1) {
            // Free the directory entry we just took
            memset(entry, 0, sizeof(dir_entry_t));
            return FS_FULL;
        }
        entry->first_block = new_block;
        
        // Mark the block as used in FAT
        fat_table[new_block].next_block = 0xFFFE; // Mark as directory block

        // Initialize the directory block with . and ..
        dir_entry_t new_dir[FS_MAX_FILES] = {0};
        
        // Create . entry
        strcpy(new_dir[0].filename, ".");
        new_dir[0].attributes = FS_ATTR_DIR;
        new_dir[0].first_block = new_block;
        
        // Create .. entry
        strcpy(new_dir[1].filename, "..");
        new_dir[1].attributes = FS_ATTR_DIR;
        new_dir[1].first_block = current_dir_block; // Parent directory block

        // Write the new directory to disk
        if (!disk_write(new_block, new_dir)) {
            // If write fails, free the block and the directory entry
            fat_table[new_block].next_block = 0xFFFF; // Mark as free again
            memset(entry, 0, sizeof(dir_entry_t));
            return FS_IO_ERROR;
        }

        // Update FAT and superblock
        superblock.free_blocks--;
    } else {
        entry->first_block = 0xFFFF;  // No blocks allocated yet for files
    }

    // Write directory back to disk - THIS IS THE CRITICAL FIX
    if (!disk_write(current_dir_block, current_dir)) {
        // If we created a directory, we need to free the block
        if (attributes & FS_ATTR_DIR) {
            fat_table[entry->first_block].next_block = 0xFFFF;
            superblock.free_blocks++;
        }
        // Free the directory entry
        memset(entry, 0, sizeof(dir_entry_t));
        return FS_IO_ERROR;
    }

    // Update FAT and superblock on disk if we created a directory
    if (attributes & FS_ATTR_DIR) {
        if (!disk_write(1, fat_table)) {
            // Handle error: we've already written the directory, so this is bad
            return FS_IO_ERROR;
        }
        if (!disk_write(0, &superblock)) {
            return FS_IO_ERROR;
        }
    }

    return FS_OK;
}

// Write data to a file
int fs_write(const char* filename, const void* data, uint32_t size) {
    // Check if disk is detected first
    if (!disk_detected()) {
        return FS_NO_DISK;
    }

    // Find the file
    dir_entry_t* entry = fs_find_file(filename);
    if (!entry) {
        return FS_NOT_FOUND;
    }

    // Calculate needed blocks
    uint32_t blocks_needed = (size + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    uint32_t current_blocks = 0;
    
    // Count existing blocks
    uint16_t block = entry->first_block;
    while (block != 0xFFFF) {
        current_blocks++;
        block = fat_table[block].next_block;
    }

    // Allocate new blocks if needed
    if (blocks_needed > current_blocks) {
        uint32_t additional = blocks_needed - current_blocks;
        uint16_t prev_block = entry->first_block;
        
        // Find last block if exists
        if (prev_block != 0xFFFF) {
            while (fat_table[prev_block].next_block != 0xFFFF) {
                prev_block = fat_table[prev_block].next_block;
            }
        }

        // Allocate new blocks
        for (uint32_t i = 0; i < additional; i++) {
            int new_block = fs_find_free_block();
            if (new_block == -1) {
                return FS_FULL;
            }

            if (prev_block == 0xFFFF) {
                entry->first_block = new_block;
            } else {
                fat_table[prev_block].next_block = new_block;
            }
            
            fat_table[new_block].next_block = 0xFFFF;
            prev_block = new_block;
            superblock.free_blocks--;
        }
    }

    // Write data to blocks
    uint32_t bytes_written = 0;
    uint16_t current_block = entry->first_block;
    const uint8_t* data_ptr = (const uint8_t*)data;
    
    while (current_block != 0xFFFF && bytes_written < size) {
        uint32_t to_write = (size - bytes_written) > FS_BLOCK_SIZE ? 
                          FS_BLOCK_SIZE : (size - bytes_written);
        
        if (!disk_write(current_block, (void*)(data_ptr + bytes_written))) {
            return FS_IO_ERROR;
        }
        
        bytes_written += to_write;
        current_block = fat_table[current_block].next_block;
    }

    // Update file size
    entry->size = size;
    
    // Update directory
    if (!disk_write(current_dir_block, current_dir)) {
        return FS_IO_ERROR;
    }

    // Update FAT and superblock
    if (!disk_write(1, fat_table)) {
        return FS_IO_ERROR;
    }

    if (!disk_write(0, &superblock)) {
        return FS_IO_ERROR;
    }

    return FS_OK;
}

// Read data from a file
int fs_read(const char* filename, void* buffer, uint32_t max_size) {
    // Check if disk is detected first
    if (!disk_detected()) {
        return FS_NO_DISK;
    }

    // Find the file
    dir_entry_t* entry = fs_find_file(filename);
    if (!entry) {
        return FS_NOT_FOUND;
    }

    // Check buffer size
    if (max_size < entry->size) {
        return FS_ERROR;  // Buffer too small
    }

    // Read data from blocks
    uint32_t bytes_read = 0;
    uint16_t current_block = entry->first_block;
    uint8_t* buffer_ptr = (uint8_t*)buffer;
    
    while (current_block != 0xFFFF && bytes_read < entry->size) {
        uint32_t to_read = (entry->size - bytes_read) > FS_BLOCK_SIZE ? 
                         FS_BLOCK_SIZE : (entry->size - bytes_read);
        
        if (!disk_read(current_block, buffer_ptr + bytes_read)) {
            return FS_IO_ERROR;
        }
        
        bytes_read += to_read;
        current_block = fat_table[current_block].next_block;
    }

    return FS_OK;
}

// List files in current directory
void fs_list() {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (current_dir[i].filename[0] != '\0') {
            // File/directory indicator
            if (current_dir[i].attributes & FS_ATTR_DIR) {
                terminal_writestring("  [D] ");
            } else {
                terminal_writestring("  [F] ");
            }
            
            // Filename
            terminal_writestring(current_dir[i].filename);
            
            // Size (for files)
            if (!(current_dir[i].attributes & FS_ATTR_DIR)) {
                terminal_writestring(" (");
                char size_str[16];
                itoa(current_dir[i].size, size_str, 10);
                terminal_writestring(size_str);
                terminal_writestring(" bytes)");
            }
            
            terminal_writestring("\n");
        }
    }
}

// Delete a file
int fs_delete(const char* filename) {
    // Check if disk is detected first
    if (!disk_detected()) {
        return FS_NO_DISK;
    }

    // Find the file
    int entry_index = -1;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (current_dir[i].filename[0] != '\0' && 
            strcmp(current_dir[i].filename, filename) == 0) {
            entry_index = i;
            break;
        }
    }
    if (entry_index == -1) {
        return FS_NOT_FOUND;
    }

    // Free all blocks used by the file
    uint16_t block = current_dir[entry_index].first_block;
    while (block != 0xFFFF) {
        uint16_t next_block = fat_table[block].next_block;
        fat_table[block].next_block = 0xFFFF;  // Mark as free
        superblock.free_blocks++;
        block = next_block;
    }

    // Clear directory entry
    memset(&current_dir[entry_index], 0, sizeof(dir_entry_t));

    // Update directory, FAT, and superblock
    if (!disk_write(current_dir_block, current_dir)) {
        return FS_IO_ERROR;
    }

    if (!disk_write(1, fat_table)) {
        return FS_IO_ERROR;
    }

    if (!disk_write(0, &superblock)) {
        return FS_IO_ERROR;
    }

    return FS_OK;
}

void fs_get_current_path(char* buffer, size_t size) {
    if (buffer == NULL || size == 0) return;
    strncpy(buffer, current_path, size);
    buffer[size-1] = '\0';
}

void fs_set_current_path(const char* path) {
    if (path == NULL) return;
    strncpy(current_path, path, MAX_PATH_LEN);
    current_path[MAX_PATH_LEN-1] = '\0';
}

// Enhanced cd command implementation (FIXED VERSION)
void handle_cd_command(const char* path) {
    if (path == NULL || strlen(path) == 0) {
        // No argument - go to root
        fs_set_current_path("/");
        if (!disk_read(FS_ROOT_DIR_BLOCK, current_dir)) {
            terminal_writestring("Error reading root directory\n");
            return;
        }
        current_dir_block = FS_ROOT_DIR_BLOCK;
        terminal_writestring("Changed to root directory\n");
        return;
    }

    // Handle absolute paths
    if (path[0] == '/') {
        // Absolute path - start from root
        fs_set_current_path("/");
        if (!disk_read(FS_ROOT_DIR_BLOCK, current_dir)) {
            terminal_writestring("Error reading root directory\n");
            return;
        }
        current_dir_block = FS_ROOT_DIR_BLOCK;
        
        // Skip the leading slash for processing
        if (strlen(path) > 1) {
            handle_cd_command(path + 1);
        }
        return;
    }

    // Handle parent directory
    if (strcmp(path, "..") == 0) {
        if (strcmp(current_path, "/") == 0) {
            terminal_writestring("Already at root directory\n");
            return;
        }
        
        // Find the ".." entry to get the parent directory block
        dir_entry_t* parent_entry = fs_find_file("..");
        if (parent_entry == NULL) {
            terminal_writestring("Error: Parent directory entry not found\n");
            return;
        }
        
        // Read the parent directory
        if (!disk_read(parent_entry->first_block, current_dir)) {
            terminal_writestring("Error reading parent directory\n");
            return;
        }
        current_dir_block = parent_entry->first_block;
        
        // Update the current path string
        char* last_slash = strrchr(current_path, '/');
        if (last_slash != NULL) {
            if (last_slash == current_path) {
                // We're at the root after removing
                current_path[1] = '\0'; // Keep the root slash
            } else {
                *last_slash = '\0';
            }
        }
        
        terminal_writestring("Changed to parent directory\n");
        return;
    }

    // Handle current directory
    if (strcmp(path, ".") == 0) {
        terminal_writestring("Remaining in current directory\n");
        return;
    }

    // Handle regular directory navigation
    dir_entry_t* entry = fs_find_file(path);
    if (entry == NULL) {
        terminal_writestring("Directory not found: ");
        terminal_writestring(path);
        terminal_writestring("\n");
        return;
    }

    if (!(entry->attributes & FS_ATTR_DIR)) {
        terminal_writestring("Not a directory: ");
        terminal_writestring(path);
        terminal_writestring("\n");
        return;
    }

    // Save the current directory block before changing
    uint32_t old_dir_block = current_dir_block;
    
    // Read the new directory contents
    if (!disk_read(entry->first_block, current_dir)) {
        terminal_writestring("Error reading directory\n");
        // Restore the original directory
        disk_read(old_dir_block, current_dir);
        current_dir_block = old_dir_block;
        return;
    }
    
    // Only update path and directory block after successful read
    current_dir_block = entry->first_block;
    
    // Update current path
    if (strcmp(current_path, "/") == 0) {
        char new_path[MAX_PATH_LEN];
        strcpy(new_path, "/");
        strcat(new_path, path);
        fs_set_current_path(new_path);
    } else {
        char new_path[MAX_PATH_LEN];
        strcpy(new_path, current_path);
        strcat(new_path, "/");
        strcat(new_path, path);
        fs_set_current_path(new_path);
    }
    
    terminal_writestring("Changed to directory: ");
    terminal_writestring(path);
    terminal_writestring("\n");
}

// Improved error reporting
void fs_perror(int error_code) {
    switch(error_code) {
        case FS_OK: terminal_writestring("Operation successful"); break;
        case FS_ERROR: terminal_writestring("General filesystem error"); break;
        case FS_NOT_FOUND: terminal_writestring("File or directory not found"); break;
        case FS_EXISTS: terminal_writestring("File already exists"); break;
        case FS_FULL: terminal_writestring("Disk full"); break;
        case FS_IO_ERROR: terminal_writestring("Disk I/O error"); break;
        case FS_INVALID_NAME: terminal_writestring("Invalid filename"); break;
        case FS_NO_DISK: terminal_writestring("No disk detected"); break;
        case FS_UNFORMATTED: terminal_writestring("Filesystem not found or formatted"); break;
    }
}

/* ===== VGA Constants ===== */
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

/* ===== VGA Helpers ===== */
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

/* ===== Terminal Config ===== */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define INPUT_BUFFER_SIZE 256
#define HISTORY_SIZE 10
#define CURSOR_BLINK_DELAY 300000

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer = (uint16_t*) VGA_MEMORY;

/* ===== Port I/O ===== */
void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Add 16-bit port I/O functions
void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

uint16_t inw(uint16_t port) {
    uint16_t result;
    __asm__ volatile ("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

void io_wait() {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

/* ===== Cursor Control ===== */
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

void strlower(char *str) {
    if (str == NULL) return;
    
    for (; *str; str++) {
        if (*str >= 'A' && *str <= 'Z') {
            *str += 32;
        }
    }
}

uint8_t parse_color(const char *name) {
    if (name == NULL) return VGA_COLOR_LIGHT_GREY;
    
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
    if (str == NULL || fmt == NULL) return 0;
    
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

/* ===== Terminal Functions ===== */
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    // Keep current terminal_color instead of resetting it

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
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT) return;
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
    if (data == NULL) return;
    
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    if (data == NULL) return;
    terminal_write(data, strlen(data));
}

/* ===== Keyboard ===== */
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
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
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
#define KEY_ENTER  0x1C
#define KEY_BACKSPACE 0x0E

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
    
    if (scancode == KEY_ENTER) {
        return '\n';
    }
    
    if (scancode == KEY_BACKSPACE) {
        return '\b';
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

/* ===== Command History ===== */
char command_history[HISTORY_SIZE][INPUT_BUFFER_SIZE];
int history_count = 0;
int history_pos = -1;

void add_to_history(const char* cmd) {
    if (cmd == NULL || strlen(cmd) == 0) return;
    
    // Don't add duplicate consecutive commands
    if (history_count > 0 && strcmp(command_history[history_count-1], cmd) == 0) {
        return;
    }

    if (history_count < HISTORY_SIZE) {
        memcpy(command_history[history_count], cmd, strlen(cmd)+1);
        history_count++;
    } else {
        // Shift history up
        for (int i = 0; i < HISTORY_SIZE-1; i++) {
            memcpy(command_history[i], command_history[i+1], INPUT_BUFFER_SIZE);
        }
        memcpy(command_history[HISTORY_SIZE-1], cmd, strlen(cmd)+1);
    }
    history_pos = -1;
}

/* ===== Input Handling ===== */
char input_buffer[INPUT_BUFFER_SIZE];
size_t input_index = 0;

/* Prompt helper - prints prompt WITHOUT leading newline */
void print_prompt(void) {
    if (fs_initialized) {
        terminal_writestring("[");
        terminal_writestring(current_path);
        terminal_writestring("] -> ");
    } else {
        terminal_writestring("-> ");
    }
}

void clear_line() {
    // Move cursor to start of line (after prompt and space)
    terminal_column = fs_initialized ? (strlen(current_path) + 4) : 8;
    update_cursor(terminal_column, terminal_row);
    
    // Clear the line from the prompt onward
    for (int i = terminal_column; i < VGA_WIDTH; i++) {
        terminal_putentryat(' ', terminal_color, i, terminal_row);
    }
    
    // Reset cursor
    terminal_column = fs_initialized ? (strlen(current_path) + 4) : 8;
    update_cursor(terminal_column, terminal_row);
}

/* Redraw input line in-place (no leading newline) */
void redraw_line() {
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

    // Update indexes / cursor position
    input_index = strlen(input_buffer);
    update_cursor(terminal_column, terminal_row);
}

void show_cursor(bool visible) {
    uint16_t cursor_pos = terminal_row * VGA_WIDTH + terminal_column;
    if (cursor_pos >= VGA_WIDTH * VGA_HEIGHT) return; // safety
    if (visible) {
        // Draw a solid rectangle cursor (reverse video)
        uint8_t current_char = terminal_buffer[cursor_pos] & 0xFF;
        uint8_t current_color = (terminal_buffer[cursor_pos] >> 8) & 0xFF;
        terminal_buffer[cursor_pos] = vga_entry(current_char, 
                                              vga_entry_color(current_color >> 4, current_color & 0x0F));
    } else {
        // Restore original character
        if (input_index < strlen(input_buffer)) {
            terminal_putentryat(input_buffer[input_index], terminal_color, terminal_column, terminal_row);
        } else {
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        }
    }
}

/* Read a line from keyboard (does NOT print the prompt) */
void read_line() {
    input_index = 0;
    input_buffer[0] = '\0';

    // Caller (shell_loop) prints the prompt once before calling read_line()
    bool cursor_visible = true;
    uint32_t last_blink = 0;

    // Initial cursor show
    show_cursor(true);

    while (1) {
        // Handle cursor blinking (simple stub; you already have it)
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
                terminal_putchar('\n');   // move to next line for command output
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
        
        // Show cursor again after processing
        show_cursor(true);
        cursor_visible = true;
        last_blink = current_time;
        
        update_cursor(terminal_column, terminal_row);
    }
}

/* ===== Reboot and Shutdown Functions ===== */
#define ACPI_SHUTDOWN_PORT 0x4004
#define ACPI_REBOOT_PORT 0x64

// Improved reboot function that works on both real hardware and emulators
void reboot() {
    terminal_writestring("Rebooting system...\n");
    delay(2000000);
    
    // Try multiple methods to ensure it works on different hardware
    uint8_t temp;
    
    // Method 1: Keyboard controller (works on most systems)
    asm volatile ("cli");
    do {
        temp = inb(0x64);
        if (temp & 0x01) inb(0.60);
    } while (temp & 0x02);
    
    outb(0x64, 0xFE);
    
    // Method 2: Triple fault (force a CPU reset)
    asm volatile (
        "cli;"
        "lidt (%0);"
        "int $0;"
        :
        : "r" (0)
    );
    
    // Method 3: Use ACPI reset command (if available)
    outw(ACPI_REBOOT_PORT, 0x1234);
    
    // If all else fails, just halt
    asm volatile ("hlt");
}

// Improved shutdown function that works on both real hardware and emulators
void shutdown() {
    terminal_writestring("Shutting down system...\n");
    delay(2000000);
    
    // Try multiple methods to ensure it works on different hardware
    
    // Method 1: ACPI shutdown (works on modern hardware)
    outw(ACPI_SHUTDOWN_PORT, 0x2000);
    
    // Method 2: QEMU and Bochs shutdown
    outw(0xB004, 0x2000);
    
    // Method 3: VirtualBox shutdown
    outw(0x4004, 0x3400);
    
    // Method 4: Try to use APM (Advanced Power Management)
    outw(0x5301, 0x0000);     // Connect to APM
    outw(0x530E, 0x0000);     // Set APM version
    outw(0x5307, 0x0001);     // Set power state to off
    outw(0x5308, 0x0000);     // Set power state to off
    
    // If all else fails, just halt
    asm volatile ("cli");
    asm volatile ("hlt");
}

/* ===== Shell Commands ===== */
void shell_filesystem_commands(const char* cmd, const char* arg1, const char* arg2, int args) {
    if (strcmp(cmd, "format") == 0) {
        int result = fs_format();
        if (result == FS_OK) {
            terminal_writestring("Filesystem formatted successfully\n");
        } else {
            terminal_writestring("Format failed: ");
            fs_perror(result);
            terminal_writestring("\n");
        }
        return;
    }

    // For all other filesystem commands, check if filesystem is initialized
    if (!fs_initialized) {
        terminal_writestring("Filesystem not initialized. Please run 'format' first.\n");
        return;
    }

    if (strcmp(cmd, "mkfile") == 0) {
        if (args < 2) {
            terminal_writestring("Usage: mkfile <filename>\n");
        } else {
            int result = fs_create(arg1, FS_ATTR_FILE);
            if (result == FS_OK) {
                terminal_writestring("File created\n");
            } else {
                terminal_writestring("Failed to create file: ");
                fs_perror(result);
                terminal_writestring("\n");
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
            } else {
                terminal_writestring("Failed to create directory: ");
                fs_perror(result);
                terminal_writestring("\n");
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
            } else {
                terminal_writestring("Write failed: ");
                fs_perror(result);
                terminal_writestring("\n");
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
            } else {
                terminal_writestring("Read failed: ");
                fs_perror(result);
                terminal_writestring("\n");
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
            } else {
                terminal_writestring("Delete failed: ");
                fs_perror(result);
                terminal_writestring("\n");
            }
        }
    }
    else if (strcmp(cmd, "cd") == 0) {
        if (args < 2) {
            handle_cd_command(NULL); // Go to root if no argument
        } else {
            handle_cd_command(arg1);
        }
    }
}

/* ===== Shell ===== */
void shell_loop() {
    enable_cursor(14, 15);
    update_cursor(0, 0);
    
    while (1) {
        // Print the prompt once (no leading newline here).
        // The previous read_line() already printed a newline when Enter was pressed.
        print_prompt();

        input_buffer[0] = '\0';
        input_index = 0;
        
        read_line();

        // Parse command and arguments
        char cmd[32] = {0};
        char arg1[32] = {0};
        char arg2[32] = {0};
        int args = sscanf(input_buffer, "%s %s %s", cmd, arg1, arg2);

        // Check for built-in commands first
        if (strcmp(cmd, "help") == 0) {
            terminal_writestring("Available commands:\n");
            terminal_writestring("  help - Show this help\n");
            terminal_writestring("  about - Show OS info\n");
            terminal_writestring("  clear - Clear screen\n");
            terminal_writestring("  color <fg> [bg] - Change text color\n");
            terminal_writestring("  history - Show command history\n");
            terminal_writestring("  reboot - Restart the system\n");
            terminal_writestring("  shutdown - Power off the system\n");
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
                strlower(arg1);
                uint8_t fg = parse_color(arg1);
                uint8_t bg = (args >= 3) ? parse_color(arg2) : VGA_COLOR_BLACK;
                terminal_setcolor(vga_entry_color(fg, bg));
                terminal_writestring("Text color changed!\n");
            }
        }
        else if (strcmp(cmd, "about") == 0) {
            terminal_writestring("FoxOS v0.1\n");
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
        else if (strcmp(cmd, "reboot") == 0) {
            reboot();
        }
        else if (strcmp(cmd, "shutdown") == 0) {
            shutdown();
        }
        else {
            // Handle filesystem commands
            bool handled = false;
            
            // Check if it's a filesystem command
            const char* fs_commands[] = {"format", "mkfile", "mkdir", "write", "read", "ls", "rm", "cd"};
            for (size_t i = 0; i < sizeof(fs_commands)/sizeof(fs_commands[0]); i++) {
                if (strcmp(cmd, fs_commands[i]) == 0) {
                    shell_filesystem_commands(cmd, arg1, arg2, args);
                    handled = true;
                    break;
                }
            }
            
            // If not a known command, show special message
            if (!handled && strlen(cmd) > 0) {
                terminal_writestring("Unknown command: '");
                terminal_writestring(cmd);
                terminal_writestring("'. Type 'help' for available commands.\n");
            }
        }
        
        update_cursor(terminal_column, terminal_row);
    }
}

/* ===== Kernel Main ===== */
void kernel_main() {
    terminal_initialize();
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
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
        
        // Initialize file system
        terminal_writestring("<");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("CHECK");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring("> Checking filesystem...\n");
        int fs_result = fs_init();
        if (fs_result == FS_OK) {
            terminal_writestring("<OK>\n");
        } else if (fs_result == FS_NO_DISK) {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            terminal_writestring("<FAIL> No disk detected\n");
            terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        } else if (fs_result == FS_UNFORMATTED) {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            terminal_writestring("<FAIL> ");
            fs_perror(fs_result);
            terminal_writestring("\n");
            terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
            terminal_writestring("Run 'format' to create a new filesystem\n");
        }
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("<FAIL> No disk found\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    }
    
    terminal_writestring("Type 'help' for commands\n\n");
    shell_loop();
}