#include "ostraps.h"
#include "dlxos.h"
#include "traps.h"
#include "disk.h"
#include "dfs.h"

void print_dfs_block(dfs_block *b) {
  int m;
  printf("Printing block b\n");
  for(m = 0; m < DFS_BLOCKSIZE; m++) {
    printf("b.data[%d] = %c\n",m, b->data[m]);
  }
}

void RunOSTests() {
  uint32 i,j,m;
  int k;
  dfs_block *b, *d;

  // // STUDENT: run any os-level tests here
  DfsModuleInit();
  DfsPrintSuperblock();
  // DfsReadDiskSuperblock();
  // // DfsPrintInodeTable();
  DfsPrintFBVBlocks();

  i = DfsAllocateBlock();
  printf("Allocated block %d\n",i);
  //DfsPrintFBVBlocks();
  j = DfsAllocateBlock();
  printf("Allocated block %d\n",j);
  // printf("out\n");
  //DfsPrintFBVBlocks();

  for(m = 0; m < DFS_BLOCKSIZE; m++) {
    b->data[m] = 'e';
  }
  for(m = 0; m < 5; m++) {
    printf("bdata[%d] = %c\n", m, b->data[m]);
  }

  k = DfsWriteBlock(j, b);
  // if(k != DFS_BLOCKSIZE) printf("k != DFS_BLOCK\n");

  // k = DfsReadBlock(j, d);
  // if(k != DFS_BLOCKSIZE) printf("k read != DFS_BLOCK\n");

  // print_dfs_block(d);

  // DfsFreeBlock(i);
  // DfsFreeBlock(i);
  // DfsFreeBlock(j);

  // DfsPrintFBVBlocks();
}