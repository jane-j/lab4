#include "usertraps.h"
#include "misc.h"

#include "file_check.h"

static char mem[2500];

void main() {
  int i, j, k, bytes = 1056;
  char * read_mem;

  i = file_open("file_check.txt", "a");
  if(i != -1) Printf("Opened file with handle %d\n", i);
  else {
    Printf("FAILED to open file\n");
    Exit();
  }

  for(j = 0; j < bytes; j++) {
    mem[j] = j;
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
  for(k = 0; k < bytes; k++) {
    if(mem[k] != read_mem[k]) Printf("Mismatch at %d, Got %d, expected %d\n", k, read_mem[k], mem[k]);
    else Printf("Matches: %d = %d at %d\n", read_mem[k], mem[k], k);
  }

  j = file_close(i);
  if(j != -1 ) Printf("Closed file!\n");
  else {
    Printf("FAILED to close file\n");
    Exit();
  }
}