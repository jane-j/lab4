#include "usertraps.h"
#include "misc.h"

#include "file_check.h"

void main() {
  unsigned int file_open(char *filename, char *mode);
int file_close(unsigned int handle);
int file_delete(char *filename);
int file_read(unsigned int handle, void *mem, int num_bytes);
int file_write(unsigned int handle, void *mem, int num_bytes);
int file_seek(unsigned int handle, int num_bytes, int from_where);

  int i, j, bytes = 1056;
  char * mem;
  char * read_mem;

  i = file_open("file_check.txt", "w");
  if(i != -1) Printf("Opened file with handle %d\n", i);
  else {
    Printf("FAILED to open file\n");
    Exit();
  }

  for(j = 0; j < bytes; j++) {
    mem[j] = 'n';
  }

  Printf("Writing %d bytes to the file with handle %d\n", bytes, i);
  j = file_write(i, mem, bytes);
  if(j != -1) Printf("Successfully wrote %d bytes\n", j);
  else {
    Printf("FAILED to write!\n");
    Exit();
  }

  Printf("Closing file with handle %d\n", i);
  j = file_close(i);
  if(j != -1 ) Printf("Closed file!\n");
  else {
    Printf("FAILED to close file\n");
    Exit();
  }

  Printf("Reopening file file_check.txt\n" );
  i = file_open("file_check.txt", "r");
  if(i != -1) Printf("Re-Opened file with handle %d\n", i);
  else {
    Printf("FAILED to re-open file\n");
    Exit();
  }

  Printf("Reading file with handle %d\n", i);
  j = file_read(i, read_mem, bytes);
  if(j!= -1) Printf("Read %d bytes from file with handle %d\n", j, i);
  else {
    Printf("FAILED to read!\n");
    Exit();
  }

  Printf("Comparing read data with written data...\n");
  if(dstrncmp(mem, read_mem, bytes) == 0) {
    Printf("Matches!\n");
  } else {
    Printf("FAILED: Doesn't match!\n");
    Exit();
  }

  j = file_close(i);
  if(j != -1 ) Printf("Closed file!\n");
  else {
    Printf("FAILED to close file\n");
    Exit();
  }
}