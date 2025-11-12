#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/*
 * Constants based on VSFS file system layout
 */
#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 64
#define MAGIC_BYTES 0xD34D
#define INODE_SIZE 256
#define INODE_COUNT 80  // 5 blocks * 4096 / 256 = 80 inodes

#define SUPERBLOCK_NUM 0
#define INODE_BITMAP_BLOCK_NUM 1
#define DATA_BITMAP_BLOCK_NUM 2
#define INODE_TABLE_START_BLOCK_NUM 3
#define INODE_TABLE_BLOCKS 5
#define DATA_BLOCK_START_NUM 8
#define DATA_BLOCKS_COUNT 56  // 64 - 8 = 56 data blocks

/*
 * Superblock structure
 */
typedef struct {
    uint16_t magic;              // Magic bytes (0xD34D)
    uint32_t block_size;         // Block size (4096)
    uint32_t total_blocks;       // Total blocks (64)
    uint32_t inode_bitmap_block; // Inode bitmap block (1)
    uint32_t data_bitmap_block;  // Data bitmap block (2)
    uint32_t inode_table_start;  // Inode table start block (3)
    uint32_t first_data_block;   // First data block (8)
    uint32_t inode_size;         // Inode size (256)
    uint32_t inode_count;        // Inode count (80)
    uint8_t reserved[4058];      // Reserved space
} superblock_t;

/*
 * Inode structure
 */
typedef struct {
    uint32_t mode;                // File mode
    uint32_t uid;                 // User ID
    uint32_t gid;                 // Group ID
    uint32_t size;                // File size in bytes
    uint32_t atime;               // Last access time
    uint32_t ctime;               // Creation time
    uint32_t mtime;               // Last modification time
    uint32_t dtime;               // Deletion time
    uint32_t links_count;         // Number of hard links
    uint32_t blocks_count;        // Number of data blocks
    uint32_t direct_block;        // Direct block pointer
    uint32_t single_indirect;     // Single indirect block pointer
    uint32_t double_indirect;     // Double indirect block pointer
    uint32_t triple_indirect;     // Triple indirect block pointer
    uint8_t reserved[156];        // Reserved space
} inode_t;

/*
 * Global variables
 */
uint8_t *fs_image = NULL;        // File system image in memory
superblock_t *superblock = NULL; // Pointer to superblock in memory
uint8_t *inode_bitmap = NULL;    // Pointer to inode bitmap
uint8_t *data_bitmap = NULL;     // Pointer to data bitmap
inode_t *inode_table = NULL;     // Pointer to inode table
bool *block_ref_count = NULL;    // Track block references for duplicate detection

/*
 * Helper functions
 */

// Read a block from the file system image
void *get_block(int block_num) {
    if (block_num < 0 || block_num >= TOTAL_BLOCKS) {
        return NULL;
    }
    return fs_image + (block_num * BLOCK_SIZE);
}

// Check if a bit is set in a bitmap
bool is_bit_set(uint8_t *bitmap, int bit_num) {
    int byte_num = bit_num / 8;
    int bit_off = bit_num % 8;
    return (bitmap[byte_num] & (1 << bit_off)) != 0;
}

// Set a bit in a bitmap
void set_bit(uint8_t *bitmap, int bit_num) {
    int byte_num = bit_num / 8;
    int bit_off = bit_num % 8;
    bitmap[byte_num] |= (1 << bit_off);
}

// Clear a bit in a bitmap
void clear_bit(uint8_t *bitmap, int bit_num) {
    int byte_num = bit_num / 8;
    int bit_off = bit_num % 8;
    bitmap[byte_num] &= ~(1 << bit_off);
}

// Check if inode is valid
bool is_inode_valid(inode_t *inode) {
    return inode->links_count > 0 && inode->dtime == 0;
}

// Convert time to string for display
char *time_to_str(uint32_t timestamp) {
    time_t t = (time_t)timestamp;
    static char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return buffer;
}

/*
 * Consistency Checker Components
 */

// 1. Superblock Validator //22101328
bool validate_superblock(bool fix) {
    bool isValid = true;
    printf("\n=== Superblock Validation ===\n");
    
    // Check magic number
    if (superblock->magic != MAGIC_BYTES) {
        printf("Error: Invalid magic number (0x%04X). Expected 0x%04X\n", 
               superblock->magic, MAGIC_BYTES);
        if (fix) {
            printf("Fixing: Setting correct magic number\n");
            superblock->magic = MAGIC_BYTES;
        }
        isValid = false;
    } else {
        printf("Magic number is valid (0x%04X)\n", superblock->magic);
    }
    
    // Check block size
    if (superblock->block_size != BLOCK_SIZE) {
        printf("Error: Invalid block size (%u). Expected %u\n", 
               superblock->block_size, BLOCK_SIZE);
        if (fix) {
            printf("Fixing: Setting correct block size\n");
            superblock->block_size = BLOCK_SIZE;
        }
        isValid = false;
    } else {
        printf("Block size is valid (%u)\n", superblock->block_size);
    }
    
    // Check total number of blocks
    if (superblock->total_blocks != TOTAL_BLOCKS) {
        printf("Error: Invalid total blocks (%u). Expected %u\n", 
               superblock->total_blocks, TOTAL_BLOCKS);
        if (fix) {
            printf("Fixing: Setting correct total blocks\n");
            superblock->total_blocks = TOTAL_BLOCKS;
        }
        isValid = false;
    } else {
        printf("Total blocks is valid (%u)\n", superblock->total_blocks);
    }
    
    // Check inode bitmap block
    if (superblock->inode_bitmap_block != INODE_BITMAP_BLOCK_NUM) {
        printf("Error: Invalid inode bitmap block (%u). Expected %u\n", 
               superblock->inode_bitmap_block, INODE_BITMAP_BLOCK_NUM);
        if (fix) {
            printf("Fixing: Setting correct inode bitmap block\n");
            superblock->inode_bitmap_block = INODE_BITMAP_BLOCK_NUM;
        }
        isValid = false;
    } else {
        printf("Inode bitmap block is valid (%u)\n", superblock->inode_bitmap_block);
    }
    
    // Check data bitmap block
    if (superblock->data_bitmap_block != DATA_BITMAP_BLOCK_NUM) {
        printf("Error: Invalid data bitmap block (%u). Expected %u\n", 
               superblock->data_bitmap_block, DATA_BITMAP_BLOCK_NUM);
        if (fix) {
            printf("Fixing: Setting correct data bitmap block\n");
            superblock->data_bitmap_block = DATA_BITMAP_BLOCK_NUM;
        }
        isValid = false;
    } else {
        printf("Data bitmap block is valid (%u)\n", superblock->data_bitmap_block);
    }
    
    // Check inode table start block
    if (superblock->inode_table_start != INODE_TABLE_START_BLOCK_NUM) {
        printf("Error: Invalid inode table start block (%u). Expected %u\n", 
               superblock->inode_table_start, INODE_TABLE_START_BLOCK_NUM);
        if (fix) {
            printf("Fixing: Setting correct inode table start block\n");
            superblock->inode_table_start = INODE_TABLE_START_BLOCK_NUM;
        }
        isValid = false;
    } else {
        printf("Inode table start block is valid (%u)\n", superblock->inode_table_start);
    }
    
    // Check first data block
    if (superblock->first_data_block != DATA_BLOCK_START_NUM) {
        printf("Error: Invalid first data block (%u). Expected %u\n", 
               superblock->first_data_block, DATA_BLOCK_START_NUM);
        if (fix) {
            printf("Fixing: Setting correct first data block\n");
            superblock->first_data_block = DATA_BLOCK_START_NUM;
        }
        isValid = false;
    } else {
        printf("First data block is valid (%u)\n", superblock->first_data_block);
    }
    
    // Check inode size
    if (superblock->inode_size != INODE_SIZE) {
        printf("Error: Invalid inode size (%u). Expected %u\n", 
               superblock->inode_size, INODE_SIZE);
        if (fix) {
            printf("Fixing: Setting correct inode size\n");
            superblock->inode_size = INODE_SIZE;
        }
        isValid = false;
    } else {
        printf("Inode size is valid (%u)\n", superblock->inode_size);
    }
    
    // Check inode count
    if (superblock->inode_count != INODE_COUNT) {
        printf("Error: Invalid inode count (%u). Expected %u\n", 
               superblock->inode_count, INODE_COUNT);
        if (fix) {
            printf("Fixing: Setting correct inode count\n");
            superblock->inode_count = INODE_COUNT;
        }
        isValid = false;
    } else {
        printf("Inode count is valid (%u)\n", superblock->inode_count);
    }
    
    return isValid;
}

// 2. Data Bitmap Consistency Checker //22101328
bool validate_data_bitmap(bool fix) {
    printf("\n=== Data Bitmap Validation ===\n");
    
    bool isValid = true;
    bool *block_used = calloc(DATA_BLOCKS_COUNT, sizeof(bool));
    
    if (!block_used) {
        printf("Memory allocation failed\n");
        return false;
    }
    
    // First pass: Check all inodes and mark which data blocks they reference
    printf("Checking blocks referenced by inodes...\n");
    for (int i = 0; i < INODE_COUNT; i++) {
        inode_t *inode = &inode_table[i];
        
        // Skip invalid inodes
        if (!is_inode_valid(inode)) {
            continue;
        }
        
        // Check direct block pointer
        if (inode->direct_block != 0) {
            int block_idx = inode->direct_block - DATA_BLOCK_START_NUM;
            if (block_idx >= 0 && block_idx < DATA_BLOCKS_COUNT) {
                block_used[block_idx] = true;
            }
        }
        
        // Check indirect block pointers
        
        if (inode->single_indirect != 0) {
            int block_idx = inode->single_indirect - DATA_BLOCK_START_NUM;
            if (block_idx >= 0 && block_idx < DATA_BLOCKS_COUNT) {
                block_used[block_idx] = true;
            }
        }
        
        if (inode->double_indirect != 0) {
            int block_idx = inode->double_indirect - DATA_BLOCK_START_NUM;
            if (block_idx >= 0 && block_idx < DATA_BLOCKS_COUNT) {
                block_used[block_idx] = true;
            }
        }
        
        if (inode->triple_indirect != 0) {
            int block_idx = inode->triple_indirect - DATA_BLOCK_START_NUM;
            if (block_idx >= 0 && block_idx < DATA_BLOCKS_COUNT) {
                block_used[block_idx] = true;
            }
        }
    }
    
    // Second pass: Check if data bitmap matches actual block usage
    printf("Validating data bitmap against block references...\n");
    for (int i = 0; i < DATA_BLOCKS_COUNT; i++) {
        bool bitmap_used = is_bit_set(data_bitmap, i);
        
        // Case 1: Block is referenced by an inode but not marked as used in bitmap
        if (block_used[i] && !bitmap_used) {
            printf("Error: Block %d is referenced by inode(s) but not marked used in data bitmap\n", 
                   i + DATA_BLOCK_START_NUM);
            if (fix) {
                printf("Fixing: Marking block %d as used in data bitmap\n", 
                       i + DATA_BLOCK_START_NUM);
                set_bit(data_bitmap, i);
            }
            isValid = false;
        }
        
        // Case 2: Block is marked as used in bitmap but not referenced by any inode
        if (!block_used[i] && bitmap_used) {
            printf("Error: Block %d is marked used in data bitmap but not referenced by any inode\n", 
                   i + DATA_BLOCK_START_NUM);
            if (fix) {
                printf("Fixing: Clearing block %d in data bitmap\n", 
                       i + DATA_BLOCK_START_NUM);
                clear_bit(data_bitmap, i);
            }
            isValid = false;
        }
    }
    
    free(block_used);
    return isValid;
}

// 3. Inode Bitmap Consistency Checker //22101305
bool validate_inode_bitmap(bool fix) {
    printf("\n=== Inode Bitmap Validation ===\n");
    
    bool isValid = true;
    
    // Check each inode
    for (int i = 0; i < INODE_COUNT; i++) {
        inode_t *inode = &inode_table[i];
        bool is_valid = is_inode_valid(inode);
        bool bitmap_used = is_bit_set(inode_bitmap, i);
        
        // Case 1: Valid inode but not marked in bitmap
        if (is_valid && !bitmap_used) {
            printf("Error: Inode %d is valid but not marked used in inode bitmap\n", i);
            if (fix) {
                printf("Fixing: Marking inode %d as used in inode bitmap\n", i);
                set_bit(inode_bitmap, i);
            }
            isValid = false;
        }
        
        // Case 2: Invalid inode but marked in bitmap
        if (!is_valid && bitmap_used) {
            printf("Error: Inode %d is invalid but marked used in inode bitmap\n", i);
            if (fix) {
                printf("Fixing: Clearing inode %d in inode bitmap\n", i);
                clear_bit(inode_bitmap, i);
            }
            isValid = false;
        }
    }
    
    return isValid;
}

// 4. Duplicate Block Checker //22101305

bool check_data_block_for_duplicates(uint32_t blk, int ino, bool do_fix, int *inode_refs) {
    bool valid = true;
    if (blk >= DATA_BLOCK_START_NUM && blk < TOTAL_BLOCKS) {
        if (block_ref_count[blk]) {
            valid = false;
            printf("Error: Block %u is referenced by inode %d and inode %d\n", blk, inode_refs[blk], ino);
            
            
            
            if (do_fix) {
                printf("Note: Duplicate in indirect block - requires file system recovery tools\n");
            }
        } else {
            block_ref_count[blk] = true;
            inode_refs[blk] = ino;
        }
    }
    return valid;
}
bool check_duplicate_blocks(bool fix) {
    printf("\n=== Duplicate Block Check ===\n");
    
    bool isValid = true;
    
    
    memset(block_ref_count, 0, TOTAL_BLOCKS * sizeof(bool));
    
    
    int *inode_refs = calloc(TOTAL_BLOCKS, sizeof(int));
    if (!inode_refs) {
        printf("Memory allocation failed\n");
        return false;
    }
    
    
    for (int i = 0; i < INODE_COUNT; i++) {
        inode_t *inode = &inode_table[i];
        
        
        if (!is_inode_valid(inode)) {
            continue;
        }
        
        
        if (inode->direct_block != 0) {
            if (inode->direct_block >= DATA_BLOCK_START_NUM && 
                inode->direct_block < TOTAL_BLOCKS) {
                if (block_ref_count[inode->direct_block]) {
                    
                    isValid = false;
                    printf("Error: Block %u is referenced by inode %d and inode %d\n", inode->direct_block, inode_refs[inode->direct_block], i);
                    if (fix) {
                        
                        
                        printf("Fixing: Zeroing out duplicate reference in inode %d\n", i);
                        inode->direct_block = 0;
                    }
                } else {
                    block_ref_count[inode->direct_block] = true;
                    inode_refs[inode->direct_block] = i;
                }
            }
        }
        
        // Helper function for checking blocks in indirect blocks
        bool is_valid = true;
        
        
        if (inode->single_indirect != 0) {
            if (inode->single_indirect >= DATA_BLOCK_START_NUM && inode->single_indirect < TOTAL_BLOCKS) {
                if (block_ref_count[inode->single_indirect]) {
                    
                    isValid = false;
                    printf("Error: Block %u (single indirect) is referenced by inode %d and inode %d\n", inode->single_indirect, inode_refs[inode->single_indirect], i);
                    if (fix) {
                        printf("Fixing: Zeroing out duplicate reference in inode %d\n", i);
                        inode->single_indirect = 0;
                    }
                } else {
                    block_ref_count[inode->single_indirect] = true;
                    inode_refs[inode->single_indirect] = i;
                    
                    uint32_t *indirect_block = (uint32_t *)get_block(inode->single_indirect);
                    int entries_per_block = BLOCK_SIZE / sizeof(uint32_t);
                    for (int j = 0; j < entries_per_block; j++) {
                        uint32_t data_block_num = indirect_block[j];
                        if (data_block_num != 0) {
                            if (!check_data_block_for_duplicates(data_block_num, i, fix, inode_refs)) {
                                isValid = false;
                                if (fix) {
                                    
                                    indirect_block[j] = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Check double indirect block pointer
        if (inode->double_indirect != 0) {
            if (inode->double_indirect >= DATA_BLOCK_START_NUM && inode->double_indirect < TOTAL_BLOCKS) {
                if (block_ref_count[inode->double_indirect]) {
                    isValid = false;
                    printf("Error: Block %u (double indirect) is referenced by inode %d and inode %d\n", inode->double_indirect, inode_refs[inode->double_indirect], i);
                    if (fix) {
                        printf("Fixing: Zeroing out duplicate reference in inode %d\n", i);
                        inode->double_indirect = 0;
                    }
                } else {
                    block_ref_count[inode->double_indirect] = true;
                    inode_refs[inode->double_indirect] = i;
                   
                    uint32_t *double_indirect_block = (uint32_t *)get_block(inode->double_indirect);
                    int entries_per_block = BLOCK_SIZE / sizeof(uint32_t);
                    for (int j = 0; j < entries_per_block; j++) {
                        uint32_t indirect_block_num = double_indirect_block[j];
                        if (indirect_block_num != 0) {
                            if (!check_data_block_for_duplicates(indirect_block_num, i, fix, inode_refs)) {
                                isValid = false;
                                if (fix) {
                                    double_indirect_block[j] = 0;
                                }
                            }
                            uint32_t *indirect_block = (uint32_t *)get_block(indirect_block_num);
                            if (indirect_block) {
                                int entries_per_indirect_block = BLOCK_SIZE / sizeof(uint32_t);
                                for (int k = 0; k < entries_per_indirect_block; k++) {
                                    uint32_t data_block_num = indirect_block[k];
                                    if (data_block_num != 0)
                                        if (!check_data_block_for_duplicates(data_block_num, i, fix, inode_refs)) {
                                            isValid = false;
                                            if (fix) {
                                                indirect_block[k] = 0;
                                            }
                                        }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Check triple indirect block pointer
        if (inode->triple_indirect != 0) {
            if (inode->triple_indirect >= DATA_BLOCK_START_NUM && inode->triple_indirect < TOTAL_BLOCKS) {
                if (block_ref_count[inode->triple_indirect]) {
                    isValid = false;
                    printf("Error: Block %u (triple indirect) is referenced by inode %d and inode %d\n", inode->triple_indirect, inode_refs[inode->triple_indirect], i);
                    if (fix) {
                        printf("Fixing: Zeroing out duplicate reference in inode %d\n", i);
                        inode->triple_indirect = 0;
                    }
                } else {
                    block_ref_count[inode->triple_indirect] = true;
                    inode_refs[inode->triple_indirect] = i;
                    uint32_t *triple_indirect_block = (uint32_t *)get_block(inode->triple_indirect);
                    int entries_per_block = BLOCK_SIZE / sizeof(uint32_t);
                    for (int j = 0; j < entries_per_block; j++) {
                        uint32_t double_indirect_block_num = triple_indirect_block[j];
                        if (double_indirect_block_num != 0) {
                            if (!check_data_block_for_duplicates(double_indirect_block_num, i, fix, inode_refs)) {
                                isValid = false;
                                if (fix) {
                                    triple_indirect_block[j] = 0;
                                }
                            }
                            uint32_t *double_indirect_block = (uint32_t *)get_block(double_indirect_block_num);
                            if (double_indirect_block) {
                                int entries_per_double_indirect_block = BLOCK_SIZE / sizeof(uint32_t);
                                for (int k = 0; k < entries_per_double_indirect_block; k++) {
                                    uint32_t single_indirect_block_num = double_indirect_block[k];
                                    if (single_indirect_block_num != 0) {
                                        if (!check_data_block_for_duplicates(single_indirect_block_num, i, fix,inode_refs)) {
                                            isValid = false;
                                            if (fix) {
                                                double_indirect_block[k] = 0;
                                            }
                                        }
                                        uint32_t *single_indirect_block = (uint32_t *)get_block(single_indirect_block_num);
                                        if (single_indirect_block) {
                                            int entries_per_single_indirect_block = BLOCK_SIZE / sizeof(uint32_t);
                                            for (int m = 0; m < entries_per_single_indirect_block; m++) {
                                                uint32_t data_block_num = single_indirect_block[m];
                                                if (data_block_num != 0)
                                                    if (!check_data_block_for_duplicates(data_block_num, i, fix, inode_refs)) {
                                                        isValid = false;
                                                        if (fix) {
                                                            single_indirect_block[m] = 0;
                                                        }
                                                    }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    free(inode_refs);
    return isValid;
}


// 5. Bad Block Checker //22101328
bool check_bad_blocks(bool fix) {
    printf("\n=== Bad Block Check ===\n");
    
    bool isValid = true;
    
    for (int i = 0; i < INODE_COUNT; i++) {
        inode_t *inode = &inode_table[i];
        
        // Skip invalid inodes
        if (!is_inode_valid(inode)) {
            continue;
        }
        
        // Check direct block
        if (inode->direct_block >= TOTAL_BLOCKS) {
            printf("Error: Inode %d has bad direct block: %u\n", i, inode->direct_block);
            if (fix) {
                printf("Fixing: Setting direct block of inode %d to 0\n", i);
                inode->direct_block = 0;
            }
            isValid = false;
        }
        
        // Check single indirect block
        if (inode->single_indirect >= TOTAL_BLOCKS) {
            printf("Error: Inode %d has bad single indirect block: %u\n", i, inode->single_indirect);
            if (fix) {
                printf("Fixing: Setting single indirect block of inode %d to 0\n", i);
                inode->single_indirect = 0;
            }
            isValid = false;
        } else if (inode->single_indirect != 0) {
            uint32_t *indirect_block = get_block(inode->single_indirect);
            if (indirect_block) {
                int entries_per_block = BLOCK_SIZE / sizeof(uint32_t);
                for (int j = 0; j < entries_per_block; j++) {
                    uint32_t data_block_num = indirect_block[j];
                    if (data_block_num >= TOTAL_BLOCKS) {
                        printf("Error: Inode %d has bad data block %u in single indirect block\n", i, data_block_num);
                        if (fix) {
                            printf("Fixing: Setting invalid data block entry %d in single indirect block of inode %d to 0\n", j, i);
                            indirect_block[j] = 0;
                        }
                        isValid = false;
                    }
                }
            }
        }
        
        // Check double indirect block
        if (inode->double_indirect >= TOTAL_BLOCKS) {
            printf("Error: Inode %d has bad double indirect block: %u\n", i, inode->double_indirect);
            if (fix) {
                printf("Fixing: Setting double indirect block of inode %d to 0\n", i);
                inode->double_indirect = 0;
            }
            isValid = false;
        } else if (inode->double_indirect != 0) {
            uint32_t *double_indirect_block = get_block(inode->double_indirect);
            if (double_indirect_block) {
                int entries_per_block = BLOCK_SIZE / sizeof(uint32_t);
                for (int j = 0; j < entries_per_block; j++) {
                    uint32_t indirect_block_num = double_indirect_block[j];
                    if (indirect_block_num >= TOTAL_BLOCKS) {
                        printf("Error: Inode %d has bad indirect block %u in double indirect block\n", i, indirect_block_num);
                        if (fix) {
                            printf("Fixing: Setting invalid indirect block entry %d in double indirect block of inode %d to 0\n", j, i);
                            double_indirect_block[j] = 0;
                        }
                        isValid = false;
                    } else if (indirect_block_num != 0) {
                        uint32_t *indirect_block = get_block(indirect_block_num);
                        if (indirect_block) {
                            int entries_per_indirect_block = BLOCK_SIZE / sizeof(uint32_t);
                            for (int k = 0; k < entries_per_indirect_block; k++) {
                                uint32_t data_block_num = indirect_block[k];
                                if (data_block_num >= TOTAL_BLOCKS) {
                                    printf("Error: Inode %d has bad data block %u in double indirect block\n", i, data_block_num);
                                    if (fix) {
                                        printf("Fixing: Setting invalid data block entry %d in indirect block of inode %d to 0\n", k, i);
                                        indirect_block[k] = 0;
                                    }
                                    isValid = false;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Check triple indirect block
        if (inode->triple_indirect >= TOTAL_BLOCKS) {
            printf("Error: Inode %d has bad triple indirect block: %u\n", i, inode->triple_indirect);
            if (fix) {
                printf("Fixing: Setting triple indirect block of inode %d to 0\n", i);
                inode->triple_indirect = 0;
            }
            isValid = false;
        }  else if (inode->triple_indirect != 0) {
            uint32_t *triple_indirect_block = get_block(inode->triple_indirect);
            if (triple_indirect_block) {
                int entries_per_block = BLOCK_SIZE / sizeof(uint32_t);
                for (int j = 0; j < entries_per_block; j++) {
                    uint32_t double_indirect_block_num = triple_indirect_block[j];
                    if (double_indirect_block_num >= TOTAL_BLOCKS) {
                        printf("Error: Inode %d has bad double indirect block %u in triple indirect block\n", i, double_indirect_block_num);
                        if (fix) {
                            printf("Fixing: Setting invalid double indirect block entry %d in triple indirect block of inode %d to 0\n", j, i);
                            triple_indirect_block[j] = 0;
                        }
                        isValid = false;
                    } else if (double_indirect_block_num != 0) {
                        uint32_t *double_indirect_block = get_block(double_indirect_block_num);
                        if (double_indirect_block) {
                            int entries_per_double_indirect_block = BLOCK_SIZE / sizeof(uint32_t);
                            for (int k = 0; k < entries_per_double_indirect_block; k++) {
                                uint32_t single_indirect_block_num = double_indirect_block[k];
                                if (single_indirect_block_num >= TOTAL_BLOCKS) {
                                    printf("Error: Inode %d has bad single indirect block %u in triple indirect block\n", i, single_indirect_block_num);
                                    if (fix) {
                                        printf("Fixing: Setting invalid single indirect block entry %d in double indirect block of inode %d to 0\n", k, i);
                                        double_indirect_block[k] = 0;
                                    }
                                    isValid = false;
                                } else if (single_indirect_block_num != 0) {
                                    uint32_t *single_indirect_block = get_block(single_indirect_block_num);
                                    if (single_indirect_block) {
                                        int entries_per_single_indirect_block = BLOCK_SIZE / sizeof(uint32_t);
                                        for (int m = 0; m < entries_per_single_indirect_block; m++) {
                                            uint32_t data_block_num = single_indirect_block[m];
                                            if (data_block_num >= TOTAL_BLOCKS) {
                                                printf("Error: Inode %d has bad data block %u in triple indirect block\n", i, data_block_num);
                                                if (fix) {
                                                    printf("Fixing: Setting invalid data block entry %d in single indirect block of inode %d to 0\n", m, i);
                                                    single_indirect_block[m] = 0;
                                                }
                                                isValid = false;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return isValid;
}

/*
 * Main function
 */
int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <file_system_image> [--fix]\n", argv[0]);
        return 1;
    }
    
    char *image_file = argv[1];
    bool fix_errors = (argc == 3 && strcmp(argv[2], "--fix") == 0);
    
    // Load the file system image
    // Open in read/write mode for fixing
    FILE *file = fopen(image_file, "rb+"); 
    if (!file) {
        perror("Error opening file system image");
        return 1;
    }
    
    // Get the file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    
    // Check if the file size matches the expected size
    if (file_size != TOTAL_BLOCKS * BLOCK_SIZE) {
        fprintf(stderr, "Error: File system image size (%ld) doesn't match expected size (%d)\n", 
                file_size, TOTAL_BLOCKS * BLOCK_SIZE);
        fclose(file);
        return 1;
    }
    
    // Allocate memory for the file system image
    fs_image = malloc(file_size);
    if (!fs_image) {
        perror("Error allocating memory for file system image");
        fclose(file);
        return 1;
    }
    
    // Read the file system image into memory
    if (fread(fs_image, 1, file_size, file) != file_size) {
        perror("Error reading file system image");
        free(fs_image);
        fclose(file);
        return 1;
    }
    
    // Initialize global pointers
    superblock = (superblock_t *)get_block(SUPERBLOCK_NUM);
    inode_bitmap = get_block(INODE_BITMAP_BLOCK_NUM);
    data_bitmap = get_block(DATA_BITMAP_BLOCK_NUM);
    inode_table = (inode_t *)get_block(INODE_TABLE_START_BLOCK_NUM);
    block_ref_count = calloc(TOTAL_BLOCKS, sizeof(bool));
    if (!block_ref_count) {
        perror("Error allocating memory for block reference tracking");
        free(fs_image);
        fclose(file);
        return 1;
    }
    
    // Run consistency checks
    printf("VSFS Consistency Checker\n");
    printf("========================\n");
    printf("File system image: %s\n", image_file);
    printf("Mode: %s\n", fix_errors ? "Check and fix" : "Check only");
    
    bool sb_valid = validate_superblock(fix_errors);
    bool data_bitmap_valid = validate_data_bitmap(fix_errors);
    bool inode_bitmap_valid = validate_inode_bitmap(fix_errors);
    bool no_duplicates = check_duplicate_blocks(fix_errors);
    bool no_bad_blocks = check_bad_blocks(fix_errors);
    
    printf("\n=== Consistency Check Summary ===\n");
    printf("Superblock: %s\n", sb_valid ? "Valid" : "Errors found");
    printf("Data bitmap: %s\n", data_bitmap_valid ? "Valid" : "Errors found");
    printf("Inode bitmap: %s\n", inode_bitmap_valid ? "Valid" : "Errors found");
    printf("Duplicate blocks: %s\n", no_duplicates ? "None found" : "Errors found");
    printf("Bad blocks: %s\n", no_bad_blocks ? "None found" : "Errors found");
    
    bool fs_valid = sb_valid && data_bitmap_valid && inode_bitmap_valid && no_duplicates && no_bad_blocks;
    
    printf("\nOverall file system status: %s\n", fs_valid ? "CONSISTENT" : "ERRORS DETECTED");
    
    if (fix_errors && !fs_valid) {
        printf("\n=== Re-running Checks After Fixes ===\n");
        bool sb_valid_recheck = validate_superblock(false);
        bool data_bitmap_valid_recheck = validate_data_bitmap(false);
        bool inode_bitmap_valid_recheck = validate_inode_bitmap(false);
        bool no_duplicates_recheck = check_duplicate_blocks(false);
        bool no_bad_blocks_recheck = check_bad_blocks(false);
        
        bool fs_valid_recheck = sb_valid_recheck && data_bitmap_valid_recheck && 
                               inode_bitmap_valid_recheck && no_duplicates_recheck && 
                               no_bad_blocks_recheck;
        
        printf("\n=== Post-Fix Consistency Check Summary ===\n");
        printf("Superblock: %s\n", sb_valid_recheck ? "Valid" : "Errors remain");
        printf("Data bitmap: %s\n", data_bitmap_valid_recheck ? "Valid" : "Errors remain");
        printf("Inode bitmap: %s\n", inode_bitmap_valid_recheck ? "Valid" : "Errors remain");
        printf("Duplicate blocks: %s\n", no_duplicates_recheck ? "None found" : "Errors remain");
        printf("Bad blocks: %s\n", no_bad_blocks_recheck ? "None found" : "Errors remain");
        
        printf("\nPost-fix file system status: %s\n", 
               fs_valid_recheck ? "CONSISTENT" : "ERRORS REMAIN");
               
        if (!fs_valid_recheck) {
            printf("Warning: Some errors could not be fixed automatically!\n");
            printf("Consider running additional maintenance or backup your data.\n");
        }
        
        // Write the changes back to the file
        fseek(file, 0, SEEK_SET);
        if (fwrite(fs_image, 1, file_size, file) != file_size) {
            perror("Error writing corrected image to file");
        }
    }
    
    // Clean up
    free(block_ref_count);
    free(fs_image);
    fclose(file);
    
    return 0;
}