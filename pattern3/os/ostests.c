#include "ostraps.h"
#include "dlxos.h"
#include "traps.h"
#include "disk.h"
#include "dfs.h"
#include "ostests.h"
 
static char test_block[NUMBYTES];
static char test_block2[NUMBYTES];
static char test_buffer[NUMBYTES_2];
static char test_buffer2[NUMBYTES_2];
 
void RunOSTests() {
  TestBasic();
  TestUnaligned();
  TestSparse();
  TestLargeFile();
  TestDeleteAndReopen();
  TestPersistence();
}
 
// write 4 complete blocks and compare the 3rd block
void TestBasic() {
  uint32 inode;
  int i;
  int r;
  int w;
  int filesize;
 
  printf("runostests: starting basic test\n");
 
  inode = DfsInodeOpen("ece695-basic");
 
  if(inode == DFS_FAIL)
  {
    printf("runostests: DfsInodeOpen failed\n");
    GracefulExit();
  }
 
  for (i = 0; i < NUMBYTES; i++)
  {
    test_block[i] = i;
  }
 
  for (i = 0; i < 4; i++)
  {
    w = DfsInodeWriteBytes(inode, test_block, i * NUMBYTES, NUMBYTES);
 
    if(w == DFS_FAIL)
    {
      printf("runostests: DfsInodeWriteBytes failed\n");
      GracefulExit();
    }
    else
    {
      if (w != NUMBYTES)
      {
        printf("runostests: DfsInodeWriteBytes wrote wrong size %d\n", w);
        GracefulExit();
      }
    }
  }
 
  filesize = DfsInodeFilesize(inode);
  if(filesize == DFS_FAIL)
  {
    printf("runostests: DfsInodeFilesize failed\n");
    GracefulExit();
  }
  else
  {
    if (filesize != 4 * NUMBYTES)
    {
      printf("runostests: DfsInodeFilesize wrong size %d\n", filesize);
      GracefulExit();
    }
  }
 
  r = DfsInodeReadBytes(inode, test_block2, 2 * NUMBYTES, NUMBYTES);
 
  if(r == DFS_FAIL)
  {
    printf("runostests: DfsInodeReadBytes failed\n");
    GracefulExit();
  }
  else
  {
    if (r != NUMBYTES)
    {
      printf("runostests: DfsInodeReadBytes read wrong size %d\n", r);
      GracefulExit();
    }
  }
 
  for (i = 0; i < NUMBYTES; i++)
  {
    if (test_block[i] != test_block2[i])
    {
      printf("runostests: FAIL: test_block[%d] != test_block2[%d] (%d != %d)\n", i, i, test_block[i], test_block2[i]);
      GracefulExit();
    }
  }
 
  printf("runostests: basic test passed!\n");

  printf("cache test:\n");

  r = DfsInodeReadBytes(inode, test_block2, 2 * NUMBYTES, NUMBYTES);

  if(r == DFS_FAIL)
  {
    printf("runostests: DfsInodeReadBytes failed\n");
    GracefulExit();
  }
  else
  {
    if (r != NUMBYTES)
    {
      printf("runostests: DfsInodeReadBytes read wrong size %d\n", r);
      GracefulExit();
    }
  }
 
  for (i = 0; i < NUMBYTES; i++)
  {
    if (test_block[i] != test_block2[i])
    {
      printf("runostests: FAIL: test_block[%d] != test_block2[%d] (%d != %d)\n", i, i, test_block[i], test_block2[i]);
      GracefulExit();
    }
    else {
      printf("runostests: Matched: test_block[%d] != test_block2[%d] (%d != %d)\n", i, i, test_block[i], test_block2[i]);
    }
  }
}
 
 
void TestUnaligned() {
  uint32 inode;
  int i;
  int w;
  int r;
  int filesize;
 
  printf("runostests: starting unaligned test\n");
 
  inode = DfsInodeOpen("ece695-unaligned");
 
  if(inode == DFS_FAIL)
  {
    printf("runostests: DfsInodeOpen failed\n");
    GracefulExit();
  }
 
 
  for (i = 0; i < NUMBYTES_2; i++)
  {
    test_buffer[i] = i;
  }
 
  w = DfsInodeWriteBytes(inode, test_buffer, 0, 500);
  printf("runostests: write 1: wrote %d bytes at offset 0\n", w);
 
  if (w != 500)
  {
    printf("runostests: write 1 size mismatch: expected 500, got %d\n", w);
    GracefulExit();
  }
 
  w = DfsInodeWriteBytes(inode, test_buffer + 500, 500, 700);
  printf("runostests: write 2: wrote %d bytes at offset 500\n", w);
 
  if (w != 700)
  {
    printf("runostests: write 2 size mismatch: expected 700, got %d\n", w);
      GracefulExit();
  }
 
  w = DfsInodeWriteBytes(inode, test_buffer + 1200, 1200, 300);
  printf("runostests: write 3: wrote %d bytes at offset 1200\n", w);
 
  if (w != 300)
  {
    printf("runostests: write 3 size mismatch: expected 300, got %d\n", w);
    GracefulExit();
  }
 
  filesize = DfsInodeFilesize(inode);
  printf("runostests: filesize after write 3 = %d\n", filesize);
 
  // if (filesize != 1500)
  // {
  //   printf("runostests: filesize mismatch after 3: expected 1500, got %d\n", filesize);
  //   GracefulExit();
  // }
 
  r = DfsInodeReadBytes(inode, test_buffer2, 0, 1500);
  printf("runostests: read back %d bytes from offset 0\n", r);
 
  if (r != 1500)
  {
    printf("runostests: read size mismatch: expected 1500, got %d\n", r);
    GracefulExit();
  }
 
  for (i = 0; i < 1500; i++)
  {
    if (test_buffer[i] != test_buffer2[i])
    {
      printf("runostests: FAIL: test_buffer[%d] != test_buffer2[%d] (%d != %d)\n", i, i, test_buffer[i], test_buffer2[i]);
      GracefulExit();
    }
  }
 
  printf("runostests: unaligned test passed!\n");
}
 
void TestSparse() {
  uint32 inode;
  int i;
  int w;
  int r;
  int filesize;
  int temp;
  int block;
  int offset;
 
  printf("runostests: starting file hole test\n");
 
  inode = DfsInodeOpen("ece695-hole");
 
  if (inode == DFS_FAIL)
  {
    printf("runostests: DfsInodeOpen failed\n");
    GracefulExit();
  }
 
  for (i = 0; i < NUMBYTES_2; i++)
  {
    test_buffer[i] = i;
  }
 
  w = DfsInodeWriteBytes(inode, test_buffer + 1700, 1700, 300);
  printf("runostests: write 1: wrote %d bytes at offset 1700\n", w);
 
  if (w != 300)
  {
    printf("runostests: write 1 size mismatch: expected 300, got %d\n", w);
    GracefulExit();
  }
 
  filesize = DfsInodeFilesize(inode);
  printf("runostests: filesize after write 1 = %d\n", filesize);
 
  // if (filesize != 2000)
  // {
  //   printf("runostests: filesize mismatch: expected 2000, got %d\n", filesize);
  //   GracefulExit();
  // }
 
  r = DfsInodeReadBytes(inode, test_buffer2, 0, 2000);
  printf("runostests: read back %d bytes from offset 0\n", r);
 
  if (r != 2000)
  {
    printf("runostests: read size mismatch: expected 2000, got %d\n", r);
    GracefulExit();
  }
 
  for (i = 0; i < 2000; i++)
  {
    if (i >= 1700)
    {
      temp = test_buffer[i];  
    }
    else
    {
      temp = 0;
    }
 
    if (temp != test_buffer2[i])
    {
      printf("runostests: mismatch at byte %d: expected=%d, actual=%d\n", i, temp, test_buffer2[i]);
      GracefulExit();
    }
  }
 
  for (block = 249; block <= 500; block++) {
    for (i = 0; i < NUMBYTES; i++) {
      test_block[i] = i;
    }
 
    offset = block * NUMBYTES;
    w = DfsInodeWriteBytes(inode, test_block, offset, NUMBYTES);
    printf("runostests: write block %d at offset %d: wrote %d bytes\n", block, offset, w);
 
    if (w != NUMBYTES) {
      printf("runostests: block %d write size mismatch: expected %d, got %d\n", block, NUMBYTES, w);
      GracefulExit();
    }
  }
 
  for (block = 249; block <= 500; block++) {
 
    offset = block * NUMBYTES;
    r = DfsInodeReadBytes(inode, test_block2, offset, NUMBYTES);
   
    if (r != NUMBYTES) {
      printf("runostests: read block %d failed: expected %d, got %d\n", block, NUMBYTES, r);
      GracefulExit();
    }
 
    for (i = 0; i < NUMBYTES; i++) {
      if ((char)i != test_block2[i])
      {
        printf("runostests: mismatch: block=%d, offset_in_file=%d, index_in_block=%d, expected=%d, actual=%d\n", block, offset, i, (char)i, test_block2[i]);
        GracefulExit();
      }
    }
  }

  filesize = DfsInodeFilesize(inode);
  printf("runostests: filesize after write 1 = %d\n", filesize);
 
  printf("runostests: file with holes test passed!\n");
}
 
void TestLargeFile() {
  uint32 inode;
  int block;
  int i;
  int w;
  int r;
  int filesize;
  int expected_filesize;
  int offset;
 
  printf("runostests: starting large file test\n");
 
  inode = DfsInodeOpen("ece695-large");
 
  if(inode == DFS_FAIL)
  {
    printf("runostests: DfsInodeOpen failed\n");
    GracefulExit();
  }
 
  for (block = 0; block < LARGE_NUM_BLOCKS; block++)
  {
    for (i = 0; i < NUMBYTES; i++)
    {
      test_block[i] = i;
    }
 
    offset = block * NUMBYTES;
    w = DfsInodeWriteBytes(inode, test_block, offset, NUMBYTES);
 
    if (w != NUMBYTES)
    {
      printf("runostests: write failed at block=%d, offset=%d: expected=%d, got=%d\n", block, offset, NUMBYTES, w);
      GracefulExit();
    }
  }
 
  filesize = DfsInodeFilesize(inode);
  expected_filesize = LARGE_NUM_BLOCKS * NUMBYTES;
  printf("runostests: final filesize=%d, expected=%d\n", filesize, expected_filesize);
 
  // if (filesize != expected_filesize)
  // {
  //   printf("runostests: final filesize mismatch: expected=%d, got=%d\n", expected_filesize, filesize);
  //   GracefulExit();
  // }
 
  printf("runostests: starting verify\n");
 
  for (block = 0; block < LARGE_NUM_BLOCKS; block++)
  {
    offset = block * NUMBYTES;
    r = DfsInodeReadBytes(inode, test_block2, offset, NUMBYTES);
   
    if (r != NUMBYTES)
    {
      printf("runostests: read failed at block=%d, offset=%d: expected=%d, got=%d\n", block, offset, NUMBYTES, r);
      GracefulExit();
    }
 
    for (i = 0; i < NUMBYTES; i++)
    {
      if ((char)i != test_block2[i])
      {
        printf("runostests: mismatch: block=%d, offset_in_file=%d, index_in_block=%d, expected=%d, actual=%d\n", block, offset + i, i, (char)i, test_block2[i]);
        GracefulExit();
      }
    }
  }
 
  printf("runostests: large file test passed!\n");
}
 
void TestDeleteAndReopen() {
  uint32 inode;
  int i;
  int w;
  int r;
  int filesize_before;
  int filesize_after;
 
  printf("runostests: starting delete test\n");
 
  inode = DfsInodeOpen("ece695-delete");
  if(inode == DFS_FAIL)
  {
    printf("runostests: DfsInodeOpen failed\n");
    GracefulExit();
  }
 
  for (i = 0; i < NUMBYTES; i++)
  {
    test_block[i] = i;
  }
 
  w = DfsInodeWriteBytes(inode, test_block, 0, NUMBYTES);
  if(w == DFS_FAIL)
  {
    printf("runostests: DfsInodeWriteBytes failed\n");
    GracefulExit();
  }
  else
  {
    if (w != NUMBYTES)
    {
      printf("runostests: DfsInodeWriteBytes wrote wrong size %d\n", w);
      GracefulExit();
    }
    else
    {
      printf("runostests: wrote %d bytes to inode %d\n", w, inode);
    }
  }
 
  filesize_before = DfsInodeFilesize(inode);
 
  if(filesize_before == DFS_FAIL)
  {
    printf("runostests: DfsInodeFilesize failed\n");
    GracefulExit();
  }
  else
  {
    if (filesize_before != NUMBYTES)
    {
      printf("runostests: filesize_before wrong size %d\n", filesize_before);
      GracefulExit();
    }
  }
 
  i = DfsInodeDelete(inode);
 
  if(i == DFS_FAIL)
  {
    printf("runostests: DfsInodeDelete failed\n");
    GracefulExit();
  }
 
  inode = DfsInodeOpen("ece695-delete");
 
  if(inode == DFS_FAIL)
  {
    printf("runostests: reopen DfsInodeOpen failed\n");
    GracefulExit();
  }
 
  filesize_after = DfsInodeFilesize(inode);
 
  if(filesize_after == DFS_FAIL)
  {
    printf("runostests: filesize_after DfsInodeFilesize failed\n");
    GracefulExit();
  }
  else
  {
    if (filesize_after != 0)
    {
      printf("runostests: filesize_after wrong size %d\n", filesize_after);
      GracefulExit();
    }
  }
 
  r = DfsInodeReadBytes(inode, test_block2, 0, NUMBYTES);
 
  if(r == DFS_FAIL)
  {
    printf("runostests: read after delete returned DFS_FAIL as expected\n");
  }
  else
  {
    printf("runostests: read after delete should not return anything other than DFS_FAIL, but got %d\n", r);
    GracefulExit();
  }
 
  printf("runostests: delete test passed!\n");
}
 
void TestPersistence() {
  uint32 inode;
  char pattern[NUMBYTES];
  char readbuf[NUMBYTES];
  int i;
  int r;
  int filesize;
  char same = 1;
 
  printf("runostests: starting persistence test\n");
 
  inode = DfsInodeOpen("ece695-persist");
 
  if(inode == DFS_FAIL)
  {
    printf("runostests: DfsInodeOpen failed\n");
    GracefulExit();
  }
 
  for (i = 0; i < NUMBYTES; i++)
  {
    pattern[i] = i;
  }
 
  filesize = DfsInodeFilesize(inode);
 
  if (filesize < NUMBYTES)
  {
    printf("runostests: initializing persistent filesize=%d\n", filesize);
    r = DfsInodeWriteBytes(inode, pattern, 0, NUMBYTES);
 
    if(r == DFS_FAIL)
    {
      printf("runostests: initial DfsInodeWriteBytes failed\n");
      GracefulExit();
    }
    else
    {
      if (r != NUMBYTES)
      {
        printf("runostests: initial DfsInodeWriteBytes wrote wrong size %d\n", r);
        GracefulExit();
      }
    }
 
    printf("runostests: first run finished.\n");
    return;
  }
 
  r = DfsInodeReadBytes(inode, readbuf, 0, NUMBYTES);
  if(r == DFS_FAIL)
  {
    printf("runostests: DfsInodeReadBytes failed\n");
    GracefulExit();
  }
  else
  {
    if (r != NUMBYTES)
    {
      printf("runostests: DfsInodeReadBytes read wrong size %d\n", r);
      GracefulExit();
    }
  }
 
  for (i = 0; i < NUMBYTES; i++)
  {
    if (readbuf[i] != pattern[i])
    {
      same = 0;
      break;
    }
  }
 
  if (same)
  {
    printf("runostests: persistence test passed!\n");
  }
  else
  {
    printf("runostests: second run mismatch\n");
    r = DfsInodeWriteBytes(inode, pattern, 0, NUMBYTES);
 
    if(r == DFS_FAIL)
    {
      printf("runostests: rewrite DfsInodeWriteBytes failed\n");
      GracefulExit();
    }
    else
    {
      if (r != NUMBYTES)
      {
        printf("runostests: rewrite DfsInodeWriteBytes wrote wrong size %d\n", r);
        GracefulExit();
      }
    }
  }
}

