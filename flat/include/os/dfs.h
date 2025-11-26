#ifndef __DFS_H__
#define __DFS_H__

#include "dfs_shared.h"

#define DFS_SUPERBLOCK_PHYBLOCKNUM 4

static inline uint32 invert(uint32 n);

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

void DfsPrintSuperblock();
void DfsPrintInode(dfs_inode *);
void DfsPrintInodeTable();
void DfsPrintFBVBlocks();
void DfsReadDiskSuperblock();

int compare_strings(char *str1, char *str2);

#define DFS_DIR_TRANS_TABLE_SIZE 10

#endif
