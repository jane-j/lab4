#ifndef __DFS_SHARED__
#define __DFS_SHARED__

typedef struct dfs_superblock {
  // STUDENT: put superblock internals here
  int valid;
  int fs_blocksize;
  int num_fs_blocks;
  int start_block_inode;
  int num_inode;
  int start_block_fbv;
  int start_block_data;
} dfs_superblock;

#define DFS_BLOCKSIZE 1024  // Must be an integer multiple of the disk blocksize
#define DFS_MAX_FILENAME_LENGTH 72
#define DFS_MAX_DIR_TABLE_SIZE 10

typedef struct dfs_block {
  char data[DFS_BLOCKSIZE];
} dfs_block;

typedef struct dfs_inode {
  // STUDENT: put inode structure internals here
  // IMPORTANT: sizeof(dfs_inode) MUST return 128 in order to fit in enough
  // inodes in the filesystem (and to make your life easier).  To do this, 
  // adjust the maximumm length of the filename until the size of the overall inode 
  // is 128 bytes.
  char inuse;
  int filesize;
  char filename[DFS_MAX_FILENAME_LENGTH];
  uint32 direct_table[DFS_MAX_DIR_TABLE_SIZE];
  int indirect_block;
  int double_indirect_block;
} dfs_inode;

#define DFS_MAX_FILESYSTEM_SIZE 0x4000000  // 64MB
#define DFS_INODE_MAX_NUM 256
#define DFS_FBV_MAX_NUM_WORDS ((DFS_MAX_FILESYSTEM_SIZE / DFS_BLOCKSIZE) / 32)

#define DFS_INODE_BLOCK_START 2 // Starts after super block (which is in file system block 0, physical block 1)
#define DFS_INODE_NUM_BLOCKS 32 // Number of file system blocks to use for inodes
#define DFS_INODE_BLOCK_END (DFS_INODE_BLOCK_START + DFS_INODE_NUM_BLOCKS - 1)
#define DFS_FBV_BLOCK_START 34 //(DFS_INODE_NUM_BLOCKS + DFS_INODE_BLOCK_START)//STUDENT: define this
#define DFS_FBV_BLOCK_END 41
#define DFS_FBV_NUM_BLCOKS (DFS_FBV_BLOCK_END - DFS_FBV_BLOCK_START + 1)
#define DFS_BOOT_FILESYSTEM_BLOCKNUM 0 // Where the boot record and superblock reside in the filesystem
#define DFS_SUPERBLOCK_BLOCKNUM 1
#define DFS_REDUNDANT_SB_BLOCKNUM ((DFS_MAX_FILESYSTEM_SIZE / DFS_BLOCKSIZE) - 1)


#define DFS_FAIL -1
#define DFS_SUCCESS 1



#endif
