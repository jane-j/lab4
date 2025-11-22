#include "usertraps.h"
#include "misc.h"

#include "fdisk.h"

dfs_superblock sb;
dfs_inode inodes[DFS_INODE_MAX_NUM];
uint32 fbv[DFS_FBV_MAX_NUM_WORDS];

int diskblocksize = 0; // These are global in order to speed things up
int disksize = 0;      // (i.e. fewer traps to OS to get the same number)
int num_filesystem_blocks = 0;

static uint32 negativeone = 0xFFFFFFFF;
static inline uint32 invert(uint32 n) { return n ^ negativeone; }

int FdiskWriteBlock(uint32 blocknum, dfs_block *b); //You can use your own function. This function 
//calls disk_write_block() to write physical blocks to disk

void main (int argc, char *argv[])
{
	// STUDENT: put your code here. Follow the guidelines below. They are just the main steps. 
	// You need to think of the finer details. You can use bzero() to zero out bytes in memory
  int i, j;
  int end_idx, end_pos; //for fbv set up
  char * kill; char * k;
  char * ace;
  dfs_block b;

  //Initializations and argc check
  // if (argc != 2) {
  //   Printf("Usage: %s <number of processes to create\n", argv[0]);
  //   Exit();
  // }

  // Need to invalidate filesystem before writing to it to make sure that the OS
  // doesn't wipe out what we do here with the old version in memory
  // You can use dfs_invalidate(); but it will be implemented in Problem 2. You can just do 
  sb.valid = 0;

  disksize = disk_size();
  diskblocksize = disk_blocksize();
  num_filesystem_blocks = disksize / DFS_BLOCKSIZE;

  // Make sure the disk exists before doing anything else
  if(disk_create() != 1) {
    Printf("Disk Creation Failed! Aborting!\n");
    Exit();
  }

  // Write all inodes as not in use and empty (all zeros)
  for(i = 0; i  < DFS_INODE_MAX_NUM; i++) {
    inodes[i].inuse = 0;
    inodes[i].fileSize = 0;
  }
  
  // Next, setup free block vector (fbv) and write free block vector to the disk
  //filesystem blocks for boot, superblock, inodes and fbv should be marked as not free
  //That is blocks 0 to last_FBV_block (inclusive), and the last block is also not free
  end_idx = FDISK_FBV_BLOCK_END/32;
  end_pos = FDISK_FBV_BLOCK_END % 32;

  Printf("FBV_BLOCK_END %d num_inodes %d, endidx %d, endpos %d\n", FDISK_FBV_BLOCK_END, FDISK_NUM_INODES, end_idx, end_pos);

  for(i = 0; i < DFS_FBV_MAX_NUM_WORDS; i++) {
     if (i < end_idx) {
      fbv[i] = 0;
    } else if (i == end_idx) {
      fbv[i] = invert((1 << (end_pos + 1)) - 1);
    } else if (i == (DFS_FBV_MAX_NUM_WORDS - 1)) {
      fbv[i] = 0x7FFFFFFF;
    } else {
      fbv[i] = 0xFFFFFFFF; // Mark all other blocks as free
    }
    //Printf("FBV[%d] = 0x%x\n", i, fbv[i]);
  }

  // Printf("DFS_FBV_MAX_NUM_WORDS 0x%x\n", DFS_FBV_MAX_NUM_WORDS);
  // //i iterates through fislesystem block numbers
  for(i = FDISK_FBV_BLOCK_START; i < (FDISK_FBV_BLOCK_END + 1); i++) {
    //for(j = (i - FDISK_FBV_BLOCK_START) * DFS_BLOCKSIZE; j < (i - FDISK_FBV_BLOCK_START + 1) * DFS_BLOCKSIZE; j++) {
      j = (i - FDISK_FBV_BLOCK_START) * DFS_BLOCKSIZE;
      bcopy((fbv + j), b.data, DFS_BLOCKSIZE);
      for(j = 0; j < DFS_BLOCKSIZE; j++) {
        Printf("b.data[%d] = %c\n", j, b.data[j]);
      }
      FdiskWriteBlock(i, &b);
    //}
  }

  // Finally, setup superblock as valid filesystem and write superblock and boot record to disk:
  sb.fsBlockSize = DFS_BLOCKSIZE;
  sb.numFsBlocks = num_filesystem_blocks;
  sb.firstInodeBlock = FDISK_INODE_BLOCK_START;
  sb.numInodes = DFS_INODE_MAX_NUM;
  sb.firstFBVBlock = FDISK_FBV_BLOCK_START;
  sb.valid = 1;

  *b.data = NULL;
  bzero(b.data, DFS_BLOCKSIZE);
  FdiskWriteBlock(FDISK_BOOT_FILESYSTEM_BLOCKNUM, &b);

  ditoa(sb.valid, k); dstrcat(b.data, k);
  ditoa(sb.fsBlockSize, k); dstrcat(b.data, k);
  ditoa(sb.numFsBlocks, k); dstrcat(b.data, k);
  ditoa(sb.firstInodeBlock, k); dstrcat(b.data, k);
  ditoa(sb.numInodes, k); dstrcat(b.data, k);
  ditoa(sb.firstFBVBlock, k); dstrcat(b.data, k);

  for(i = 0; i < DFS_BLOCKSIZE; i++) {
    Printf("bdata[%d] = %c\n", i, b.data[i]);
  }

  FdiskWriteBlock(FDISK_SUPERBLOCK_BLOCKNUM, &b);
  Printf("fdisk (%d): Formatted DFS disk for %d bytes.\n", getpid(), disksize);
}

int FdiskWriteBlock(uint32 blocknum, dfs_block *b) {
  // STUDENT: put your code here
  uint32 phy_blocknum;
  int i;

  Printf("We here %d\n", blocknum);

  for(i = 0; i < (DFS_BLOCKSIZE/diskblocksize); i++) {
    phy_blocknum = blocknum*(DFS_BLOCKSIZE/diskblocksize) + i;
    Printf("phy block %d\n", phy_blocknum);
    if(disk_write_block(phy_blocknum, (b->data + i*diskblocksize)) == -1) {
      Printf("FDiskWriteBlock: Disk write failed!\n");
      Exit();
    }
  }
  return 1;  
}
