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


#define P3_NUM_INTS 262144 
#define P3_NUM_REQUESTS 262144

static unsigned int lcg_seed = 1;

int get_num(int max_val) {
  lcg_seed = (lcg_seed * 1103515245 + 12345) & 0x7FFFFFFF;
  if (max_val == 0) return 0;
  return (int)lcg_seed % max_val;
}

void main(int argc, char *argv[]) {
  int handle, i, j;
  char *fname = "cachtest_large.dat";
  int block_buf[INTS_PER_BLOCK]; // 1KB buffer
  int read_int;
  int seek_pos, read_bytes;
  int error_count = 0;
  int expected_int;
  int written;
  int r;
  int int_index;

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
  
  //------------------------- Pattern 3 -------------------------
  Printf("cachtest: Starting Pattern 3\n");
  error_count = 0;
  for (i = 0; i < P3_NUM_REQUESTS; i++) {
    r = get_num(1000); 

    if (r < 300) { 
      int_index = 0;
    } 
    else if (r < 600) { 
      int_index = 1 + get_num(255); 
    } 
    else if (r < 800) { 
      int_index = 256 + get_num(768); 
    } 
    else if (r < 950) { 
      int_index = 1024 + get_num(3072); 
    } 
    else { 
      int_index = 4096 + get_num(P3_NUM_INTS - 4096);
    }
    
    /* Now seek to that int's byte offset and read it */
    seek_pos = file_seek(handle, int_index * INT_SIZE, FILE_SEEK_SET);
    if (FILE_SUCCESS != seek_pos) {
      Printf("cachtest: Error: Seek failed for int index %d!\n", int_index);
      error_count++;
      break;
    }

    read_bytes = file_read(handle, (void *)&read_int, INT_SIZE);
    expected_int = int_index;

    if (read_bytes != INT_SIZE) {
      Printf("cachtest: Error: Incorrect bytes read at int index %d! Read %d bytes.\n", expected_int, read_bytes);
      error_count++;
    }
    
    if (read_int != expected_int) {
      Printf("cachtest: Incorrect data read at int index %d! Expected %d, Got %d\n", expected_int, expected_int, read_int);
      error_count++;
    }
  }
  Printf("cachtest: Pattern 3 complete. Total errors: %d\n", error_count);

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
