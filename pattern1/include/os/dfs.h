#ifndef __DFS_H__
#define __DFS_H__

#include "dfs_shared.h"
#include "queue.h"

typedef struct cache_block { 
  char valid;
  char dirty;
  int age;
  int blocknum; 
  char data[DFS_BLOCKSIZE];
  Link *l;
} cache_block;

#define CACHE_SIZE 128
#define CACHE_WAYS CACHE_SIZE
#define CACHE_SETS (CACHE_SIZE / CACHE_WAYS)
#define CACHE_IDX_MASK (CACHE_SETS - 1)
#define CACHE_TAG_MASK (0xFFFFFFFF - CACHE_IDX_MASK)
#define MAX_WND 64
#define DEF_WND 8
#define DFS_DISK_ACCESS_LATENCY 5 //ms

void DfsModuleInit();
void DfsInvalidate();
int DfsOpenFileSystem();
int DfsCloseFileSystem();
int DfsAllocateBlock();
int DfsFreeBlock(int blocknum);
int DfsReadBlock(int blocknum, dfs_block *b);
int DfsWriteBlock(int blocknum, dfs_block *b);

int DfsInodeFilenameExists(char *filename);
int DfsInodeOpen(char *filename);
int DfsInodeDelete(int handle);
int DfsInodeReadBytes(int handle, void *mem, int start_byte, int num_bytes);
int DfsInodeWriteBytes(int handle, void *mem, int start_byte, int num_bytes);
int DfsInodeFilesize(int handle);
int DfsInodeAllocateVirtualBlock(int handle, int virtual_blocknum);
int DfsInodeTranslateVirtualToFilesys(int handle, int virtual_blocknum);
int DfsInodeRename(char *oldname, char *newname);
int DfsReadBlockUncached(int blocknum, dfs_block *b);
int DfsWriteBlockUncached(int blocknum, dfs_block *b);

int DfsCacheHit(int blocknum);
int DfsCacheAllocateSlot(int blocknum);
int DfsCacheFlush();
int DfsCacheReplacementPolicy();

#define DFS_SUPERBLOCK_PHY_BLOCKNUM 4
#define DFS_REDUNDANT_SB_PHY_BLOCKNUM 65535*4

#endif
