#ifndef MFS_TYPES_H
#define MFS_TYPES_H

#include <stdint.h>

#define MFS_MAGIC 0x4D465331       // "MFS1" in hex
#define BLOCK_SIZE 4096            // 4KB standard block size
#define MAX_FILENAME_LEN 256
#define MAX_DIRECT_BLOCKS 12       // Simple direct block pointers for MVP

// 1. Superblock: Stored at Block 0 (Offset 0)
typedef struct {
    uint32_t magic;                // Magic number to verify file system type
    uint32_t block_size;           // Size of each block (4096 bytes)
    uint32_t total_blocks;         // Total blocks in the massive file
    uint32_t free_blocks;          // Count of currently unallocated blocks
    uint32_t total_inodes;         // Total inode slots available
    uint32_t free_inodes;          // Count of currently free inode slots
    uint32_t inode_table_block;    // Starting block index for the inode table
    uint32_t data_region_block;    // Starting block index for actual data blocks
} __attribute__((packed)) mfs_superblock_t;

// 2. Inode: Represents a single file or directory metadata
typedef struct {
    uint32_t inode_id;             // Unique identifier for this inode
    uint8_t  file_type;            // 1 = file, 2 = directory
    uint32_t file_size;            // Size of the file in bytes
    uint16_t permissions;          // Read/write permissions
    uint32_t direct_blocks[MAX_DIRECT_BLOCKS]; // Pointers to data blocks
    char     name[MAX_FILENAME_LEN]; // File name
} __attribute__((packed)) mfs_inode_t;

#endif // MFS_TYPES_H