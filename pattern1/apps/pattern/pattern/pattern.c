#include "usertraps.h"
#include "misc.h"
#include "files_shared.h"

// Define file system and test parameters
#define BLOCK_SIZE 1024
#define INT_SIZE sizeof(int)
#define INTS_PER_BLOCK (BLOCK_SIZE / INT_SIZE) // 1024 / 4 = 256

// 2MB file = 2 * 1024 * 1024 bytes = 2,097,152 bytes
// Total blocks = 2 * 1024 = 2048 blocks
#define TOTAL_BLOCKS 2048
#define TOTAL_INTS (TOTAL_BLOCKS * INTS_PER_BLOCK)


#define P1_NUM_INTS 262144

void main(int argc, char *argv[]) {
  int handle, i, j;
  char *fname = "cachtest_large.dat";
  int block_buf[INTS_PER_BLOCK]; // 1KB buffer
  int read_int;
  int read_bytes;
  int error_count = 0;
  int written;


  // ------------------------- Setup, create a file -------------------------
  Printf("cachtest: Opening file '%s' in 'w' mode to write 1MB.\n", fname);
  handle = file_open(fname, "w");
  
  if(handle == FILE_FAIL) {
    Printf("cachtest: Failed to open file for writing!\n");
    Exit();
  }

  Printf("cachtest: Writing %d blocks (%dMB)...\n", TOTAL_BLOCKS, TOTAL_BLOCKS * BLOCK_SIZE / (1024*1024));
  for (i = 0; i < TOTAL_BLOCKS; i++) {
    // Fill the buffer with a predictable pattern
    for (j = 0; j < INTS_PER_BLOCK; j++) {
      // Each int in the file will be its own absolute index
      block_buf[j] = (i * INTS_PER_BLOCK) + j;
    }
    written = file_write(handle, (void *)block_buf, BLOCK_SIZE);
    if (written != BLOCK_SIZE) {
      Printf("cachtest: Error: Short write (%d) on block %d!\n", written, i);
      file_close(handle);
      Exit();
    }
  }
  Printf("cachtest: Finished writing 1MB. Closing file.\n");
  file_close(handle);

  // ------------------------- Open file -------------------------
  Printf("cachtest: Re-opening file '%s' in 'r' mode for testing.\n", fname);
  handle = file_open(fname, "r");
  if(handle == FILE_FAIL) {
    Printf("cachtest: Failed to open file for reading!\n");
    Exit();
  }

  // ------------------------- Pattern 1 -------------------------
  Printf("cachtest: Starting Pattern 1\n");
  error_count = 0;
  for (i = 0; i < P1_NUM_INTS; i++) {
    read_bytes = file_read(handle, (void *)&read_int, INT_SIZE);
    
    if (read_bytes != INT_SIZE) {
      Printf("cachtest (Scan): Error: Incorrect bytes read at int index %d! Read %d bytes.\n", i, read_bytes);
      error_count++;
      break; 
    }
    
    if (read_int != i) {
      Printf("cachtest: Incorrect data read at int index %d! Expected %d, Got %d\n", i, read_int);
      error_count++;
    }
  }
  Printf("cachtest: Pattern 1 complete. Total errors: %d\n", error_count);

  

  // -------------------------  Closing stuff -------------------------
  Printf("cachtest: Closing file.\n");
  file_close(handle);

  Printf("cachtest: Deleting file '%s'.\n", fname);
  if(file_delete(fname) == FILE_FAIL) {
    Printf("cachtest: Failed to delete file.\n");
  } else {
    Printf("cachtest: File deleted.\n");
  }

}
