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

void FillBuffer(char *buf, int size, char c) {
  int i;
  for (i = 0; i < size; i++) buf[i] = c;
}

void RunOSTests() {
  int ret;
  int h1, h2;
  dfs_block block;
  char smallbuf[37];
  char readbuf[50];

  // // // STUDENT: run any os-level tests here

  // ret = DfsReadBlock(95, &block);
  // printf("ret %d\n", ret);
  
  printf("=== Testing Simple File Create ===\n");
  h1 = DfsInodeOpen("file1.txt");
  printf("Opened inode handle: %d\n", h1);

  printf("=== Writing Non-Block-Aligned Bytes ===\n");
  FillBuffer(smallbuf, 40, 'A');
  ret = DfsInodeWriteBytes(h1, smallbuf, 0, 40);
  printf("Wrote %d bytes (expected 37)\n", ret);

  // printf("=== Reading Back Non-Block-Aligned Data ===\n");
  // bzero(readbuf, sizeof(readbuf));
  // ret = DfsInodeReadBytes(h1, readbuf, 0, 37);
  // printf("Read %d bytes: %.*s\n", ret, 37, readbuf);

  // printf("=== Testing Larger File (Indirect Blocks) ===\n");
  // h2 = DfsInodeOpen("bigfile.bin");
  // printf("Opened big file handle: %d\n", h2);

  // // Allocate large buffer enough to enter indirect addressing
  // int bigsize = 30000; // adjust > #direct blocks * block size
  // char *bigbuf = malloc(bigsize);
  // FillBuffer(bigbuf, bigsize, 'X');

  // ret = DfsInodeWriteBytes(h2, bigbuf, 0, bigsize);
  // printf("Wrote %d bytes to bigfile.bin\n", ret);

  // printf("=== Reading Portion of Large File ===\n");
  // char verifybuf[64];
  // ret = DfsInodeReadBytes(h2, verifybuf, 15000, 64);
  // printf("Read %d bytes from large file\n", ret);

  // printf("=== Checking File Sizes ===\n");
  // printf("file1.txt size = %d\n", DfsInodeFilesize(h1));
  // printf("bigfile.bin size = %d\n", DfsInodeFilesize(h2));

  // printf("=== Testing Block Allocation and Free ===\n");
  // int newblk = DfsAllocateBlock();
  // printf("Allocated block number: %d\n", newblk);

  // printf("Writing something to new block\n");
  // FillBuffer((char *)&block, sizeof(block), 'B');
  // DfsWriteBlock(newblk, &block);

  // printf("Reading back block\n");
  // DfsReadBlock(newblk, &block);

  // printf("Freeing allocated block\n");
  // DfsFreeBlock(newblk);

  // printf("=== Testing Delete File ===\n");
  // ret = DfsInodeDelete(h1);
  // printf("Deleted file1.txt result = %d\n", ret);

  // printf("=== Closing File System ===\n");
  // ret = DfsCloseFileSystem();
  // printf("Close result: %d\n", ret);

  // printf("=== Reopening to Test Persistence ===\n");
  // DfsOpenFileSystem();
  // int h3 = DfsInodeOpen("bigfile.bin");  // should exist
  // printf("Persistence test handle: %d\n", h3);

  // memset(readbuf, 0, sizeof(readbuf));
  // ret = DfsInodeReadBytes(h3, readbuf, 0, 20);
  // printf("Read after restart: %.*s\n", ret, readbuf);

  // printf("=== End of Tests ===\n");
  // return 0;
}