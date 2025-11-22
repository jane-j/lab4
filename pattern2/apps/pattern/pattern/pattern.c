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

#define P2_NUM_INTS 65536
#define P2_ITERATIONS 10
#define P2_START_INT_INDEX 0
#define P2_START_BYTE_OFFSET 0

void main(int argc, char *argv[]) {
  int handle, i, j;
  char *fname = "cachtest_large.dat";
  int block_buf[INTS_PER_BLOCK]; // 1KB buffer
  int read_int;
  int seek_pos, read_bytes;
  int error_count = 0;
  int expected_int;
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
  Printf("cachtest: Finished writing 2MB. Closing file.\n");
  file_close(handle);

  // ------------------------- Open file -------------------------
  Printf("cachtest: Re-opening file '%s' in 'r' mode for testing.\n", fname);
  handle = file_open(fname, "r");
  if(handle == FILE_FAIL) {
    Printf("cachtest: Failed to open file for reading!\n");
    Exit();
  }


  //------------------------- Pattern 2 -------------------------
  Printf("cachtest: Starting Pattern 2\n");
  error_count = 0;
  
  for (j = 0; j < P2_ITERATIONS; j++) {
    // Seek to the start of the 256KB working set
    seek_pos = file_seek(handle, P2_START_INT_INDEX, FILE_SEEK_SET);
    if (FILE_SUCCESS != seek_pos) {
      Printf("cachtest: Error: Seek failed for iteration %d! Pos: %d\n", j, P2_START_INT_INDEX);
      break;
    }

    for (i = 0; i < P2_NUM_INTS; i++) {
      read_bytes = file_read(handle, (void *)&read_int, INT_SIZE);
      expected_int = P2_START_INT_INDEX + i;

      if (read_bytes != INT_SIZE) {
        Printf("cachtest: Error: Incorrect bytes read at int index %d! Read %d bytes.\n", expected_int, read_bytes);
        error_count++;
        break; 
      }
      
      if (read_int != expected_int) {
        Printf("cachtest: Incorrect data read at int index %d! Expected %d, Got %d\n", expected_int, expected_int, read_int);
        error_count++;
      }
    }
  }
  Printf("cachtest: Pattern 2 complete. Total errors: %d\n", error_count);


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
