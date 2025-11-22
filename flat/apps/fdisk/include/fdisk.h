#ifndef __FDISK_H__
#define __FDISK_H__

typedef unsigned int uint32;

#include "dfs_shared.h" // This gets us structures and #define's from main filesystem driver

#define FDISK_INODE_BLOCK_START 2 // Starts after super block (which is in file system block 0, physical block 1)
#define FDISK_INODE_NUM_BLOCKS 32 // Number of file system blocks to use for inodes
#define FDISK_NUM_INODES  ((FDISK_INODE_NUM_BLOCKS * DFS_BLOCKSIZE)/sizeof(dfs_inode))// or 256?//STUDENT: define this
#define FDISK_FBV_BLOCK_START (FDISK_INODE_NUM_BLOCKS + FDISK_INODE_BLOCK_START)//STUDENT: define this
#define FDISK_BOOT_FILESYSTEM_BLOCKNUM 0 // Where the boot record and superblock reside in the filesystem
#define FDISK_SUPERBLOCK_BLOCKNUM 1

#ifndef NULL
#define NULL (void *)0x0
#endif

//STUDENT: define additional parameters here, if any
#define FDISK_FBV_BLOCK_END (FDISK_FBV_BLOCK_START + ((DFS_FBV_MAX_NUM_WORDS*4) / DFS_BLOCKSIZE) - 1)

static inline uint32 invert(uint32 n);

#endif
