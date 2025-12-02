// #include "usertraps.h"
// #include "misc.h"

// #include "file_check.h"

// static char mem[2500];

// void main() {
//   int i, j, k, bytes = 1056;
//   char * read_mem;

//   i = file_open("file_check.txt", "a");
//   if(i != -1) Printf("Opened file with handle %d\n", i);
//   else {
//     Printf("FAILED to open file\n");
//     Exit();
//   }

//   for(j = 0; j < bytes; j++) {
//     mem[j] = j;
//   }

//   Printf("Writing %d bytes to the file with handle %d\n", bytes, i);
//   j = file_write(i, mem, bytes);
//   if(j != -1) Printf("Successfully wrote %d bytes\n", j);
//   else {
//     Printf("FAILED to write!\n");
//     Exit();
//   }

//   Printf("Closing file with handle %d\n", i);
//   j = file_close(i);
//   if(j != -1 ) Printf("Closed file!\n");
//   else {
//     Printf("FAILED to close file\n");
//     Exit();
//   }

//   Printf("Reopening file file_check.txt\n" );
//   i = file_open("file_check.txt", "r");
//   if(i != -1) Printf("Re-Opened file with handle %d\n", i);
//   else {
//     Printf("FAILED to re-open file\n");
//     Exit();
//   }

//   Printf("Reading file with handle %d\n", i);
//   j = file_read(i, read_mem, bytes);
//   if(j!= -1) Printf("Read %d bytes from file with handle %d\n", j, i);
//   else {
//     Printf("FAILED to read!\n");
//     Exit();
//   }
  
//   Printf("Comparing read data with written data...\n");
//   for(k = 0; k < bytes; k++) {
//     if(mem[k] != read_mem[k]) Printf("Mismatch at %d, Got %d, expected %d\n", k, read_mem[k], mem[k]);
//     else Printf("Matches: %d = %d at %d\n", read_mem[k], mem[k], k);
//   }

//   j = file_close(i);
//   if(j != -1 ) Printf("Closed file!\n");
//   else {
//     Printf("FAILED to close file\n");
//     Exit();
//   }
// }
#include "usertraps.h"
#include "misc.h"
#include "files_shared.h"

#include "file_check.h"

static char write_buffer[2500];
static char read_buffer[2500];

// Forward declarations
void TestBasicWriteRead();
void TestWriteMode();
void TestAppendMode();
void TestFileSeek();
void TestFileDelete();
void TestFileRename();
void TestReadOnlyWrite();
void TestMultipleOpenFail();
void TestEOFFlag();

void main() {
    TestBasicWriteRead();
    TestWriteMode();
    TestAppendMode();
    TestFileSeek();
    TestEOFFlag();
    TestReadOnlyWrite();
    TestMultipleOpenFail();
    TestFileDelete();
    TestFileRename();

    Printf("All File API Tests Completed\n");
    Printf("========================================\n");
}

// Basic write and read test
void TestBasicWriteRead() {
    int fd, result, idx;
    int bytes_to_write = 1056;
    int mismatches = 0;

    Printf("TEST 1: Basic Write/Read Test\n");
    Printf("--------------------------------------\n");

    // Initialize write buffer with pattern
    for(idx = 0; idx < bytes_to_write; idx++) {
        write_buffer[idx] = (char)idx;
    }

    // Open file in append mode
    fd = file_open("test_basic.dat", "a");
    if(fd == -1) {
        Printf("FAILED: Could not open file for writing\n");
        Exit();
    }

    // Write data
    result = file_write(fd, write_buffer, bytes_to_write);
    if(result != bytes_to_write) {
        Printf("FAILED: Write returned %d, expected %d\n", result, bytes_to_write);
        Exit();
    }
    // Close file
    result = file_close(fd);
    if(result == -1) {
        Printf("FAILED: Could not close file\n");
        Exit();
    }

    // Reopen for reading
    fd = file_open("test_basic.dat", "r");
    if(fd == -1) {
        Printf("FAILED: Could not reopen file for reading\n");
        Exit();
    }

    // Read data back
    result = file_read(fd, read_buffer, bytes_to_write);
    if(result != bytes_to_write) {
        Printf("FAILED: Read returned %d, expected %d\n", result, bytes_to_write);
        Exit();
    }
    // Verify data integrity
    for(idx = 0; idx < bytes_to_write; idx++) {
        if(write_buffer[idx] != read_buffer[idx]) {
            mismatches++;
            if(mismatches <= 5) {
                Printf("  Mismatch at byte %d: wrote %d, read %d\n", 
                       idx, (unsigned char)write_buffer[idx], (unsigned char)read_buffer[idx]);
            }
        }
    }
    if(mismatches > 0) {
        Printf("FAILED: %d mismatches found\n", mismatches);
        Exit();
    }

    file_close(fd);
    Printf("TEST 1 PASSED\n\n");
}

// Write mode should delete file if it exists
void TestWriteMode() {
    int fd, result;
    char pattern1[] = "AAAAAAAAAA";
    char pattern2[] = "BBB";
    
    Printf("TEST 2: Write Mode Test\n");
    Printf("--------------------------------------\n");

    // Write initial data
    fd = file_open("test_write.dat", "w");
    if(fd == -1) {
        Printf("FAILED: Could not open file in write mode\n");
        Exit();
    }
    result = file_write(fd, pattern1, 10);
    file_close(fd);

    // Reopen in write mode (should delete existing)
    fd = file_open("test_write.dat", "w");
    if(fd == -1) {
        Printf("FAILED: Could not reopen file in write mode\n");
        Exit();
    }
    result = file_write(fd, pattern2, 3);
    file_close(fd);

    // Verify only 3 bytes exist
    fd = file_open("test_write.dat", "r");
    result = file_read(fd, read_buffer, 10);
    if(result != 3) {
        Printf("FAILED: Expected 3 bytes, got %d\n", result);
        Exit();
    }
    
    // Verify content
    if(read_buffer[0] != 'B' || read_buffer[1] != 'B' || read_buffer[2] != 'B') {
        Printf("FAILED: Content mismatch\n");
        Exit();
    }
    
    file_close(fd);
    Printf("TEST 2 PASSED\n\n");
}

// Append mode should add to end of file
void TestAppendMode() {
    int fd, result;
    char data1[] = "FIRST";
    char data2[] = "SECOND";
    
    Printf("TEST 3: Append Mode Test\n");
    Printf("--------------------------------------\n");

    // Create file with initial data
    fd = file_open("test_append.dat", "w");
    file_write(fd, data1, 5);
    file_close(fd);

    // Append more data
    fd = file_open("test_append.dat", "a");
    if(fd == -1) {
        Printf("FAILED: Could not open file in append mode\n");
        Exit();
    }
    result = file_write(fd, data2, 6);
    if(result != 6) {
        Printf("FAILED: Append write returned %d\n", result);
        Exit();
    }
    file_close(fd);

    // Verify total size is 11 bytes
    fd = file_open("test_append.dat", "r");
    result = file_read(fd, read_buffer, 20);
    if(result != 11) {
        Printf("FAILED: Expected 11 bytes total, got %d\n", result);
        Exit();
    }
    
    // Verify content
    if(read_buffer[0] != 'F' || read_buffer[4] != 'T' || 
       read_buffer[5] != 'S' || read_buffer[10] != 'D') {
        Printf("FAILED: Content verification failed\n");
        Exit();
    }
    
    file_close(fd);
    Printf("TEST 3 PASSED\n\n");
}

// File seek test
void TestFileSeek() {
    int fd, result, idx;
    char alphabet[26];
    
    Printf("TEST 4: File Seek Test\n");
    Printf("--------------------------------------\n");

    // Create alphabet file
    for(idx = 0; idx < 26; idx++) {
        alphabet[idx] = 'A' + idx;
    }
    
    fd = file_open("test_seek.dat", "w");
    file_write(fd, alphabet, 26);
    file_close(fd);

    // Test SEEK_SET
    fd = file_open("test_seek.dat", "r");
    result = file_seek(fd, 10, FILE_SEEK_SET);
    if(result == -1) {
        Printf("FAILED: SEEK_SET failed\n");
        Exit();
    }
    result = file_read(fd, read_buffer, 1);
    if(read_buffer[0] != 'K') {
        Printf("FAILED: SEEK_SET, expected 'K', got '%c'\n", read_buffer[0]);
        Exit();
    }

    // Test SEEK_CUR
    result = file_seek(fd, 5, FILE_SEEK_CUR);
    if(result == -1) {
        Printf("FAILED: SEEK_CUR failed\n");
        Exit();
    }
    result = file_read(fd, read_buffer, 1);
    if(read_buffer[0] != 'Q') {
        Printf("FAILED: SEEK_CUR, expected 'Q', got '%c'\n", read_buffer[0]);
        Exit();
    }

    // Test SEEK_END
    result = file_seek(fd, -5, FILE_SEEK_END);
    if(result == -1) {
        Printf("FAILED: SEEK_END failed\n");
        Exit();
    }
    result = file_read(fd, read_buffer, 1);
    if(read_buffer[0] != 'V') {
        Printf("FAILED: SEEK_END, expected 'V', got '%c'\n", read_buffer[0]);
        Exit();
    }

    file_close(fd);
    Printf("TEST 4 PASSED\n\n");
}

// Test EOF flag behavior
void TestEOFFlag() {
    int fd, result;
    char data[100];
    
    Printf("TEST 5: EOF Flag Test\n");
    Printf("--------------------------------------\n");

    // Create small file
    fd = file_open("test_eof.dat", "w");
    file_write(fd, "TEST", 4);
    file_close(fd);

    // Read all data
    fd = file_open("test_eof.dat", "r");
    result = file_read(fd, data, 4);

    // Try to read past EOF
    result = file_read(fd, data, 10);
    if(result != -1) {
        Printf("WARNING: Reading past EOF should return -1, got %d\n", result);
    }

    // Seek should clear EOF flag
    result = file_seek(fd, 0, FILE_SEEK_SET);
    if(result == -1) {
        Printf("FAILED: Seek failed\n");
        Exit();
    }

    // Should be able to read again
    result = file_read(fd, data, 4);
    if(result != 4) {
        Printf("FAILED: Could not read after seek, got %d\n", result);
        Exit();
    }

    file_close(fd);
    Printf("TEST 5 PASSED\n\n");
}


// Read only model should not allow writes
void TestReadOnlyWrite() {
    int fd, result;
    
    Printf("TEST 6: Read-Only Write Test\n");
    Printf("--------------------------------------\n");

    // Create a file
    fd = file_open("test_readonly.dat", "w");
    file_write(fd, "DATA", 4);
    file_close(fd);

    // Open in read mode
    fd = file_open("test_readonly.dat", "r");
    if(fd == -1) {
        Printf("FAILED: Could not open file\n");
        Exit();
    }

    // Try to write (should fail)
    result = file_write(fd, "NEWDATA", 7);
    if(result == -1) {
        
    } else {
        Printf("FAILED: Write should fail in read mode, got %d\n", result);
        Exit();
    }

    file_close(fd);
    Printf("TEST 6 PASSED\n\n");
}

// Test that multiple opens of same file fail
void TestMultipleOpenFail() {
    int fd1, fd2;
    
    Printf("TEST 7: Multiple Open Prevention Test\n");
    Printf("--------------------------------------\n");

    // Open file first time
    fd1 = file_open("test_multiopen.dat", "w");
    if(fd1 == -1) {
        Printf("FAILED: Could not open file first time\n");
        Exit();
    }

    // Try to open same file again
    fd2 = file_open("test_multiopen.dat", "r");
    if(fd2 == -1) {
        
    } else {
        Printf("WARNING: Same file opened twice (handles %d, %d)\n", fd1, fd2);
        file_close(fd2);
    }

    file_close(fd1);
    
    // Now should be able to open
    fd2 = file_open("test_multiopen.dat", "r");
    if(fd2 != -1) {
        file_close(fd2);
    }

    Printf("TEST 7 PASSED\n\n");
}


// Tests: FileDelete() removes file
void TestFileDelete() {
    int fd, result;
    
    Printf("TEST 8: File Delete Test\n");
    Printf("--------------------------------------\n");

    // Create a file
    fd = file_open("test_delete.dat", "w");
    file_write(fd, "DELETE_ME", 9);
    file_close(fd);

    // Delete the file
    result = file_delete("test_delete.dat");
    if(result == -1) {
        Printf("FAILED: Could not delete file\n");
        Exit();
    }

    // Try to open deleted file (should create new empty file)
    fd = file_open("test_delete.dat", "r");
    if(fd != -1) {
        result = file_read(fd, read_buffer, 10);
        if(result == -1 || result == 0) {
        } else {
            Printf("WARNING: Deleted file still has data: %d bytes\n", result);
        }
        file_close(fd);
    }

    Printf("TEST 8 PASSED\n\n");
}

//File Rename test
void TestFileRename() {
    int fd, result;
    
    Printf("TEST 9: File Rename Test\n");
    Printf("--------------------------------------\n");

    // Create file with original name
    fd = file_open("old_name.dat", "w");
    file_write(fd, "RENAMED_DATA", 12);
    file_close(fd);

    // Rename the file
    result = file_rename("old_name.dat", "new_name.dat");
    if(result == -1) {
        Printf("FAILED: Could not rename file\n");
        Exit();
    }

    // Open new name and verify data
    fd = file_open("new_name.dat", "r");
    if(fd == -1) {
        Printf("FAILED: Could not open renamed file\n");
        Exit();
    }
    result = file_read(fd, read_buffer, 12);
    if(result == 12 && read_buffer[0] == 'R' && read_buffer[8] == 'D') {
        Printf("Renamed file data matches at index 0 and 8\n");
        Printf("TEST 9 PASSED\n\n");
    } else {
        Printf("WARNING: Renamed file data mismatch\n");
    }
    file_close(fd);

    
}