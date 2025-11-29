#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "dfs.h"
#include "files.h"
#include "synch.h"

// You have already been told about the most likely places where you should use locks. You may use 
// additional locks if it is really necessary.

// STUDENT: put your file-level functions here
static file_descriptor files[DFS_INODE_MAX_NUM];
lock_t files_lock;

void FileModuleInit() {
    int i;
    files_lock = LockCreate();
    for(i = 0; i < DFS_INODE_MAX_NUM; i++) {
        files[i].inuse = 0;
        files[i].inode_handle = -1;
        files[i].mode = 0;
        files[i].eof = 0;
        files[i].currentPos = 0;
        files[i].pid = -1; 
        bzero(files[i].filename, FILE_MAX_FILENAME_LENGTH);
    }
}

int FileNameExists(char * filename) {
    int i;
    for(i = 0; i < DFS_INODE_MAX_NUM; i++) {
        if(files[i].inuse && (dstrncmp(files[i].filename, filename, FILE_MAX_FILENAME_LENGTH) == 0)) {
            return i;
        } 
    }
    return FILE_FAIL;
}

int FileOpen(char *filename, char *mode)
{
    int i, inode_handle;
    int isWrite = 0, isRead = 0, isAppend = 0;

    //Check file is already opened
    if(FileNameExists(filename) != FILE_FAIL) {
        printf("FileOpen: File already open!\n");
        return FILE_FAIL;
    }

    if(dstrncmp(mode, 'r', 1) == 0) isRead = 1;
    else if(dstrncmp(mode, 'w', 1) == 0) isWrite = 1;
    else if(dstrncmp(mode, 'a', 1) == 0) isAppend = 1;
    else {
        printf("FileOpen: Unrecognized mode '%c'\n", mode[0]);
        return FILE_FAIL;
    }

    //Check if file exists
    inode_handle = DfsInodeFilenameExists(filename);
    if(inode_handle == DFS_FAIL) {
        //File does not exist
        if(isRead) {
            printf("FileOpen: File doesn't exist!\n");
            return FILE_FAIL;
        } else {
            //create inode
            inode_handle = DfsInodeOpen(filename);
            if(inode_handle == DFS_FAIL) return FILE_FAIL;

            //open file
            if(LockHandleAcquire(files_lock) != SYNC_SUCCESS) {
                printf("FileOpen: Failed to acquire lock!\n");
                return FILE_FAIL;
            }
            for(i = 0; i < DFS_INODE_MAX_NUM; i++) {
                if(files[i].inuse == 0) {
                    files[i].inuse = 1;
                    dstrncpy(files[i].filename, filename, dstrlen(filename));
                    files[i].mode = mode[0];
                    files[i].pid = GetCurrentPid();
                    files[i].inode_handle = inode_handle;
                    files[i].currentPos = 0;
                    files[i].eof = 1;
                    if(LockHandleRelease(files_lock) != SYNC_SUCCESS) {
                        printf("FileOpen: Failed to release lock!\n");
                        return FILE_FAIL;
                    }
                    return i;
                }
            }
        }
    } else {
        //File exists
        if(isWrite) {
            if(DfsInodeDelete(inode_handle) == DFS_FAIL) {
                printf("FileOpen: Couldn't delete file!\n");
                return FILE_FAIL;
            }
            //create inode
            inode_handle = DfsInodeOpen(filename);
            if(inode_handle == DFS_FAIL) return FILE_FAIL;
        }
        //open file
        if(LockHandleAcquire(files_lock) != SYNC_SUCCESS) {
            printf("FileOpen: Failed to acquire lock!\n");
            return FILE_FAIL;
        }
        for(i = 0; i < DFS_INODE_MAX_NUM; i++) {
            if(files[i].inuse == 0) {
                files[i].inuse = 1;
                dstrncpy(files[i].filename, filename, dstrlen(filename));
                files[i].mode = 'r';
                files[i].pid = GetCurrentPid();
                files[i].inode_handle = inode_handle;
                if(isRead) {
                    files[i].currentPos = 0;
                    files[i].eof = 0;
                } else if(isWrite) {
                    files[i].currentPos = 0;
                    files[i].eof = 1;
                } else {
                    files[i].currentPos = DfsInodeFilesize(inode_handle);
                    if(files[i].currentPos == DFS_FAIL) return FILE_FAIL;
                    files[i].eof = 1;
                }
                if(LockHandleRelease(files_lock) != SYNC_SUCCESS) {
                    printf("FileOpen: Failed to release lock!\n");
                    return FILE_FAIL;
                }
                return i;
            }
        }
    }
    if(LockHandleRelease(files_lock) != SYNC_SUCCESS) {
        printf("FileOpen: Failed to release lock!\n");
        return FILE_FAIL;
    }
    return FILE_FAIL;
}

int FileClose(int handle)
{
    int pid = GetCurrentPid();

    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) return FILE_FAIL;

    if(files[handle].pid != pid) {
        printf("FileClose: Illegal process accessing file!\n");
        return FILE_FAIL;
    }
    if(files[handle].inuse == 0) return FILE_FAIL;

    files[handle].inuse = 0;
    files[handle].inode_handle = -1;
    files[handle].mode = 0;
    files[handle].eof = 0;
    files[handle].currentPos = 0;
    files[handle].pid = -1;
    bzero(files[handle].filename, FILE_MAX_FILENAME_LENGTH);

    return FILE_SUCCESS;
}

int FileRead(int handle, void *mem, int num_bytes)
{
    int pid = GetCurrentPid();
    int bytes_to_read = 0, filesize = 0, bytes_read = 0;

    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) return FILE_FAIL;
    if(num_bytes < 0 || num_bytes > FILE_MAX_READWRITE_BYTES) return FILE_FAIL;
    if(files[handle].inuse == 0) return FILE_FAIL;
    if(files[handle].pid != pid) {
        printf("FileClose: Illegal process accessing file!\n");
        return FILE_FAIL;
    }
    if(files[handle].mode == 'w') return FILE_FAIL;
    if(files[handle].eof) return FILE_FAIL;

    filesize = DfsInodeFilesize(files[handle].inode_handle);
    if(filesize == DFS_FAIL) return DFS_FAIL;

    bytes_to_read = num_bytes;
    if(files[handle].currentPos + num_bytes >= filesize) {
        bytes_to_read = filesize - files[handle].currentPos;
        files[handle].eof = 1;
    }

    bytes_read = DfsInodeReadBytes(files[handle].inode_handle, mem, files[handle].currentPos, bytes_to_read);
    if(bytes_read != bytes_to_read) return FILE_FAIL;

    files[handle].currentPos += bytes_read;
    return bytes_read;
}

int FileWrite(int handle, void *mem, int num_bytes)
{
    int pid = GetCurrentPid();
    int filesize = 0, bytes_written = 0;

    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) return FILE_FAIL;
    if(num_bytes < 0 || num_bytes > FILE_MAX_READWRITE_BYTES) return FILE_FAIL;
    if(files[handle].inuse == 0) return FILE_FAIL;
    if(files[handle].pid != pid) {
        printf("FileClose: Illegal process accessing file!\n");
        return FILE_FAIL;
    }
    if(files[handle].mode == 'r') return FILE_FAIL;
    
    bytes_written = DfsInodeWriteBytes(files[handle].inode_handle, mem, files[handle].currentPos, num_bytes);
    if(bytes_written != num_bytes) return FILE_FAIL;

    files[handle].currentPos += bytes_written;
    filesize = DfsInodeFilesize(files[handle].inode_handle);
    if(filesize == DFS_FAIL) return DFS_FAIL;

    if(files[handle].currentPos == filesize) files[handle].eof = 1;
    else if(files[handle].currentPos > filesize) {
        printf("FileWrite: File current position beyond filesize!\n");
        return FILE_FAIL;
    }

    return bytes_written;
}

int FileSeek(int handle, int num_bytes, int from_where)
{
    int pid = GetCurrentPid();
    int filesize, i;

    if(handle < 0 || handle >= DFS_INODE_MAX_NUM) return FILE_FAIL;
    if(files[handle].inuse == 0) return FILE_FAIL;
    if(files[handle].pid != pid) {
        printf("FileClose: Illegal process accessing file!\n");
        return FILE_FAIL;
    }
    filesize = DfsInodeFilesize(files[handle].inode_handle);
    if(filesize == DFS_FAIL) return DFS_FAIL;

    if(from_where == FILE_SEEK_SET) i = num_bytes;
    else if(from_where == FILE_SEEK_END) i = filesize + num_bytes;
    else if(from_where == FILE_SEEK_CUR) i = files[handle].currentPos + num_bytes;
    else {
        printf("FileSeek: Illegal from_where!\n");
        return FILE_FAIL;
    }

    if(i < 0 || i > filesize) {
        printf("FileSeek: Illegal seek!\n");
        return FILE_FAIL;
    }

    files[handle].currentPos = i;
    files[handle].eof = 0;
    return FILE_SUCCESS;
}

int FileDelete(char *filename)
{
    int i;
    int inode_handle;
    int pid = GetCurrentPid();

    i = FileNameExists(filename);
    if(i == FILE_FAIL) {
        printf("FileDelete: File doesn't exist!\n");
        return FILE_FAIL;
    }

    if(files[i].pid != pid) {
        printf("FileDelete: Illegal process accessing file!\n");
        return FILE_FAIL;
    }

    inode_handle = DfsInodeFilenameExists(filename);
    if(inode_handle == DFS_FAIL) {
       printf("FileDelete: File doesn't exist!\n");
       return FILE_FAIL; 
    }

    if(FileClose(i) == FILE_FAIL) return FILE_FAIL;

    if(DfsInodeDelete((uint32) inode_handle) == DFS_FAIL) return FILE_FAIL;

    return FILE_SUCCESS;
}

int FileRename(char *oldname, char *newname)
{   
    int i;
    int len = dstrlen(newname);
    int pid = GetCurrentPid();

    if(FileNameExists(newname) != FILE_FAIL) {
        printf("FileRename: Newname already exists!\n");
        return FILE_FAIL;
    }
    
    i = FileNameExists(oldname);
    if(i == FILE_FAIL) {
        printf("FileRename: Oldname doesn't exist!\n");
        return FILE_FAIL;
    }

    if(files[i].pid != pid) {
        printf("FileRename: Illegal process accessing file!\n");
        return FILE_FAIL;
    }

    bzero(files[i].filename, FILE_MAX_FILENAME_LENGTH);
    if(len > FILE_MAX_FILENAME_LENGTH) len = FILE_MAX_FILENAME_LENGTH;
    dstrncpy(files[i].filename, newname, len);

    if(DfsInodeFileRename(oldname, files[i].filename) == DFS_FAIL){
        return FILE_FAIL;
    }

    return FILE_SUCCESS;
}
