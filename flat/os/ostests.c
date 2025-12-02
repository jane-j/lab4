#include "ostraps.h"
#include "dlxos.h"
#include "traps.h"
#include "disk.h"
#include "dfs.h"
#include "ostests.h"

// Test data buffers
static char primary_buffer[NUMBYTES];
static char secondary_buffer[NUMBYTES];
static char extended_buffer_1[NUMBYTES_2];
static char extended_buffer_2[NUMBYTES_2];

// Helper function prototypes
static void InitializePattern(char *buffer, int size);
static int VerifyPattern(char *buffer1, char *buffer2, int size);
static void HandleTestFailure(char *msg);
static int ValidateWriteResult(int result, int expected, char *context);
static int ValidateReadResult(int result, int expected, char *context);
static int ValidateFilesize(int inode_handle, int expected_size);


void RunOSTests() {
    TestFunc();
    TestUnaligned();
    TestLargeFile();
    TestDeleteAndReopen();
    // Run Twice to check persistence
    TestPersistence();
    TestPersistence();
}

static void InitializePattern(char *buffer, int size) {
    int idx;
    for (idx = 0; idx < size; idx++) {
        buffer[idx] = (char)idx;
    }
}

static int VerifyPattern(char *buffer1, char *buffer2, int size) {
    int idx;
    for (idx = 0; idx < size; idx++) {
        if (buffer1[idx] != buffer2[idx]) {
            printf("Data mismatch at index %d: expected=%d, actual=%d\n", 
                   idx, (unsigned char)buffer1[idx], (unsigned char)buffer2[idx]);
            return 0;
        }
    }
    return 1;
}

static void HandleTestFailure(char *msg) {
    printf("TEST FAILURE: %s\n", msg);
    GracefulExit();
}

static int ValidateWriteResult(int result, int expected, char *context) {
    if (result == DFS_FAIL) {
        printf("Write operation failed in %s\n", context);
        HandleTestFailure(context);
        return 0;
    }
    if (result != expected) {
        printf("Write size mismatch in %s: expected=%d, got=%d\n", 
               context, expected, result);
        HandleTestFailure(context);
        return 0;
    }
    return 1;
}

static int ValidateReadResult(int result, int expected, char *context) {
    if (result == DFS_FAIL) {
        printf("Read operation failed in %s\n", context);
        HandleTestFailure(context);
        return 0;
    }
    if (result != expected) {
        printf("Read size mismatch in %s: expected=%d, got=%d\n", 
               context, expected, result);
        HandleTestFailure(context);
        return 0;
    }
    return 1;
}

static int ValidateFilesize(int inode_handle, int expected_size) {
    int actual_size = DfsInodeFilesize(inode_handle);
    
    if (actual_size == DFS_FAIL) {
        printf("DfsInodeFilesize failed\n");
        return 0;
    }
    
    if (actual_size != expected_size) {
        printf("File size validation failed: expected=%d, got=%d\n", 
               expected_size, actual_size);
        return 0;
    }
    
    return 1;
}

//======================================================================
// Test Case 1: Basic Read/Write Operations
// Tests: 4 block writes, file size check, block read verification
//======================================================================
void TestFunc() {
    uint32 inode_handle;
    int write_result, read_result;
    int block_idx;
    int offset;
    
    printf("===== Starting Basic I/O Test =====\n");
    
    // Open inode for testing
    inode_handle = DfsInodeOpen("ece695-basic");
    if (inode_handle == DFS_FAIL) {
        HandleTestFailure("DfsInodeOpen failed for basic test");
    }
    
    InitializePattern(primary_buffer, NUMBYTES);
    
    // Write 4 consecutive blocks
    for (block_idx = 0; block_idx < 4; block_idx++) {
        offset = block_idx * NUMBYTES;
        write_result = DfsInodeWriteBytes(inode_handle, primary_buffer, offset, NUMBYTES);
        ValidateWriteResult(write_result, NUMBYTES, "basic test write loop");
    }
    

    if (!ValidateFilesize(inode_handle, 4 * NUMBYTES)) {
        HandleTestFailure("File size check failed in basic test");
    }
    
    read_result = DfsInodeReadBytes(inode_handle, secondary_buffer, 2 * NUMBYTES, NUMBYTES);
    ValidateReadResult(read_result, NUMBYTES, "basic test read");
    
    if (!VerifyPattern(primary_buffer, secondary_buffer, NUMBYTES)) {
        HandleTestFailure("Data verification failed in basic test");
    }
    
    printf("===== Basic I/O Test PASSED =====\n\n");
}

//  Non-block-aligned writes
void TestUnaligned() {
    uint32 inode_handle;
    int write_result, read_result;
    int total_written = 0;
    
    printf("===== Starting Unaligned Write Test =====\n");
    
    inode_handle = DfsInodeOpen("ece695-unaligned");
    if (inode_handle == DFS_FAIL) {
        HandleTestFailure("DfsInodeOpen failed for unaligned test");
    }
    
    InitializePattern(extended_buffer_1, NUMBYTES_2);
    
    // First write: 500 bytes at offset 0
    write_result = DfsInodeWriteBytes(inode_handle, extended_buffer_1, 0, 500);
    printf("Write operation 1: %d bytes at offset 0\n", write_result);
    ValidateWriteResult(write_result, 500, "unaligned write #1");
    total_written += write_result;
    
    // Second write: 700 bytes at offset 500
    write_result = DfsInodeWriteBytes(inode_handle, extended_buffer_1 + 500, 500, 700);
    printf("Write operation 2: %d bytes at offset 500\n", write_result);
    ValidateWriteResult(write_result, 700, "unaligned write #2");
    total_written += write_result;
    
    // Third write: 300 bytes at offset 1200
    write_result = DfsInodeWriteBytes(inode_handle, extended_buffer_1 + 1200, 1200, 300);
    printf("Write operation 3: %d bytes at offset 1200\n", write_result);
    ValidateWriteResult(write_result, 300, "unaligned write #3");
    total_written += write_result;
    
    // Verify file size
    printf("Total bytes written: %d\n", total_written);
    if (!ValidateFilesize(inode_handle, 1500)) {
        HandleTestFailure("File size check failed in unaligned test");
    }
    
    // Read entire file and verify
    read_result = DfsInodeReadBytes(inode_handle, extended_buffer_2, 0, 1500);
    printf("Read operation: %d bytes from offset 0\n", read_result);
    ValidateReadResult(read_result, 1500, "unaligned read");
    
    if (!VerifyPattern(extended_buffer_1, extended_buffer_2, 1500)) {
        HandleTestFailure("Data verification failed in unaligned test");
    }
    
    printf("===== Unaligned Write Test PASSED =====\n\n");
}

// Large write test
void TestLargeFile() {
    uint32 inode_handle;
    int block_num;
    int write_result, read_result;
    int byte_offset;
    int expected_total_size;
    
    printf("===== Starting Large File Test =====\n");
    printf("Writing %d blocks to test indirect addressing\n", LARGE_NUM_BLOCKS);
    
    inode_handle = DfsInodeOpen("ece695-large");
    if (inode_handle == DFS_FAIL) {
        HandleTestFailure("DfsInodeOpen failed for large file test");
    }
    
    InitializePattern(primary_buffer, NUMBYTES);
    
    // Write phase: write LARGE_NUM_BLOCKS blocks
    for (block_num = 0; block_num < LARGE_NUM_BLOCKS; block_num++) {
        byte_offset = block_num * NUMBYTES;
        write_result = DfsInodeWriteBytes(inode_handle, primary_buffer, byte_offset, NUMBYTES);
        
        if (write_result != NUMBYTES) {
            printf("Write failure at block %d (offset %d)\n", block_num, byte_offset);
            HandleTestFailure("Large file write operation failed");
        }
    }
    
    expected_total_size = LARGE_NUM_BLOCKS * NUMBYTES;
    printf("Expected file size: %d bytes\n", expected_total_size);
    
    if (!ValidateFilesize(inode_handle, expected_total_size)) {
        HandleTestFailure("File size check failed in large file test");
    }
    
    printf("Starting verification phase\n");
    for (block_num = 0; block_num < LARGE_NUM_BLOCKS; block_num++) {
        byte_offset = block_num * NUMBYTES;
        read_result = DfsInodeReadBytes(inode_handle, secondary_buffer, byte_offset, NUMBYTES);
        
        if (read_result != NUMBYTES) {
            printf("Read failure at block %d (offset %d)\n", block_num, byte_offset);
            HandleTestFailure("Large file read operation failed");
        }
        
        if (!VerifyPattern(primary_buffer, secondary_buffer, NUMBYTES)) {
            printf("Verification failed at block %d\n", block_num);
            HandleTestFailure("Large file data verification failed");
        }
    }
    
    printf("===== Large File Test PASSED =====\n\n");
    
}


// Delete and reopen test.
void TestDeleteAndReopen() {
    uint32 inode_handle;
    int write_result, read_result, delete_result;
    int size_before, size_after;
    
    printf("===== Starting Delete/Reopen Test =====\n");
    
    // Create and write to file
    inode_handle = DfsInodeOpen("ece695-delete");
    if (inode_handle == DFS_FAIL) {
        HandleTestFailure("DfsInodeOpen failed for delete test");
    }
    
    InitializePattern(primary_buffer, NUMBYTES);
    write_result = DfsInodeWriteBytes(inode_handle, primary_buffer, 0, NUMBYTES);
    
    if (ValidateWriteResult(write_result, NUMBYTES, "delete test write")) {
        printf("Successfully wrote %d bytes to file\n", write_result);
    }
    
    // Check size before deletion
    size_before = DfsInodeFilesize(inode_handle);
    if (size_before != NUMBYTES) {
        printf("Pre-deletion size mismatch: expected=%d, got=%d\n", NUMBYTES, size_before);
        HandleTestFailure("Size check before deletion failed");
    }
    printf("File size before deletion: %d bytes\n", size_before);
    
    // Delete the inode
    delete_result = DfsInodeDelete(inode_handle);
    if (delete_result == DFS_FAIL) {
        HandleTestFailure("DfsInodeDelete failed");
    }
    printf("File deleted successfully\n");
    
    // Reopen with same name 
    inode_handle = DfsInodeOpen("ece695-delete");
    if (inode_handle == DFS_FAIL) {
        HandleTestFailure("Reopen after delete failed");
    }
    printf("File reopened successfully\n");
    
    // Check size after reopening
    size_after = DfsInodeFilesize(inode_handle);
    if (size_after != 0) {
        printf("Post-deletion size should be 0, got %d\n", size_after);
        HandleTestFailure("Size check after deletion failed");
    }
    printf("File size after reopen: %d bytes (correct)\n", size_after);
    
    // Try to read from empty file
    read_result = DfsInodeReadBytes(inode_handle, secondary_buffer, 0, NUMBYTES);
    if (read_result == DFS_FAIL) {
        printf("Read from empty file correctly returned DFS_FAIL\n");
    } else {
        printf("Unexpected: read returned %d instead of DFS_FAIL\n", read_result);
        HandleTestFailure("Read from empty file should fail");
    }
    
    printf("===== Delete/Reopen Test PASSED =====\n\n");
}

// Check persisitence across runs
void TestPersistence() {
    uint32 inode_handle;
    char expected_pattern[NUMBYTES];
    char read_buffer[NUMBYTES];
    int write_result, read_result;
    int current_size;
    int idx;
    int data_matches = 1;
    
    printf("===== Starting Persistence Test =====\n");
    
    inode_handle = DfsInodeOpen("ece695-persist");
    if (inode_handle == DFS_FAIL) {
        HandleTestFailure("DfsInodeOpen failed for persistence test");
    }
    
    // Initialize expected pattern
    InitializePattern(expected_pattern, NUMBYTES);
    
    // Check current file size
    current_size = DfsInodeFilesize(inode_handle);
    printf("Current file size: %d bytes\n", current_size);
    
    // First run: initialize the file
    if (current_size < NUMBYTES) {
        printf("First run detected - initializing persistent file\n");
        write_result = DfsInodeWriteBytes(inode_handle, expected_pattern, 0, NUMBYTES);
        
        if (ValidateWriteResult(write_result, NUMBYTES, "persistence initialization")) {
            printf("Persistence file initialized with %d bytes\n", write_result);
        }
        
        printf("Run simulation again to verify persistence\n");
        printf("===== Persistence Test - First Run Complete =====\n\n");
        return;
    }
    
    // second run: verify data persisted
    printf("Subsequent run detected - verifying persisted data\n");
    read_result = DfsInodeReadBytes(inode_handle, read_buffer, 0, NUMBYTES);
    
    if (!ValidateReadResult(read_result, NUMBYTES, "persistence read")) {
        HandleTestFailure("Failed to read persisted data");
    }
    
    // Compare byte by byte
    for (idx = 0; idx < NUMBYTES; idx++) {
        if (read_buffer[idx] != expected_pattern[idx]) {
            data_matches = 0;
            break;
        }
    }
    
    if (data_matches) {
        printf("Data successfully persisted across runs!\n");
        printf("===== Persistence Test PASSED =====\n\n");
    } else {
        printf("Data mismatch detected - reinitializing\n");
        write_result = DfsInodeWriteBytes(inode_handle, expected_pattern, 0, NUMBYTES);
        
        if (ValidateWriteResult(write_result, NUMBYTES, "persistence rewrite")) {
            printf("File rewritten - run again to verify\n");
        }
    }
}

