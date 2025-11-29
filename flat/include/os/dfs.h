#ifndef __DFS_H__
#define __DFS_H__

#include "dfs_shared.h"

#define DFS_SUPERBLOCK_PHYBLOCKNUM 4
#define DFS_REDUNDANT_SB_PHYBLOCKNUM 65535*4

//static inline uint32 invert(uint32);

void DfsModuleInit();
void DfsInvalidate();
int DfsOpenFileSystem();
int DfsCloseFileSystem();
int DfsAllocateBlock();
int DfsFreeBlock(uint32);
int DfsReadBlock(uint32, dfs_block *);
int DfsWriteBlock(uint32, dfs_block *);

uint32 DfsInodeFilenameExists(char *);
uint32 DfsInodeOpen(char *);
int DfsInodeDelete(uint32);
int DfsInodeReadBytes(uint32, void *, int, int);
int DfsInodeWriteBytes(uint32, void *, int, int);
uint32 DfsInodeFilesize(uint32);
uint32 DfsInodeAllocateVirtualBlock(uint32, uint32);
uint32 DfsInodeTranslateVirtualToFilesys(uint32, uint32);

int DfsFreeDirectBlocks(uint32 *, int);
int DfsFreeIndirectBlocks(uint32);
int DfsInodeFileRename(char *, char *);

// void DfsPrintSuperblock();
// void DfsPrintInode(dfs_inode *);
// void DfsPrintInodeTable();
// void DfsPrintFBVBlocks();
// void DfsReadDiskSuperblock();


#define DFS_DIR_TABLE_SIZE 10
#define DFS_INDIR_TABLE_SIZE (DFS_BLOCKSIZE/sizeof(uint32))
#define DFS_DOUBLE_INDIR_TABLE_SIZE (DFS_INDIR_TABLE_SIZE * DFS_INDIR_TABLE_SIZE)
#define DFS_MAX_NUM_VIRTUAL_BLOCKS (DFS_DIR_TABLE_SIZE + DFS_INDIR_TABLE_SIZE + DFS_DOUBLE_INDIR_TABLE_SIZE)

#endif
