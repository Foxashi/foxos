#include "fs.h"
#include "vga.h"
#include "string.h"
#include "stdlib.h"
#include <stdbool.h> // Added for bool type

/* Filesystem global state */
static fat_entry_t fat_table[FS_MAX_BLOCKS];
static dir_entry_t current_dir[FS_MAX_FILES];
static uint32_t current_dir_block = FS_ROOT_DIR_BLOCK;
static fs_superblock_t superblock;
static char current_path[MAX_PATH_LEN] = "/";

/* Disk interface stubs (to be implemented with actual disk driver) */
bool disk_read(uint32_t block, void* buffer) {
    // TODO: Replace with actual disk reading
    (void)block; // Mark parameter as unused
    memset(buffer, 0, FS_BLOCK_SIZE);
    return true;
}

bool disk_write(uint32_t block, void* buffer) {
    // TODO: Replace with actual disk writing
    (void)block;      // Mark parameter as unused
    (void)buffer;     // Mark parameter as unused
    return true;
}

bool disk_detected() {
    return true;
}

/* Filesystem initialization status check */
bool fs_is_initialized(void) {
    return (superblock.magic == FS_MAGIC);
}

/* Get current path */
const char* fs_get_current_path(void) {
    return current_path;
}

/* Core filesystem functions */
int fs_init(void) {
    // Read superblock (block 0)
    if (!disk_read(0, &superblock)) {
        terminal_writestring("Failed to read superblock\n");
        return FS_IO_ERROR;
    }

    // Check filesystem magic
    if (superblock.magic != FS_MAGIC) {
        terminal_writestring("No filesystem detected\n");
        return FS_NOT_FOUND;
    }

    // Read FAT (starts at block 1)
    uint32_t fat_size = superblock.fat_blocks;
    for (uint32_t i = 0; i < fat_size; i++) {
        if (!disk_read(1 + i, (uint8_t*)fat_table + i * FS_BLOCK_SIZE)) {
            terminal_writestring("Failed to read FAT\n");
            return FS_IO_ERROR;
        }
    }

    // Read root directory
    if (!disk_read(superblock.root_dir_block, current_dir)) {
        terminal_writestring("Failed to read root directory\n");
        return FS_IO_ERROR;
    }

    current_dir_block = superblock.root_dir_block;
    return FS_OK;
}

int fs_format(void) {
    // Initialize superblock
    superblock.magic = FS_MAGIC;
    superblock.block_count = FS_MAX_BLOCKS;
    superblock.free_blocks = FS_MAX_BLOCKS - 3;  // Reserve blocks 0-2
    superblock.root_dir_block = FS_ROOT_DIR_BLOCK;
    superblock.fat_blocks = 1;

    // Write superblock
    if (!disk_write(0, &superblock)) {
        terminal_writestring("Failed to write superblock\n");
        return FS_IO_ERROR;
    }

    // Initialize FAT table
    for (int i = 0; i < FS_MAX_BLOCKS; i++) {
        fat_table[i].next_block = 0xFFFF;  // Mark all blocks as free
    }

    // Reserve system blocks
    fat_table[0].next_block = 0xFFFE;  // Superblock
    fat_table[1].next_block = 0xFFFE;  // FAT
    fat_table[2].next_block = 0xFFFE;  // Root directory

    // Write FAT table
    for (uint32_t i = 0; i < superblock.fat_blocks; i++) {
        if (!disk_write(1 + i, (uint8_t*)fat_table + i * FS_BLOCK_SIZE)) {
            terminal_writestring("Failed to write FAT\n");
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
        terminal_writestring("Failed to write root directory\n");
        return FS_IO_ERROR;
    }

    // Update in-memory state
    memcpy(current_dir, root_dir, sizeof(current_dir));
    current_dir_block = FS_ROOT_DIR_BLOCK;

    return FS_OK;
}

int fs_find_free_block(void) {
    for (int i = 3; i < FS_MAX_BLOCKS; i++) {
        if (fat_table[i].next_block == 0xFFFF) {
            return i;
        }
    }
    return -1;  // No free blocks
}

dir_entry_t* fs_find_file(const char* filename) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (current_dir[i].filename[0] != '\0' && 
            strcmp(current_dir[i].filename, filename) == 0) {
            return &current_dir[i];
        }
    }
    return NULL;
}

int fs_create(const char* filename, uint8_t attributes) {
    // Validate filename
    if (strlen(filename) == 0 || strlen(filename) >= FS_FILENAME_LEN) {
        return FS_ERROR;
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
    entry->first_block = 0xFFFF;  // No blocks allocated yet

    // Write directory back to disk
    if (!disk_write(current_dir_block, current_dir)) {
        return FS_IO_ERROR;
    }

    return FS_OK;
}

int fs_write(const char* filename, const void* data, uint32_t size) {
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

int fs_read(const char* filename, void* buffer, uint32_t max_size) {
    // Find the file
    dir_entry_t* entry = fs_find_file(filename);
    if (!entry) {
        return FS_NOT_FOUND;
    }

    // Check buffer size
    if (max_size < entry->size) {
        return FS_ERROR;
    }

    // Read data from blocks
    uint32_t bytes_read = 0;
    uint16_t current_block = entry->first_block;
    uint8_t* buffer_ptr = (uint8_t*)buffer;
    
    while (current_block != 0xFFFF && bytes_read < entry->size) {
        uint32_t to_read = (entry->size - bytes_read) > FS_BLOCK_SIZE ? 
                         FS_BLOCK_SIZE : (entry->size - bytes_read);
        
        // Fixed the extra parenthesis here
        if (!disk_read(current_block, buffer_ptr + bytes_read)) {
            return FS_IO_ERROR;
        }
        
        bytes_read += to_read;
        current_block = fat_table[current_block].next_block;
    }

    return FS_OK;
}

void fs_list(void) {
    terminal_writestring("Directory listing:\n");
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

int fs_delete(const char* filename) {
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
    strncpy(buffer, current_path, size);
    buffer[size-1] = '\0';
}

void fs_set_current_path(const char* path) {
    strncpy(current_path, path, MAX_PATH_LEN);
    current_path[MAX_PATH_LEN-1] = '\0';
}