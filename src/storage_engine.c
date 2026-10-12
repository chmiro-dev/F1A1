#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "../include/mfs_types.h"

// Global handle for our active container file descriptor
static int disk_fd = -1;
static mfs_superblock_t cached_sb;

// 1. Mount the storage image
int mfs_mount(const char *image_path) {
    disk_fd = open(image_path, O_RDWR);
    if (disk_fd < 0) {
        perror("Storage Engine: Failed to open image file");
        return -1;
    }

    // Read the superblock from Block 0
    if (pread(disk_fd, &cached_sb, sizeof(mfs_superblock_t), 0) != sizeof(mfs_superblock_t)) {
        perror("Storage Engine: Failed to read superblock");
        close(disk_fd);
        return -1;
    }

    // Verify magic number
    if (cached_sb.magic != MFS_MAGIC) {
        fprintf(stderr, "Storage Engine Error: Invalid magic number! Not an MFS image.\n");
        close(disk_fd);
        return -1;
    }

    printf("Storage Engine: Successfully mounted %s (Free blocks: %u)\n", image_path, cached_sb.free_blocks);
    return 0;
}

// 2. Unmount the storage image
void mfs_unmount(void) {
    if (disk_fd >= 0) {
        close(disk_fd);
        disk_fd = -1;
        printf("Storage Engine: Unmounted successfully.\n");
    }
}

// 3. Create and write a file into the storage image
int mfs_write_file(const char *filename, const char *data, size_t data_size) {
    if (disk_fd < 0) {
        fprintf(stderr, "Error: Storage engine not mounted.\n");
        return -1;
    }

    if (data_size > BLOCK_SIZE * MAX_DIRECT_BLOCKS) {
        fprintf(stderr, "Error: File too large for MVP limits.\n");
        return -1;
    }

    // Scan the Inode Table to find a free inode slot
    off_t inode_table_offset = cached_sb.inode_table_block * BLOCK_SIZE;
    mfs_inode_t target_inode;
    int free_inode_index = -1;

    for (uint32_t i = 0; i < cached_sb.total_inodes; i++) {
        off_t current_inode_offset = inode_table_offset + (i * sizeof(mfs_inode_t));
        
        if (pread(disk_fd, &target_inode, sizeof(mfs_inode_t), current_inode_offset) != sizeof(mfs_inode_t)) {
            perror("Failed to read inode slot");
            return -1;
        }

        // If inode_id is 0, we consider it a free slot
        if (target_inode.inode_id == 0) {
            free_inode_index = i;
            break;
        }
    }

    if (free_inode_index == -1) {
        fprintf(stderr, "Error: No free inodes available!\n");
        return -1;
    }

    // Simple Data Block Allocation: For MVP, grab the first available free block in the data region
    // (In a full engine, you'd check a bitmap; here we use a naive sequential allocation)
    uint32_t allocated_data_block = cached_sb.data_region_block; // Simplified MVP allocation

    // Populate the new inode
    memset(&target_inode, 0, sizeof(mfs_inode_t));
    target_inode.inode_id = free_inode_index + 1;
    target_inode.file_type = 1; // 1 = regular file
    target_inode.file_size = data_size;
    target_inode.permissions = 0644;
    strncpy(target_inode.name, filename, MAX_FILENAME_LEN - 1);
    target_inode.direct_blocks[0] = allocated_data_block;

    // Write the data payload into the allocated data block offset
    off_t data_offset = (off_t)allocated_data_block * BLOCK_SIZE;
    if (pwrite(disk_fd, data, data_size, data_offset) != (ssize_t)data_size) {
        perror("Failed to write file payload data");
        return -1;
    }

    // Write the populated inode back to its slot in the Inode Table
    off_t target_slot_offset = inode_table_offset + (free_inode_index * sizeof(mfs_inode_t));
    if (pwrite(disk_fd, &target_inode, sizeof(mfs_inode_t), target_slot_offset) != sizeof(mfs_inode_t)) {
        perror("Failed to write inode metadata");
        return -1;
    }

    printf("Storage Engine: Created file '%s' (Size: %zu bytes) at Inode %u\n", filename, data_size, target_inode.inode_id);
    return 0;
}