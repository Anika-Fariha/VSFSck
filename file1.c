//22101305
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 64
#define SUPERBLOCK_BLOCK 0
#define INODE_BITMAP_BLOCK 1
#define DATA_BITMAP_BLOCK 2
#define INODE_TABLE_START_BLOCK 3
#define INODE_TABLE_BLOCKS 5
#define DATA_BLOCK_START 8
#define INODES_PER_BLOCK (BLOCK_SIZE / 256)
#define TOTAL_INODES (INODES_PER_BLOCK * INODE_TABLE_BLOCKS)

#define MAGIC_NUMBER 0xD34D

typedef struct {
    uint16_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t inode_bitmap;
    uint32_t data_bitmap;
    uint32_t inode_table;
    uint32_t data_blocks_start;
    uint32_t inode_size;
    uint32_t inode_count;
    uint8_t reserved[4058];
} __attribute__((packed)) SuperBlock;

typedef struct {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t file_size;
    uint32_t access_time;
    uint32_t creation_time;
    uint32_t modification_time;
    uint32_t deletion_time;
    uint32_t links_count;
    uint32_t blocks;
    uint32_t direct_ptr;
    uint32_t single_indirect;
    uint32_t double_indirect;
    uint32_t triple_indirect;
    uint8_t reserved[156];
} __attribute__((packed)) Inode;

uint8_t disk[TOTAL_BLOCKS * BLOCK_SIZE];
SuperBlock sb;
uint8_t *inode_bitmap, *data_bitmap;
Inode *inode_table;

void load_image(const char *filename) {
    FILE *fp = fopen(filename, "rb+");
    if (!fp) {
        perror("Error opening image");
        exit(1);
    }
    fread(disk, sizeof(disk), 1, fp);
    fclose(fp);

    memcpy(&sb, &disk[0], sizeof(SuperBlock));
    inode_bitmap = &disk[INODE_BITMAP_BLOCK * BLOCK_SIZE];
    data_bitmap = &disk[DATA_BITMAP_BLOCK * BLOCK_SIZE];
    inode_table = (Inode *) &disk[INODE_TABLE_START_BLOCK * BLOCK_SIZE];
}
    
void check_and_fix_inode_bitmap() {
    for (int i = 0; i < TOTAL_INODES; i++) {
        Inode *inode = &inode_table[i];
        int valid = (inode->links_count > 0 && inode->deletion_time == 0);
        int bitmap_set = inode_bitmap[i / 8] & (1 << (i % 8));

        if (valid && !bitmap_set) {
            printf("Fixing inode bitmap: inode %d should be marked used\n", i);
            inode_bitmap[i / 8] |= (1 << (i % 8));
        } else if (!valid && bitmap_set) {
            printf("Fixing inode bitmap: inode %d should not be marked used\n", i);
            inode_bitmap[i / 8] &= ~(1 << (i % 8));
        }
    }
}void write_image(const char *filename) {
    FILE *fp = fopen(filename, "rb+");
    if (!fp) {
        perror("Error writing image");
        exit(1);
    }
    fwrite(disk, sizeof(disk), 1, fp);
    fclose(fp);
}int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <vsfs.img>\n", argv[0]);
        return 1;
    }

    load_image(argv[1]);

    printf("Starting VSFS Consistency Check...\n");

    
    check_and_fix_inode_bitmap();
    

    write_image(argv[1]);

    printf("VSFS Check Completed. Corrections written to image.\n");
    return 0;
}
