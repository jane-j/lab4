#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "dfs.h"
#include "files.h"
#include "synch.h"

// You have already been told about the most likely places where you should use locks. You may use 
// additional locks if it is really necessary.
static file_descriptor files[DFS_INODE_MAX_NUM]; // all files
lock_t file_lock;

// STUDENT: put your file-level functions here

void FileModuleInit() {
    int idx;
    file_lock = LockCreate();
    
    // Initialize all file descriptors to invalid state
    for (idx = 0; idx < DFS_INODE_MAX_NUM; idx++) {
        files[idx].valid = 0;
        files[idx].inode_handle = -1;
        files[idx].current_offset = 0;
        files[idx].end_of_file = 0;
        files[idx].mode = 0;
        files[idx].pid = -1;
        bzero(files[idx].filename, FILE_MAX_FILENAME_LENGTH);
    }
}

int FileNameExists(char *filename) {
    int idx;
    
    // Search through all file descriptors for matching filename
    for(idx = 0; idx < DFS_INODE_MAX_NUM; idx++) {
        if(files[idx].valid && dstrncmp(files[idx].filename, filename, FILE_MAX_FILENAME_LENGTH) == 0) {
            return idx;
        }
    }
    
    return FILE_FAIL;
}

int FileOpen(char *filename, char *mode) {
    int idx;
    int inode_h = DfsInodeFilenameExists(filename);
    int file_h = FileNameExists(filename);
    int fname_len = dstrlen(filename);

    // Check if file is already open
    if(file_h != FILE_FAIL) {
        return FILE_FAIL;
    }

    // Handle read mode
    if(dstrncmp(mode, "r", 1) == 0) {
        // File must exist for reading
        if(inode_h == DFS_FAIL) {
            return FILE_FAIL;
        }

        // Lock and find free descriptor
        if(LockHandleAcquire(file_lock) != SYNC_SUCCESS) {
            return FILE_FAIL;
        }

        for(idx = 0; idx < DFS_INODE_MAX_NUM; idx++) {
            if(!files[idx].valid) {
                files[idx].valid = 1;
                files[idx].inode_handle = inode_h;
                files[idx].current_offset = 0;
                files[idx].end_of_file = 0;
                files[idx].mode = 'r';
                files[idx].pid = GetCurrentPid();
                dstrncpy(files[idx].filename, filename, fname_len);
                
                LockHandleRelease(file_lock);
                return idx;
            }
        }

        LockHandleRelease(file_lock);
        return FILE_FAIL;
    }
    // Handle write mode
    else if(dstrncmp(mode, "w", 1) == 0) {
        // Delete existing file if present
        if(inode_h != DFS_FAIL) {
            if(DfsInodeDelete(inode_h) == DFS_FAIL) {
                return FILE_FAIL;
            }
        }

        // Create new inode
        inode_h = DfsInodeOpen(filename);
        if(inode_h == DFS_FAIL) {
            return FILE_FAIL;
        }

        // Lock and find free descriptor
        if(LockHandleAcquire(file_lock) != SYNC_SUCCESS) {
            return FILE_FAIL;
        }

        for(idx = 0; idx < DFS_INODE_MAX_NUM; idx++) {
            if(!files[idx].valid) {
                files[idx].valid = 1;
                files[idx].inode_handle = inode_h;
                files[idx].current_offset = 0;
                files[idx].end_of_file = 1;
                files[idx].mode = 'w';
                files[idx].pid = GetCurrentPid();
                dstrncpy(files[idx].filename, filename, fname_len);
                
                LockHandleRelease(file_lock);
                return idx;
            }
        }

        LockHandleRelease(file_lock);
        return FILE_FAIL;
    }
    // Handle append mode
    else if(dstrncmp(mode, "a", 1) == 0) {
        // Create file if it doesn't exist
        if(inode_h == DFS_FAIL) {
            inode_h = DfsInodeOpen(filename);
            if(inode_h == DFS_FAIL) {
                return FILE_FAIL;
            }
        }

        // Lock and find free descriptor
        if(LockHandleAcquire(file_lock) != SYNC_SUCCESS) {
            return FILE_FAIL;
        }

        for(idx = 0; idx < DFS_INODE_MAX_NUM; idx++) {
            if(!files[idx].valid) {
                int file_sz = DfsInodeFilesize(inode_h);
                if(file_sz == DFS_FAIL) {
                    LockHandleRelease(file_lock);
                    return FILE_FAIL;
                }
                
                files[idx].valid = 1;
                files[idx].inode_handle = inode_h;
                files[idx].current_offset = file_sz;
                files[idx].end_of_file = 1;
                files[idx].mode = 'a';
                files[idx].pid = GetCurrentPid();
                dstrncpy(files[idx].filename, filename, fname_len);
                
                LockHandleRelease(file_lock);
                return idx;
            }
        }

        LockHandleRelease(file_lock);
        return FILE_FAIL;
    }

    return FILE_FAIL;
}

int FileClose(int handle) {
    int curr_pid = GetCurrentPid();

    // Validate handle
    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) {
        return FILE_FAIL;
    }

    if(!files[handle].valid) {
        return FILE_FAIL;
    }

    // Check ownership
    if(curr_pid != files[handle].pid) {
        return FILE_FAIL;
    }

    // Reset file descriptor
    files[handle].valid = 0;
    files[handle].inode_handle = -1;
    files[handle].current_offset = 0;
    files[handle].end_of_file = 0;
    files[handle].mode = 0;
    files[handle].pid = -1;
    bzero(files[handle].filename, FILE_MAX_FILENAME_LENGTH);
    
    return FILE_SUCCESS;
}

int FileRead(int handle, void *mem, int num_bytes) {
    int curr_pid = GetCurrentPid();
    int file_sz;
    int bytes_to_read;
    int result;

    // Validate handle
    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) {
        return FILE_FAIL;
    }

    // Validate byte count
    if(num_bytes < 0 || num_bytes > FILE_MAX_READWRITE_BYTES) {
        return FILE_FAIL;
    }

    if(!files[handle].valid) {
        return FILE_FAIL;
    }

    // Check ownership
    if(curr_pid != files[handle].pid) {
        return FILE_FAIL;
    }

    // Check mode - can only read in 'r' or 'a' mode
    if(files[handle].mode != 'r' && files[handle].mode != 'a') {
        return FILE_FAIL;
    }

    // Check if already at EOF
    if(files[handle].end_of_file) {
        return FILE_FAIL;
    }

    // Get file size and adjust bytes to read
    file_sz = DfsInodeFilesize(files[handle].inode_handle);
    if(file_sz == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Calculate actual bytes to read
    bytes_to_read = num_bytes;
    if(files[handle].current_offset + num_bytes >= file_sz) {
        bytes_to_read = file_sz - files[handle].current_offset;
        files[handle].end_of_file = 1;
    }

    // Perform read
    result = DfsInodeReadBytes(files[handle].inode_handle, mem, files[handle].current_offset, bytes_to_read);
    if(result == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Update offset
    files[handle].current_offset += bytes_to_read;
    return bytes_to_read;
}

int FileWrite(int handle, void *mem, int num_bytes) {
    int curr_pid = GetCurrentPid();
    int file_sz;
    int result;

    // Validate handle
    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) {
        return FILE_FAIL;
    }

    // Validate byte count
    if(num_bytes < 0 || num_bytes > FILE_MAX_READWRITE_BYTES) {
        return FILE_FAIL;
    }

    if(!files[handle].valid) {
        return FILE_FAIL;
    }

    // Check ownership
    if(curr_pid != files[handle].pid) {
        return FILE_FAIL;
    }

    // Check mode - cannot write in 'r' mode
    if(files[handle].mode == 'r') {
        return FILE_FAIL;
    }

    // Perform write
    result = DfsInodeWriteBytes(files[handle].inode_handle, mem, files[handle].current_offset, num_bytes);
    if(result == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Update offset and EOF status
    files[handle].current_offset += num_bytes;
    
    file_sz = DfsInodeFilesize(files[handle].inode_handle);
    if(file_sz == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Update EOF flag
    files[handle].end_of_file = (files[handle].current_offset >= file_sz) ? 1 : 0;
    
    return num_bytes;
}

int FileSeek(int handle, int num_bytes, int from_where) {
    int file_sz;
    int curr_pid = GetCurrentPid();
    int new_offset;

    // Validate handle
    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) {
        return FILE_FAIL;
    }

    if(!files[handle].valid) {
        return FILE_FAIL;
    }

    // Check ownership
    if(curr_pid != files[handle].pid) {
        return FILE_FAIL;
    }

    // Get file size
    file_sz = DfsInodeFilesize(files[handle].inode_handle);
    if(file_sz == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Calculate new offset based on from_where
    if(from_where == FILE_SEEK_SET) {
        new_offset = num_bytes;
    }
    else if(from_where == FILE_SEEK_CUR) {
        new_offset = files[handle].current_offset + num_bytes;
    }
    else if(from_where == FILE_SEEK_END) {
        new_offset = file_sz + num_bytes;
    }
    else {
        return FILE_FAIL;
    }

    // Validate new offset
    if(new_offset < 0 || new_offset > file_sz) {
        return FILE_FAIL;
    }

    // Update offset and clear EOF
    files[handle].current_offset = new_offset;
    files[handle].end_of_file = 0;
    
    return FILE_SUCCESS;
}

int FileDelete(char *filename) {
    int inode_h = DfsInodeFilenameExists(filename);
    int file_h;

    // Check if file exists
    if(inode_h == DFS_FAIL) {
        return FILE_FAIL;
    }
   
    // Close if currently open
    file_h = FileNameExists(filename);
    if(file_h != FILE_FAIL) {
        if(FileClose(file_h) == FILE_FAIL) {
            return FILE_FAIL;
        }
    }
    
    // Delete inode
    if(DfsInodeDelete(inode_h) == DFS_FAIL) {
        return FILE_FAIL;
    }
    
    return FILE_SUCCESS;
}

int FileRename(char *oldname, char *newname) {
    int old_file_h;
    int newname_len = dstrlen(newname);

    // Check that new name doesn't already exist
    if(FileNameExists(newname) != FILE_FAIL) {
        return FILE_FAIL;
    }

    // Rename at inode level
    if(DfsInodeRename(oldname, newname) == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Update file descriptor if file is open
    old_file_h = FileNameExists(oldname);
    if(old_file_h == FILE_FAIL) {
        return FILE_FAIL;
    }

    // Update filename in descriptor
    bzero(files[old_file_h].filename, FILE_MAX_FILENAME_LENGTH);
    dstrncpy(files[old_file_h].filename, newname, newname_len);
    
    return FILE_SUCCESS;
}
