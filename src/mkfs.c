#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../include/mfs_types.h"

#define DEFAULT_IMAGE_SIZE (64 * 1024 * 1024) // 64 MB container file

int main(int argc, char *argv[]) {
    const char *image_path = "storage.img";
    
    // Allow custom image name via command line argument if desired
    if (argc > 1) {
        image_path = argv[1];
    }

    printf("Initializing MFS storage image: %s\n", image_path);

    // 1. Open/Create the massive file with read/write permissions
    int fd = open(image_path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to open image file");
        return 1;
    }

    // 2. Pre-allocate 64MB of space to avoid runtime fragmentation and write errors
    size_t image_size = DEFAULT_IMAGE_SIZE;
    if (posix_fallocate(fd, 0, image_size) != 0) {
        perror("Failed to pre-allocate space (posix_fallocate)");
        close(fd);
        return 1;
    }

    uint32_t total_blocks = image_size / BLOCK_SIZE;

    // 3. Initialize Superblock values
    mfs_superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    
    sb.magic = MFS_MAGIC;
    sb.block_size = BLOCK_SIZE;
    sb.total_blocks = total_blocks;
    sb.total_inodes = 128;             // Fixed MVP inode count
    sb.inode_table_block = 1;          // Inode table starts immediately at Block 1

    // Calculate how many blocks the inode table spans
    size_t inode_table_bytes = sb.total_inodes * sizeof(mfs_inode_t);
    uint32_t inode_table_blocks = (inode_table_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // Data region begins right after the inode table
    sb.data_region_block = sb.inode_table_block + inode_table_blocks;
    
    // Calculate remaining unallocated blocks for file payloads
    sb.free_blocks = total_blocks - sb.data_region_block;
    sb.free_inodes = sb.total_inodes;

    // 4. Write the Superblock struct to Block 0 (Byte Offset 0)
    if (pwrite(fd, &sb, sizeof(sb), 0) != sizeof(sb)) {
        perror("Failed to write superblock");
        close(fd);
        return 1;
    }

    printf("Successfully formatted %s:\n", image_path);
    printf("  - Block Size: %u bytes\n", sb.block_size);
    printf("  - Total Blocks: %u (~64 MB)\n", sb.total_blocks);
    printf("  - Inode Table: Starts at block %u (%u total slots)\n", sb.inode_table_block, sb.total_inodes);
    printf("  - Data Region: Starts at block %u (%u available blocks)\n", sb.data_region_block, sb.free_blocks);

    close(fd);
    return 0;
}