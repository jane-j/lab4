#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "dfs.h"
#include "files.h"
#include "synch.h"

// You have already been told about the most likely places where you should use locks. You may use 
// additional locks if it is really necessary.
static file_descriptor files[DFS_INODE_MAX_NUM]; // all files
lock_t files_lock;

// STUDENT: put your file-level functions here

void FileModuleInit() {
    int i;
    files_lock = LockCreate();
    
    // Initialize all file descriptors to invalid state
    for (i = 0; i < DFS_INODE_MAX_NUM; i++) {
        files[i].inuse = 0;
        files[i].inode_handle = -1;
        files[i].current_pos = 0;
        files[i].eof = 0;
        files[i].mode = 0;
        files[i].pid = -1;
        bzero(files[i].filename, FILE_MAX_FILENAME_LENGTH);
    }
}

int FileNameExists(char *filename) {
    int i;
    for(i = 0; i < DFS_INODE_MAX_NUM; i++) {
        if(files[i].inuse && (dstrncmp(files[i].filename, filename, FILE_MAX_FILENAME_LENGTH) == 0)) {
            return i;
        }
    }
    
    return FILE_FAIL;
}

int FileOpen(char *filename, char *mode) {
    int i;
    int inode_h = DfsInodeFilenameExists(filename);
    int file_h = FileNameExists(filename);
    int len = dstrlen(filename);

    // Check if already open
    if(file_h != FILE_FAIL) {
        return FILE_FAIL;
    }

    //read mode
    if(dstrncmp(mode, "r", 1) == 0) {
        if(inode_h == DFS_FAIL) {
            return FILE_FAIL;
        }

        // find free descriptor
        if(LockHandleAcquire(files_lock) != SYNC_SUCCESS) {
            return FILE_FAIL;
        }

        for(i = 0; i < DFS_INODE_MAX_NUM; i++) {
            if(!files[i].inuse) {
                files[i].inuse = 1;
                files[i].inode_handle = inode_h;
                files[i].current_pos = 0;
                files[i].eof = 0;
                files[i].mode = 'r';
                files[i].pid = GetCurrentPid();
                dstrncpy(files[i].filename, filename, len);
                if(LockHandleRelease(files_lock) != SYNC_SUCCESS)
                {
                    files[i].inuse = 0;
                    files[i].inode_handle = -1;
                    files[i].current_pos = 0;
                    files[i].eof = 0;
                    files[i].mode = 0;
                    files[i].pid = -1;
                    bzero(files[i].filename, FILE_MAX_FILENAME_LENGTH);
                    return FILE_FAIL;
                }
                return i;
            }
        }
        LockHandleRelease(files_lock);
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
        printf("FileOpen: Creating a new inode in write mode\n");
        inode_h = DfsInodeOpen(filename);
        if(inode_h == DFS_FAIL) {
            printf("FileOpen: Failed to open inode\n");
            return FILE_FAIL;
        }
        // Find free descriptor
        if(LockHandleAcquire(files_lock) != SYNC_SUCCESS) {
            printf("FileOpen: Failed to acquire lock\n");
            return FILE_FAIL;
        }
        for(i = 0; i < DFS_INODE_MAX_NUM; i++) {
            if(!files[i].inuse) {
                files[i].inuse = 1;
                files[i].inode_handle = inode_h;
                files[i].current_pos = 0;
                files[i].eof = 1;
                files[i].mode = 'w';
                files[i].pid = GetCurrentPid();
                dstrncpy(files[i].filename, filename, len);
                if(LockHandleRelease(files_lock) != SYNC_SUCCESS)
                {
                    files[i].inuse = 0;
                    files[i].inode_handle = -1;
                    files[i].current_pos = 0;
                    files[i].eof = 0;
                    files[i].mode = 0;
                    files[i].pid = -1;
                    bzero(files[i].filename, FILE_MAX_FILENAME_LENGTH);
                    printf("FileOpen: Failed to release lock\n");
                    return FILE_FAIL;
                }
                return i;
            }
        }

        LockHandleRelease(files_lock);
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
        if(LockHandleAcquire(files_lock) != SYNC_SUCCESS) {
            return FILE_FAIL;
        }

        for(i = 0; i < DFS_INODE_MAX_NUM; i++) {
            if(!files[i].inuse) {
                int file_sz = DfsInodeFilesize(inode_h);
                if(file_sz == DFS_FAIL) {
                    LockHandleRelease(files_lock);
                    return FILE_FAIL;
                }
                
                files[i].inuse = 1;
                files[i].inode_handle = inode_h;
                files[i].current_pos = file_sz;
                files[i].eof = 1;
                files[i].mode = 'a';
                files[i].pid = GetCurrentPid();
                dstrncpy(files[i].filename, filename, len);
                
                if(LockHandleRelease(files_lock) != SYNC_SUCCESS)
                {
                    files[i].inuse = 0;
                    files[i].inode_handle = -1;
                    files[i].current_pos = 0;
                    files[i].eof = 0;
                    files[i].mode = 0;
                    files[i].pid = -1;
                    bzero(files[i].filename, FILE_MAX_FILENAME_LENGTH);
                    return FILE_FAIL;
                }
                return i;
            }
        }

        LockHandleRelease(files_lock);
        return FILE_FAIL;
    }

    return FILE_FAIL;
}

int FileClose(int handle) {
    int pid = GetCurrentPid();

    // Validate handle
    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) {
        return FILE_FAIL;
    }

    if(!files[handle].inuse) {
        return FILE_FAIL;
    }

    // Check ownership
    if(pid != files[handle].pid) {
        return FILE_FAIL;
    }

    // Reset file descriptor
    files[handle].inuse = 0;
    files[handle].inode_handle = -1;
    files[handle].current_pos = 0;
    files[handle].eof = 0;
    files[handle].mode = 0;
    files[handle].pid = -1;
    bzero(files[handle].filename, FILE_MAX_FILENAME_LENGTH);
    
    return FILE_SUCCESS;
}

int FileRead(int handle, void *mem, int num_bytes) {
    int pid = GetCurrentPid();
    int filesize;
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

    //Check if its in use
    if(!files[handle].inuse) {
        return FILE_FAIL;
    }

    // Check ownership
    if(pid != files[handle].pid) {
        return FILE_FAIL;
    }

    // Check mode
    if(files[handle].mode != 'r' && files[handle].mode != 'a') {
        return FILE_FAIL;
    }

    // Check if already at EOF
    if(files[handle].eof) {
        return FILE_FAIL;
    }

    // Get file size and adjust bytes to read
    filesize = DfsInodeFilesize(files[handle].inode_handle);
    if(filesize == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Calculate actual bytes to read
    bytes_to_read = num_bytes;
    if(files[handle].current_pos + num_bytes >= filesize) {
        bytes_to_read = filesize - files[handle].current_pos;
        files[handle].eof = 1;
    }

    // Read
    result = DfsInodeReadBytes(files[handle].inode_handle, mem, files[handle].current_pos, bytes_to_read);
    if(result == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Update offset
    files[handle].current_pos += bytes_to_read;
    return bytes_to_read;
}

int FileWrite(int handle, void *mem, int num_bytes) {
    int pid = GetCurrentPid();
    int filesize;
    int result;

    // Validate handle
    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) {
        return FILE_FAIL;
    }

    // Validate byte count
    if(num_bytes < 0 || num_bytes > FILE_MAX_READWRITE_BYTES) {
        return FILE_FAIL;
    }

    //Check if its in use
    if(!files[handle].inuse) {
        return FILE_FAIL;
    }

    // Check ownership
    if(pid != files[handle].pid) {
        return FILE_FAIL;
    }

    // Check mode - cannot write in 'r' mode
    if(files[handle].mode == 'r') {
        return FILE_FAIL;
    }

    //Write
    result = DfsInodeWriteBytes(files[handle].inode_handle, mem, files[handle].current_pos, num_bytes);
    if(result == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Update offset and EOF status
    files[handle].current_pos += num_bytes;
    
    filesize = DfsInodeFilesize(files[handle].inode_handle);
    if(filesize == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Update EOF flag
    files[handle].eof = (files[handle].current_pos >= filesize) ? 1 : 0;
    
    return num_bytes;
}

int FileSeek(int handle, int num_bytes, int from_where) {
    int filesize;
    int pid = GetCurrentPid();
    int new_offset;

    // Validate handle
    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) {
        return FILE_FAIL;
    }

    // Check if its in use
    if(!files[handle].inuse) {
        return FILE_FAIL;
    }

    // Check ownership
    if(pid != files[handle].pid) {
        return FILE_FAIL;
    }

    // Get file size
    filesize = DfsInodeFilesize(files[handle].inode_handle);
    if(filesize == DFS_FAIL) {
        return FILE_FAIL;
    }

    // Calculate new offset based on 'from_where'
    if(from_where == FILE_SEEK_SET) {
        new_offset = num_bytes;
    }
    else if(from_where == FILE_SEEK_CUR) {
        new_offset = files[handle].current_pos + num_bytes;
    }
    else if(from_where == FILE_SEEK_END) {
        new_offset = filesize + num_bytes;
    }
    else {
        return FILE_FAIL;
    }

    // Validate new offset
    if(new_offset < 0 || new_offset > filesize) {
        return FILE_FAIL;
    }

    // Update offset and clear EOF
    files[handle].current_pos = new_offset;
    files[handle].eof = 0;
    
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
    int file_h;
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
    file_h = FileNameExists(oldname);
    if(file_h == FILE_FAIL) {
        return FILE_FAIL;
    }

    if(newname_len > FILE_MAX_FILENAME_LENGTH)
    {
        newname_len = FILE_MAX_FILENAME_LENGTH;
    }

    // Update filename in descriptor
    bzero(files[file_h].filename, FILE_MAX_FILENAME_LENGTH);
    dstrncpy(files[file_h].filename, newname, newname_len);
    
    return FILE_SUCCESS;
}
