#ifndef __DFS_SHARED__
#define __DFS_SHARED__

typedef struct dfs_superblock {
  // STUDENT: put superblock internals here
  char valid;
  int block_size;
  int num_block;
  int start_block_inode;
  int num_inode;
  int start_block_fbv;
} dfs_superblock;

#define DFS_BLOCKSIZE 1024  // Must be an integer multiple of the disk blocksize

typedef struct dfs_block {
  char data[DFS_BLOCKSIZE];
} dfs_block;

typedef struct dfs_inode {
  // STUDENT: put inode structure internals here
  // IMPORTANT: sizeof(dfs_inode) MUST return 128 in order to fit in enough
  // inodes in the filesystem (and to make your life easier).  To do this, 
  // adjust the maximumm length of the filename until the size of the overall inode 
  // is 128 bytes.
  char valid;
  int file_size;
  char filename[71];
  uint32 direct_table[10];
  int num_indirect_blocks;
  int num_double_indirect_blocks;
} dfs_inode;

#define DFS_MAX_FILESYSTEM_SIZE 0x4000000  // 64MB
#define DFS_INODE_MAX_NUM 256
#define DFS_FBV_MAX_NUM_WORDS ((DFS_MAX_FILESYSTEM_SIZE / DFS_BLOCKSIZE) / 32)


#define DFS_FAIL -1
#define DFS_SUCCESS 1



#endif
