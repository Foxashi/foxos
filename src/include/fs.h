#ifndef _FS_H
#define _FS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* === Filesystem Constants === */
#define FS_BLOCK_SIZE          512
#define FS_MAX_BLOCKS          1024       // 512KB total storage
#define FS_MAX_FILES           128
#define FS_FILENAME_LEN        32
#define FS_MAGIC               0x464F5800 // "FOX\0"
#define FS_ROOT_DIR_BLOCK      2          // Block where root directory resides
#define MAX_PATH_LEN           256

/* File attributes */
#define FS_ATTR_DIR           0x01
#define FS_ATTR_FILE          0x02
#define FS_ATTR_SYSTEM        0x04
#define FS_ATTR_HIDDEN        0x08

/* Error codes */
#define FS_OK                 0
#define FS_ERROR              -1
#define FS_NOT_FOUND          -2
#define FS_EXISTS             -3
#define FS_FULL               -4
#define FS_IO_ERROR           -5

bool fs_is_initialized(void);
const char* fs_get_current_path(void);

/* === Filesystem Structures === */
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

/* === Disk Interface === */
bool disk_read(uint32_t block, void* buffer);
bool disk_write(uint32_t block, void* buffer);
bool disk_detected();

/* === Core Filesystem API === */
int fs_init(void);
int fs_format(void);
int fs_create(const char* filename, uint8_t attributes);
int fs_write(const char* filename, const void* data, uint32_t size);
int fs_read(const char* filename, void* buffer, uint32_t max_size);
void fs_list(void);
int fs_delete(const char* filename);

/* === Path/Directory Functions === */
void fs_get_current_path(char* buffer, size_t size);
void fs_set_current_path(const char* path);

/* === Utility Functions === */
int fs_find_free_block(void);
dir_entry_t* fs_find_file(const char* filename);
bool fs_is_initialized(void);

#endif // _FS_H