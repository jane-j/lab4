#include "usertraps.h"
#include "misc.h"

#include "fdisk.h"

dfs_superblock sb;
dfs_inode inodes[DFS_INODE_MAX_NUM];
uint32 fbv[DFS_FBV_MAX_NUM_WORDS];

int diskblocksize = 0; // These are global in order to speed things up
int disksize = 0;      // (i.e. fewer traps to OS to get the same number)
static uint32	negativeone = 0xffffffff;

int FdiskWriteBlock(uint32 blocknum, dfs_block *b); //You can use your own function. This function 
//calls disk_write_block() to write physical blocks to disk

static
inline
uint32
invert (uint32 n)
{
  return (n ^ negativeone);
}

void main (int argc, char *argv[])
{
	// STUDENT: put your code here. Follow the guidelines below. They are just the main steps. 
	// You need to think of the finer details. You can use bzero() to zero out bytes in memory

  int i;
  int j;
  uint32 mask_32;
  dfs_block temp_block;

  //Initializations and argc check
  if (argc != 1) {
    Exit();
  }
  
  // Need to invalidate filesystem before writing to it to make sure that the OS
  // doesn't wipe out what we do here with the old version in memory
  // You can use dfs_invalidate(); but it will be implemented in Problem 2. You can just do 

  //superblock setup
  dfs_invalidate();
  sb.fs_blocksize = DFS_BLOCKSIZE;
  sb.num_fs_blocks = DFS_MAX_FILESYSTEM_SIZE / DFS_BLOCKSIZE;
  sb.start_block_inode = FDISK_INODE_BLOCK_START;
  sb.num_inode = FDISK_NUM_INODES;
  sb.start_block_fbv = FDISK_FBV_BLOCK_START;
  sb.start_block_data = DFS_FBV_BLOCK_END + 1;

  Printf("start block data %d\n", sb.start_block_data);

  disksize = disk_size();
  diskblocksize = disk_blocksize();
  // num_filesystem_blocks = 

  // Make sure the disk exists before doing anything else
  if(disk_create() == DISK_FAIL) {
    Printf("fdisk: Error creating disk file. Aborting!\n");
    Exit();
  }

  // Write all inodes as not in use and empty (all zeros)
  // Next, setup free block vector (fbv) and write free block vector to the disk
  // Finally, setup superblock as valid filesystem and write superblock and boot record to disk: 
  // boot record is all zeros in the first physical block, and superblock structure goes into the second physical block
  
  
  //Inode setup
  for(i = 0; i <= DFS_INODE_MAX_NUM - 1; i++)
  {
    inodes[i].inuse = 0;
    inodes[i].filesize = 0;
    bzero(inodes[i].filename, DFS_MAX_FILENAME_LENGTH);
    bzero((char *)inodes[i].direct_table, sizeof(inodes[i].direct_table));
    inodes[i].indirect_block = 0;
    inodes[i].double_indirect_block = 0;
  }

  //FBV setup
  for(i = 0; i <= DFS_FBV_MAX_NUM_WORDS - 2; i++)
  {
    fbv[i] = 0xFFFFFFFF;
  }
  
  fbv[DFS_FBV_MAX_NUM_WORDS - 1] = 0x7FFFFFFF;

  for(i = 0; i <= DFS_FBV_BLOCK_END; i++)
  {
    j = i / 32;
    mask_32 = invert((uint32)((0x1) << (i % 32)));
    fbv[j] = fbv[j] & mask_32; 
  }

  sb.valid = 1;

  bzero(temp_block.data, DFS_BLOCKSIZE);

  Printf("sizeof inode %d\n ", sizeof(dfs_inode));

  //Writing to boot record block
  if(FdiskWriteBlock(FDISK_BOOT_FILESYSTEM_BLOCKNUM, &temp_block) == DFS_FAIL) {
    Printf("fdisk: Error writing boot record to disk.\n");
    Exit();
  }
  
  // //Resetting all the data blocks
  // for(i = 42; i <= 65534; i++) {
  //   if(FdiskWriteBlock(i, &temp_block) == DFS_FAIL) {
  //     // Printf("fdisk (%d): Error writing zero blocks to disk.\n", getpid());
  //     Exit();
  //   }
  // }

  //Writing superblock
  bcopy((char *)&sb, temp_block.data, sizeof(dfs_superblock));
  if(FdiskWriteBlock(DFS_SUPERBLOCK_BLOCKNUM, &temp_block) == DFS_FAIL) {
    Printf("fdisk: Error writing superblock to disk\n");
    Exit();
  }
  if(FdiskWriteBlock(DFS_REDUNDANT_SB_BLOCKNUM, &temp_block) == DFS_FAIL) {
    Printf("fdisk: Error writing redundant superblock to disk\n");
    Exit();
  }

  //Writing inodes
  for(i = 0; i < FDISK_INODE_NUM_BLOCKS; i++) {
    bzero(temp_block.data, sizeof(dfs_block));
    bcopy((char *)(inodes + (i * (DFS_BLOCKSIZE / 128))), temp_block.data, DFS_BLOCKSIZE);

    if(FdiskWriteBlock(FDISK_INODE_BLOCK_START + i, &temp_block) == DFS_FAIL) {
      Printf("fdisk: Error writing inode blocks to disk.\n");
      Exit();
    }
  }


  for(i = 0; i < DFS_FBV_NUM_BLCOKS; i++) {
    bzero(temp_block.data, sizeof(dfs_block));
    bcopy((char *)(fbv + (i * (DFS_BLOCKSIZE / 4))), temp_block.data, DFS_BLOCKSIZE);
    if(FdiskWriteBlock(FDISK_FBV_BLOCK_START + i, &temp_block) == DFS_FAIL) {
      Printf("fdisk: Error writing fbv blocks to disk.\n");
      Exit();
    }
  }
  
  Printf("fdisk (%d): Formatted DFS disk for %d bytes.\n", getpid(), disksize);
}

int FdiskWriteBlock(uint32 blocknum, dfs_block *b) {
  // STUDENT: put your code here
  int i;
  int physical_block_num = DFS_BLOCKSIZE / diskblocksize;
  int data_index;
  
  for(i = 0; i <= physical_block_num - 1; i++) {
    data_index = i * diskblocksize;
    
    if(disk_write_block(blocknum * physical_block_num + i, &b->data[data_index]) == DISK_FAIL) {
      return DFS_FAIL;
    }
  }

  return DFS_SUCCESS;
}
