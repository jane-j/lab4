#include "ostraps.h"
#include "dlxos.h"
#include "traps.h"
#include "queue.h"
#include "disk.h"
#include "dfs.h"
#include "synch.h"

static dfs_inode inodes[DFS_INODE_MAX_NUM]; // all inodes
static dfs_superblock sb; // superblock
static uint32 fbv[DFS_FBV_MAX_NUM_WORDS]; // Free block vector

static uint32 negativeone = 0xFFFFFFFF;
static inline uint32 invert(uint32 n) { return n ^ negativeone; }

int diskblocksize;

// You have already been told about the most likely places where you should use locks. You may use 
// additional locks if it is really necessary.

lock_t inode_lock;
lock_t fbv_lock;
lock_t cache_lock;

// STUDENT: put your file system level functions below.
// Some skeletons are provided. You can implement additional functions.

///////////////////////////////////////////////////////////////////
// Non-inode functions first
///////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------
// DfsModuleInit is called at boot time to initialize things and
// open the file system for use.
//-----------------------------------------------------------------

void DfsModuleInit() {
// You essentially set the file system as invalid and then open 
// using DfsOpenFileSystem().
    DfsInvalidate();
    printf("moduleinit: sb valid %d\n",sb.valid);
    inode_lock = LockCreate();
    fbv_lock = LockCreate();
    cache_lock = LockCreate();
    diskblocksize = DiskBytesPerBlock();
    DfsOpenFileSystem();
    printf("moduleinit 2: sb valid %d\n",sb.valid);
}

//-----------------------------------------------------------------
// DfsInavlidate marks the current version of the filesystem in
// memory as invalid.  This is really only useful when formatting
// the disk, to prevent the current memory version from overwriting
// what you already have on the disk when the OS exits.
//-----------------------------------------------------------------

void DfsInvalidate() {
// This is just a one-line function which sets the valid bit of the 
// superblock to 0.
    sb.valid = 0;
}

//-------------------------------------------------------------------
// DfsOpenFileSystem loads the file system metadata from the disk
// into memory.  Returns DFS_SUCCESS on success, and DFS_FAIL on 
// failure.
//-------------------------------------------------------------------

int DfsOpenFileSystem() {
//Basic steps:
// Check that filesystem is not already open

// Read superblock from disk.  Note this is using the disk read rather 
// than the DFS read function because the DFS read requires a valid 
// filesystem in memory already, and the filesystem cannot be valid 
// until we read the superblock. Also, we don't know the block size 
// until we read the superblock, either.

// Copy the data from the block we just read into the superblock in memory

// All other blocks are sized by virtual block size:
// Read inodes
// Read free block vector
// Change superblock to be invalid, write back to disk, then change 
// it back to be valid in memory
    int i, j;
    disk_block * b;
    int ratio; //ratio of df_blocksize to disk_blocksize
    char *s;

    if(sb.valid) {
        printf("DfsOpenFileSystem: Filesystem already open!\n");
        return DFS_FAIL; //File system already open
    }

    //Read superblock
    printf("DfsOpenFileSystem: Reading superblock diskblocksize %d\n", diskblocksize);
    if(DiskReadBlock(DFS_SUPERBLOCK_PHYBLOCKNUM, b) != diskblocksize) {
        dbprintf('m',"DfsOpenFileSystem: Failed to read superblock from disk!\n");
        return DFS_FAIL;
    }
    bcopy(b->data, (char *)&sb, sizeof(dfs_superblock));
    printf("DfsOpenFileSystem: sb valid %d fsb size %d\n", sb.valid, sb.fs_block_size);

    ratio = sb.fs_block_size/diskblocksize;
    
    //Read inodes
    s = (char *) inodes;
    for(i = DFS_INODE_BLOCK_START; i <= DFS_INODE_BLOCK_END; i++) {
        for(j = 0; j < ratio; j++) {
            if(DiskReadBlock(i*ratio + j, b) != diskblocksize) {
                dbprintf('m',"DfsOpenFileSystem: Failed to read inodes from disk!\n");
                return DFS_FAIL;
            }
            bcopy(b->data, s, diskblocksize);
            s += diskblocksize;
        }
    }

    //Read FBV
    s = (char *) fbv;
    for(i = DFS_FBV_BLOCK_START; i <= DFS_FBV_BLOCK_END; i++) {
        for(j = 0; j < ratio; j++) {
            if(DiskReadBlock(i*ratio + j, b) != diskblocksize) {
                dbprintf('m',"DfsOpenFileSystem: Failed to read fbv from disk!\n");
                return DFS_FAIL;
            }
            bcopy(b->data, s, diskblocksize);
            s += diskblocksize;
        }
    }

    //Invalidate superblock, write to disk, then validate
    DfsInvalidate();
    bzero(b->data, diskblocksize);
    bcopy((char *)&sb, b->data, sizeof(dfs_superblock));
    i = DiskWriteBlock(DFS_SUPERBLOCK_PHYBLOCKNUM, b);
    if(i != diskblocksize) {
        dbprintf('m',"DfsOpenFileSystem: Failed to write superblock to disk %d!\n", i);
        return DFS_FAIL;
    }
    i = DiskWriteBlock(DFS_REDUNDANT_SB_PHYBLOCKNUM, b);
    if(i != diskblocksize) {
        dbprintf('m',"DfsOpenFileSystem: Failed to write superblock to disk %d!\n", i);
        return DFS_FAIL;
    }
    sb.valid = 1;
    return DFS_SUCCESS;
}


//-------------------------------------------------------------------
// DfsCloseFileSystem writes the current memory version of the
// filesystem metadata to the disk, and invalidates the memory's 
// version.
//-------------------------------------------------------------------

int DfsCloseFileSystem() {
    int i, j;
    disk_block * b;
    char * s;
    int ratio; //ratio of df_blocksize to disk_blocksize

    if(!sb.valid) {
        printf("DfsCloseFileSystem: Filesystem not open!\n");
        return DFS_FAIL;
    }

    ratio = sb.fs_block_size/diskblocksize;
    
    //Write inodes
    s = (char *) inodes;
    for(i = DFS_INODE_BLOCK_START; i <= DFS_INODE_BLOCK_END; i++) {
        for(j = 0; j < ratio; j++) {
            bcopy(s , b->data, diskblocksize);
            if(DiskWriteBlock(i*ratio + j, b) != diskblocksize) {
                printf("DfsCloseFileSystem: Failed to write inodes to disk!\n");
                return DFS_FAIL;
            }
            s += diskblocksize;
        }
    }

    //Write FBV
    s = (char *) fbv;
    //printf("before closing fbv[65535] = 0x%x\n", fbv[2047]);
    for(i = DFS_FBV_BLOCK_START; i <= DFS_FBV_BLOCK_END; i++) {
        for(j = 0; j < ratio; j++) {
            bcopy(s, b->data, diskblocksize);
            if(DiskWriteBlock(i*ratio + j, b) != diskblocksize) {
                printf("DfsCloseFileSystem: Failed to write fbv to disk!\n");
                return DFS_FAIL;
            }
            s += diskblocksize;
        }
    }

    //write sb to disk, then invalidate
    bzero(b->data, diskblocksize);
    bcopy((char *)&sb, b->data, sizeof(dfs_superblock));
    printf("before closing sb valid %d\n", sb.valid);
    if(DiskWriteBlock(DFS_SUPERBLOCK_PHYBLOCKNUM, b) != diskblocksize) {
        printf("DfsCloseFileSystem: Failed to write superblock to disk!\n");
        return DFS_FAIL;
    }
    if(DiskWriteBlock(DFS_REDUNDANT_SB_PHYBLOCKNUM, b) != diskblocksize) {
        printf("DfsCloseFileSystem: Failed to write superblock to disk!\n");
        return DFS_FAIL;
    }
    DfsInvalidate();
    return DFS_SUCCESS;
}


//-----------------------------------------------------------------
// DfsAllocateBlock allocates a DFS block for use. Remember to use 
// locks where necessary.
//-----------------------------------------------------------------

int DfsAllocateBlock() {
// Check that file system has been validly loaded into memory
// Find the first free block using the free block vector (FBV), mark it in use
// Return handle to block
    int i = 0;
    int bit_position = 0;

    if(!sb.valid) {
        printf("DfsAllocateBlock: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }

    if(LockHandleAcquire(fbv_lock) != SYNC_SUCCESS) {
        printf("DfsAllocateBlock: Failed to acquire lock!\n");
        return DFS_FAIL;
    }

    for(i = 0; i < DFS_FBV_MAX_NUM_WORDS; i++) {
        if(fbv[i] != 0) {
            for(bit_position = 0; bit_position < 32; bit_position++) {
                if( (fbv[i] & (0x1 << bit_position)) != 0 ) {
                    //Mark the block as used
                    fbv[i] &= invert(0x1 << bit_position);
                    return (i * 32 + bit_position); //Return the page number
                }
            }
        }
    }

    if(LockHandleRelease(fbv_lock) != SYNC_SUCCESS) {
        printf("DfsAllocateBlock: Failed to release lock!\n");
        return DFS_FAIL;
    }

    printf("DfsAllocateBlock: No free blocks!\n");
    return DFS_FAIL;
}


//-----------------------------------------------------------------
// DfsFreeBlock deallocates a DFS block.
//-----------------------------------------------------------------

int DfsFreeBlock(uint32 blocknum) {
    uint32 idx, bit_pos;
    if(!sb.valid) {
        printf("DfsFreeBlock: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }
    idx = blocknum/32;
    bit_pos = blocknum % 32;

    if((idx < 0) || (idx > (DFS_FBV_MAX_NUM_WORDS - 1))) {
        printf("DfsFreeBlock: Block not valid in memory!\n");
        return DFS_FAIL;
    }

    if((blocknum <= DFS_FBV_BLOCK_END) || (blocknum >= DFS_REDUNDANT_SB_PHYBLOCKNUM/4)) {
        printf("DfsFreeBlock: Illegal block to free; blocknum: %d\n", blocknum);
        return DFS_FAIL;
    }

    if(LockHandleAcquire(fbv_lock) != SYNC_SUCCESS) {
        printf("DfsAllocateBlock: Failed to acquire lock!\n");
        return DFS_FAIL;
    }

    if(fbv[idx] & (0x1 << bit_pos)) fbv[idx] |= (0x1 << bit_pos);
    else {
        if(LockHandleRelease(fbv_lock) != SYNC_SUCCESS) {
            printf("DfsAllocateBlock: Failed to release lock!\n");
            return DFS_FAIL;
        }
        return DFS_FAIL;
    }

    if(LockHandleRelease(fbv_lock) != SYNC_SUCCESS) {
        printf("DfsAllocateBlock: Failed to release lock!\n");
        return DFS_FAIL;
    }
    return DFS_SUCCESS;
}


//-----------------------------------------------------------------
// DfsReadBlock reads an allocated DFS block from the disk
// (which could span multiple physical disk blocks).  The block
// must be allocated in order to read from it.  Returns DFS_FAIL
// on failure, and the number of bytes read on success.  
//-----------------------------------------------------------------

int DfsReadBlock(uint32 blocknum, dfs_block *b) {
    uint32 idx, bit_pos;
    int i, ratio;
    disk_block * db;
    char * s;

    if(!sb.valid) {
        printf("DfsReadBlock: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }

    if((blocknum <= DFS_FBV_BLOCK_END) || (blocknum >= DFS_REDUNDANT_SB_PHYBLOCKNUM/4)) {
        printf("DfsReadBlock: Illegal block; blocknum: %d\n", blocknum);
        return DFS_FAIL;
    }

    idx = blocknum / 32;
    bit_pos = blocknum % 32;

    if((idx < 0) || (idx > (DFS_FBV_MAX_NUM_WORDS - 1))) {
        printf("DfsReadBlock: Block not valid in memory!\n");
        return DFS_FAIL;
    }

    if(LockHandleAcquire(fbv_lock) != SYNC_SUCCESS) {
        printf("DfsReadBlock: Failed to acquire lock!\n");
        return DFS_FAIL;
    }

    if(fbv[idx] & (0x1 << bit_pos)) {
        printf("DfsReadBlock: Invalid block!\n");
        return DFS_FAIL;
    }

    if(LockHandleRelease(fbv_lock) != SYNC_SUCCESS) {
        printf("DfsReadBlock: Failed to release lock!\n");
        return DFS_FAIL;
    }

    bzero(db->data, diskblocksize);
    ratio = sb.fs_block_size/diskblocksize;
    printf("DfsReadBlock: ratio %d blocknum %d\n", ratio, blocknum);

    s = b->data;
    for(i = 0; i < ratio; i++) {
        if(DiskReadBlock((blocknum*ratio + i), db) != diskblocksize) {
            printf("DfsReadBlock: Failed to read from disk!\n");
            return DFS_FAIL;
        }
        printf("read data db[255] = %d\n", db->data[0]);
        bcopy(db->data, s, diskblocksize);
        printf("copied data s[0] = %d\n", s[0]);
        s += diskblocksize;
        bzero(db->data, diskblocksize);
    }
    printf("retrunig from read\n");
    return sb.fs_block_size;
}


//-----------------------------------------------------------------
// DfsWriteBlock writes to an allocated DFS block on the disk
// (which could span multiple physical disk blocks).  The block
// must be allocated in order to write to it.  Returns DFS_FAIL
// on failure, and the number of bytes written on success.  
//-----------------------------------------------------------------

int DfsWriteBlock(uint32 blocknum, dfs_block *b){
    uint32 idx, bit_pos;
    int i, ratio;
    disk_block * db;
    char * s;

    printf("DfsWrite block %d, idx %d, bitpos %d\n", blocknum, idx, bit_pos);
    idx = blocknum/32;
    bit_pos = blocknum % 32;

    if(!sb.valid) {
        printf("DfsWriteBlock: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }

    if((blocknum <= DFS_FBV_BLOCK_END) || (blocknum >= DFS_REDUNDANT_SB_PHYBLOCKNUM/4)) {
        printf("DfsWriteBlock: Illegal block; blocknum: %d\n", blocknum);
        return DFS_FAIL;
    }

    if((idx < 0) || (idx > (DFS_FBV_MAX_NUM_WORDS - 1))) {
        printf("DfsWriteBlock: Block not valid in memory!\n");
        return DFS_FAIL;
    }

    // if(LockHandleAcquire(fbv_lock) != SYNC_SUCCESS) {
    //     printf("DfsWriteBlock: Failed to acquire lock!\n");
    //     return DFS_FAIL;
    // }

    if(fbv[idx] & (0x1 << bit_pos)) {
        printf("DfsWriteBlock: Invalid block!\n");
        return DFS_FAIL;
    }

    // if(LockHandleRelease(fbv_lock) != SYNC_SUCCESS) {
    //     printf("DfsWriteBlock: Failed to release lock!\n");
    //     return DFS_FAIL;
    // }

    printf("DfsWrite block %d, idx %d, bitpos %d\n", blocknum, idx, bit_pos);

    bzero(db->data, diskblocksize);
    ratio = sb.fs_block_size/diskblocksize;
    printf("DfsWrite block %d, idx %d, bitpos %d ratio %d\n", blocknum, idx, bit_pos, ratio);
    
    s = b->data;
    for(i = 0; i < ratio; i++) {
        bcopy(s, db->data, diskblocksize);
        if(DiskWriteBlock(blocknum*ratio + i, db) != diskblocksize) {
            printf("DfsWriteBlock: Failed to read from disk!\n");
            return DFS_FAIL;
        }
        s += diskblocksize;
        bzero(db->data, diskblocksize);
    }
    printf("DfsWrite block %d, idx %d, bitpos %d\n", blocknum, idx, bit_pos);
    return sb.fs_block_size;
}


////////////////////////////////////////////////////////////////////////////////
// Inode-based functions
////////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------
// DfsInodeFilenameExists looks through all the inuse inodes for 
// the given filename. If the filename is found, return the handle 
// of the inode. If it is not found, return DFS_FAIL.
//-----------------------------------------------------------------

uint32 DfsInodeFilenameExists(char *filename) {
    int j;
    char * name;
    if(!sb.valid) {
        printf("DfsInodeFilenameExists: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }
    for(j = 0; j < DFS_INODE_MAX_NUM; j++) {
        if(inodes[j].inuse && (dstrncmp(name, filename, DFS_MAX_FILENAME_LENGTH) == 0)) {
            return j;
        }
    }
    printf("DfsInodeFilenameExists: File inode does not exist!\n");
    return DFS_FAIL;
}

//-----------------------------------------------------------------
// DfsInodeOpen: search the list of all inuse inodes for the 
// specified filename. If the filename exists, return the handle 
// of the inode. If it does not, allocate a new inode for this 
// filename and return its handle. Return DFS_FAIL on failure. 
// Remember to use locks whenever you allocate a new inode.
//-----------------------------------------------------------------

uint32 DfsInodeOpen(char *filename) {
    uint32 handle;
    int i;

    if(!sb.valid) {
        printf("DfsInodeOpen: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }
    
    handle = DfsInodeFilenameExists(filename);
    if(handle != DFS_FAIL) return handle;

    //Allocate a new inode
    if(LockHandleAcquire(inode_lock) != SYNC_SUCCESS) {
        printf("DfsWriteBlock: Failed to acquire lock!\n");
        return DFS_FAIL;
    }
    
    for(i = 0; i < DFS_INODE_MAX_NUM; i++) {
        if(inodes[i].inuse == 0) {
            inodes[i].inuse = 1;
            if(LockHandleRelease(inode_lock) != SYNC_SUCCESS) {
                printf("DfsWriteBlock: Failed to release lock!\n");
                return DFS_FAIL;
            }
            inodes[i].fileSize = 0;
            dstrncpy(inodes[i].filename, filename, dstrlen(filename));
            bzero((char *) inodes[i].dirTransTable, sizeof(inodes[i].dirTransTable));
            inodes[i].indirectBlock = 0;
            inodes[i].doubleIndirectBlock = 0;
            return i;
        }
    }

    if(LockHandleRelease(inode_lock) != SYNC_SUCCESS) {
        printf("DfsWriteBlock: Failed to release lock!\n");
        return DFS_FAIL;
    }
    return DFS_FAIL;
}


//-----------------------------------------------------------------
// DfsInodeDelete de-allocates any data blocks used by this inode, 
// including the indirect addressing block if necessary, then mark 
// the inode as no longer in use. Use locks when modifying the 
// "inuse" flag in an inode.Return DFS_FAIL on failure, and 
// DFS_SUCCESS on success.
//-----------------------------------------------------------------

int DfsFreeDirectBlocks(uint32 * tableBase, int size) {
    /*int i;
    for(i = 0; i < size; i++) {
        if(tableBase[i] != 0) {
            if(DfsFreeBlock(tableBase[i]) == DFS_FAIL) {
                printf("DfsInodeDelete: Could not free a block!\n");
                return DFS_FAIL;
            }
        }
    }*/
    return DFS_SUCCESS;
}

int DfsFreeIndirectBlocks(uint32 blocknum) {
    /*dfs_block * temp;

    if(DfsReadBlock(blocknum, temp) != sb.fs_block_size){
        printf("DfsInodeDelete: Failed to read indirect blcok!\n");
        return DFS_FAIL;
    }
    if(DfsFreeDirectBlocks((uint32*)(temp->data), DFS_INDIR_TABLE_SIZE) == DFS_FAIL) {
        return DFS_FAIL;
    }
    if(DfsFreeBlock(blocknum) == DFS_FAIL){
        return DFS_FAIL;
    }*/
    return DFS_SUCCESS;
}

int DfsInodeDelete(uint32 handle) {
    /*uint32 i;
    dfs_block * temp;
    uint32 * baseptr;
    dfs_inode *inode;

    if(!sb.valid) {
        printf("DfsInodeDelete: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }

    if(handle < 0 || (handle > DFS_INODE_MAX_NUM)) {
        return DFS_FAIL;
    }
    
    if(LockHandleAcquire(inode_lock) != SYNC_SUCCESS) {
        printf("DfsInodeDelete: Failed to acquire lock!\n");
        return DFS_FAIL;
    }

    inode = &inodes[handle];
    inode->fileSize = 0;
    bzero(inode->filename, DFS_MAX_FILENAME_LENGTH);

    //Free direct mapped blocks
    if(DfsFreeDirectBlocks(inode->dirTransTable, DFS_DIR_TABLE_SIZE) == DFS_FAIL) {
        if(LockHandleRelease(inode_lock) != SYNC_SUCCESS) {
            printf("DfsInodeDelete: Failed to release lock!\n");
        }
        return DFS_FAIL;
    }
    bzero((char *)inode->dirTransTable, sizeof(inode->dirTransTable));

    //Free indirect mapped blocks
    if(inode->indirectBlock != 0) {
        if(DfsFreeIndirectBlocks(inode->indirectBlock) == DFS_FAIL) {
            if(LockHandleRelease(inode_lock) != SYNC_SUCCESS) {
                printf("DfsInodeDelete: Failed to release lock!\n");
            }
            return DFS_FAIL;
        }
        inode->indirectBlock = 0;
    }

    //Free double indirect blocks
    if(inode->doubleIndirectBlock != 0) {
        if(DfsReadBlock(inode->doubleIndirectBlock, temp) != sb.fs_block_size){
            printf("DfsInodeDelete: Failed to read indirect blcok!\n");
            if(LockHandleRelease(inode_lock) != SYNC_SUCCESS) {
                printf("DfsInodeDelete: Failed to release lock!\n");
            }
            return DFS_FAIL;
        }
        baseptr = (uint32*) temp->data;
        for(i = 0; i < DFS_DIR_TABLE_SIZE; i++) {
            if(baseptr[i] != 0) {
                if(DfsFreeIndirectBlocks(baseptr[i]) == DFS_FAIL) {
                    if(LockHandleRelease(inode_lock) != SYNC_SUCCESS) {
                        printf("DfsInodeDelete: Failed to release lock!\n");
                    }
                    return DFS_FAIL;
                }
            }
        }
        if(DfsFreeBlock(inode->doubleIndirectBlock) == DFS_FAIL) {
            printf("DfsInodeDelete: Could not free double indirect block!\n");
            if(LockHandleRelease(inode_lock) != SYNC_SUCCESS) {
                printf("DfsInodeDelete: Failed to release lock!\n");
            }
            return DFS_FAIL;
        }
        inode->doubleIndirectBlock = 0;
    }

    inode->inuse = 0;

    if(LockHandleRelease(inode_lock) != SYNC_SUCCESS) {
        printf("DfsInodeDelete: Failed to release lock!\n");
        return DFS_FAIL;
    }*/

    return DFS_SUCCESS;
}


//-----------------------------------------------------------------
// DfsInodeReadBytes reads num_bytes from the file represented by 
// the inode handle, starting at virtual byte start_byte, copying 
// the data to the address pointed to by mem. Return DFS_FAIL on 
// failure, and the number of bytes read on success.
//-----------------------------------------------------------------

int DfsInodeReadBytes(uint32 handle, void *mem, int start_byte, int num_bytes) {
    /*int first_vblock, last_vblock, end_byte;
    uint32 phy_block;
    dfs_block * temp;
    dfs_inode * inode;
    int size;
    int i, offset;*/
    int bytes_read;
/*
    if(!sb.valid) {
        printf("DfsInodeReadBytes: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }

    if(handle < 0 || (handle >= DFS_INODE_MAX_NUM) || (inodes[handle].inuse == 0)) {
        printf("DfsInodeReadBytes: Inode handle not valid!\n");
        return DFS_FAIL;
    }

    inode = &inodes[handle];

    if(start_byte < 0 || num_bytes < 0 || (num_bytes > inode->fileSize)) {
        printf("DfsInodeReadBytes: Invalid start/num_bytes!\n");
        return DFS_FAIL;
    } 

    //finding first and last blocks
    end_byte = start_byte + num_bytes - 1;
    first_vblock = start_byte / sb.fs_block_size; 
    last_vblock = end_byte / sb.fs_block_size;

    i = first_vblock;
    bytes_read = 0;

    while(i <= last_vblock) {
        phy_block = DfsInodeTranslateVirtualToFilesys(handle, i);
        if(phy_block == DFS_FAIL) {
            printf("DfsInodeReadBytes: Virtual block translation failed!\n");
            return DFS_FAIL;
        }
        if(DfsReadBlock(phy_block, temp) == DFS_FAIL) {
            printf("DfsInodeReadBytes: Failed to read block!\n");
            return DFS_FAIL;
        }

        //only one block, first block, last block, middle blocks
        if(first_vblock == last_vblock) {
            size = num_bytes;
            offset = start_byte % sb.fs_block_size;
        } else if (i == first_vblock) {
            offset = start_byte % sb.fs_block_size;
            size = sb.fs_block_size - offset;
        } else if (i == last_vblock) {
            offset = 0;
            size = (end_byte % sb.fs_block_size) + 1;
        } else {
            offset = 0;
            size = sb.fs_block_size;
        }

        bcopy(temp->data + offset, (char *) mem + bytes_read, size);
        bytes_read += size;
        i++;
    }*/
    return bytes_read;
}


//-----------------------------------------------------------------
// DfsInodeWriteBytes writes num_bytes from the memory pointed to 
// by mem to the file represented by the inode handle, starting at 
// virtual byte start_byte. Note that if you are only writing part 
// of a given file system block, you'll need to read that block 
// from the disk first. Return DFS_FAIL on failure and the number 
// of bytes written on success.
//-----------------------------------------------------------------

int DfsInodeWriteBytes(uint32 handle, void *mem, int start_byte, int num_bytes) {
    int first_vblock, last_vblock, end_byte;
    uint32 phy_block;
    dfs_block * temp;
    dfs_inode * inode;
    int size;
    int i, offset, retval;
    int bytes_written;

    if(!sb.valid) {
        printf("DfsInodeWriteBytes: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }

    if(handle < 0 || (handle >= DFS_INODE_MAX_NUM) || (inodes[handle].inuse == 0)) {
        printf("DfsInodeWriteBytes: Inode handle not valid!\n");
        return DFS_FAIL;
    }

    inode = &inodes[handle];

    if(start_byte < 0 || num_bytes < 0) {
        printf("DfsInodeWriteBytes: Invalid start/num_bytes!\n");
        return DFS_FAIL;
    } 

    //finding first and last blocks
    end_byte = start_byte + num_bytes - 1;
    first_vblock = start_byte / sb.fs_block_size; 
    last_vblock = end_byte / sb.fs_block_size;

    //Check if blocks before first block are even allocated
    //if not allocate them
    for(i = 0; i < first_vblock; i++) {
        phy_block = DfsInodeTranslateVirtualToFilesys(handle, i);
        if(phy_block == DFS_FAIL){
            phy_block = DfsInodeAllocateVirtualBlock(handle, (uint32) i);
            if(phy_block == DFS_FAIL) return DFS_FAIL;
        }
    }

    i = first_vblock;
    bytes_written = 0;

    while(i <= last_vblock) {
        phy_block = DfsInodeTranslateVirtualToFilesys(handle, i);
        printf("translated: phy %d virt %d\n", phy_block, i);
        if(phy_block == DFS_FAIL) {
            phy_block = DfsInodeAllocateVirtualBlock(handle, i);
            printf("translated: phy %d virt %d\n", phy_block, i);
            if(phy_block == DFS_FAIL) return DFS_FAIL;
        }

        //only one block, first block, last block, middle blocks
        if(first_vblock == last_vblock) {
            size = num_bytes;
            offset = start_byte % sb.fs_block_size;
            if(size < sb.fs_block_size) {
                printf("in this condition %d\n", phy_block);
                retval = DfsReadBlock(phy_block, temp);
                if(retval == DFS_FAIL) {
                    printf("DfsInodeWriteBytes: Failed to read block!\n");
                    return DFS_FAIL;
                }
                printf("out of this condition %d\n", phy_block);
            }
        } else if (i == first_vblock) {
            offset = start_byte % sb.fs_block_size;
            size = sb.fs_block_size - offset;
            if(size < sb.fs_block_size) {
                retval = DfsReadBlock(phy_block, temp);
                if(retval == DFS_FAIL) {
                    printf("DfsInodeWriteBytes: Failed to read block!\n");
                    return DFS_FAIL;
                }
            }
        } else if (i == last_vblock) {
            offset = 0;
            size = (end_byte % sb.fs_block_size) + 1;
            if(size < sb.fs_block_size) {
                retval = DfsReadBlock(phy_block, temp);
                if(retval == DFS_FAIL) {
                    printf("DfsInodeWriteBytes: Failed to read block!\n");
                    return DFS_FAIL;
                }
            }
        } else {
            offset = 0;
            size = sb.fs_block_size;
        }

        bcopy((char *) mem + bytes_written, temp->data + offset, size);
        printf("reached this last write\n");
        retval = DfsWriteBlock(phy_block, temp);
        if(retval == DFS_FAIL) return DFS_FAIL;
        bytes_written += size;
        i++;
    }

    if(bytes_written != num_bytes) return DFS_FAIL;

    if(inode->fileSize < (end_byte + 1)) {
        inode->fileSize = end_byte + 1;
    }

    return bytes_written;
}


//-----------------------------------------------------------------
// DfsInodeFilesize simply returns the size of an inode's file. 
// This is defined as the maximum virtual byte number that has 
// been written to the inode thus far. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

uint32 DfsInodeFilesize(uint32 handle) {
    if(!sb.valid) {
        printf("DfsInodeReadBytes: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }

    if(handle < 0 || (handle >= DFS_INODE_MAX_NUM) || (inodes[handle].inuse == 0)) {
        printf("DfsInodeReadBytes: Inode handle not valid!\n");
        return DFS_FAIL;
    }
    
    return inodes[handle].fileSize; 
}


//-----------------------------------------------------------------
// DfsInodeAllocateVirtualBlock allocates a new filesystem block 
// for the given inode, storing its blocknumber at index 
// virtual_blocknumber in the translation table. If the 
// virtual_blocknumber resides in the indirect address space, and 
// there is not an allocated indirect addressing table, allocate it. 
// Return DFS_FAIL on failure, and the newly allocated file system 
// block number on success.
//-----------------------------------------------------------------

uint32 DfsInodeAllocateVirtualBlock(uint32 handle, uint32 virtual_blocknum) {
    dfs_inode * inode;
    dfs_block * temp1, * temp2;
    uint32 * base1, * base2;
    uint32 phy_blocknum;
    int idx1, idx2, retval;

    if(!sb.valid) {
        printf("DfsInodeAllocateVirtualBlock: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }
    if(handle < 0 || (handle >= DFS_INODE_MAX_NUM) || (inodes[handle].inuse == 0)) {
        printf("DfsInodeAllocateVirtualBlock: Inode handle not valid!\n");
        return DFS_FAIL;
    }
    if(virtual_blocknum < 0 || (virtual_blocknum >= DFS_MAX_NUM_VIRTUAL_BLOCKS)) {
        printf("DfsInodeAllocateVirtualBlock: Illegal virtual blocknum!\n");
        return DFS_FAIL;
    }

    inode = &inodes[handle];
    printf("here in inodeAllocate\n");
    phy_blocknum = DfsAllocateBlock();
    printf("allocated phy block %d in inodeAllocate\n", phy_blocknum);
    if(phy_blocknum == DFS_FAIL) return DFS_FAIL;

    bzero(temp1->data, sb.fs_block_size);
    retval = DfsWriteBlock(phy_blocknum, temp1);
    if(retval == DFS_FAIL) return DFS_FAIL;
    
    if(virtual_blocknum < DFS_DIR_TABLE_SIZE) {
        inode->dirTransTable[virtual_blocknum] = phy_blocknum;
        printf("updated translation table %d %d\n", virtual_blocknum, phy_blocknum);
    } else if(virtual_blocknum < (DFS_DIR_TABLE_SIZE + DFS_INDIR_TABLE_SIZE)) {
        if(inode->indirectBlock == 0) {
            inode->indirectBlock = DfsAllocateBlock();
            if(inode->indirectBlock == DFS_FAIL) return DFS_FAIL;
            bzero(temp1->data, sb.fs_block_size);
            if(DfsWriteBlock(inode->indirectBlock, temp1) == DFS_FAIL) return DFS_FAIL;
        }
        virtual_blocknum -= DFS_DIR_TABLE_SIZE;
        if(DfsReadBlock(inode->indirectBlock, temp1) != sb.fs_block_size) return DFS_FAIL;
        base1 = (uint32 *) temp1->data;
        base1[virtual_blocknum] = phy_blocknum;
        if(DfsWriteBlock(inode->indirectBlock, temp1) == DFS_FAIL) return DFS_FAIL;
    } else {
        if(inode->doubleIndirectBlock == 0) {
            inode->doubleIndirectBlock = DfsAllocateBlock();
            if(inode->doubleIndirectBlock == DFS_FAIL) return DFS_FAIL;
            bzero(temp1->data, sb.fs_block_size);
            if(DfsWriteBlock(inode->doubleIndirectBlock, temp1) == DFS_FAIL) return DFS_FAIL;
        }
        virtual_blocknum -= (DFS_DIR_TABLE_SIZE + DFS_INDIR_TABLE_SIZE);
        idx1 = virtual_blocknum / DFS_INDIR_TABLE_SIZE;
        idx2 = virtual_blocknum % DFS_INDIR_TABLE_SIZE;
        if(DfsReadBlock(inode->doubleIndirectBlock, temp1) != sb.fs_block_size) return DFS_FAIL;
        base1 = (uint32*) temp1->data;
        if(base1[idx1] == 0) {
            base1[idx1] = DfsAllocateBlock();
            if(base1[idx1] == DFS_FAIL) return DFS_FAIL;
            bzero(temp2->data, sb.fs_block_size);
            if(DfsWriteBlock(base1[idx1], temp2) == DFS_FAIL) return DFS_FAIL;
        }
        if(DfsReadBlock(base1[idx1], temp2) != sb.fs_block_size) return DFS_FAIL;
        base2 = (uint32*) temp2->data;
        base2[idx2] = phy_blocknum;
        if(DfsWriteBlock(base1[idx1], temp2) == DFS_FAIL) return DFS_FAIL;
        if(DfsWriteBlock(inode->doubleIndirectBlock, temp1) == DFS_FAIL) return DFS_FAIL;
    }
    
    printf("here in inodeAllocate initialized new block %d\n", phy_blocknum);

    return phy_blocknum;
}


//-----------------------------------------------------------------
// DfsInodeTranslateVirtualToFilesys translates the 
// virtual_blocknum to the corresponding file system block using 
// the inode identified by handle. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

uint32 DfsInodeTranslateVirtualToFilesys(uint32 handle, uint32 virtual_blocknum) {
    /*dfs_inode * inode;
    dfs_block * temp1, * temp2;
    uint32 * base1, * base2;
    uint32 phy_blocknum;
    int total_num_blocks;
    int idx1, idx2;

    if(!sb.valid) {
        printf("DfsInodeTranslateVirtualToFilesys: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }

    if(handle < 0 || (handle >= DFS_INODE_MAX_NUM) || (inodes[handle].inuse == 0)) {
        printf("DfsInodeTranslateVirtualToFilesys: Inode handle not valid!\n");
        return DFS_FAIL;
    }

    inode = &inodes[handle];
    total_num_blocks = inode->fileSize / sb.fs_block_size;

    if(virtual_blocknum < 0 || (virtual_blocknum >= DFS_MAX_NUM_VIRTUAL_BLOCKS) || (virtual_blocknum >= total_num_blocks)) {
        printf("DfsInodeTranslateVirtualToFilesys: Illegal virtual blocknum!\n");
        return DFS_FAIL;
    }

    if(virtual_blocknum < DFS_DIR_TABLE_SIZE) {
        phy_blocknum = inode->dirTransTable[virtual_blocknum];
        if(phy_blocknum > DFS_FBV_BLOCK_END && phy_blocknum < (DFS_REDUNDANT_SB_PHYBLOCKNUM/4)) {
            return phy_blocknum;
        } else {
            printf("DfsInodeTranslateVirtualToFilesys: Illegal physical blocknum!\n");
            return DFS_FAIL;
        }
    } else if(virtual_blocknum < (DFS_DIR_TABLE_SIZE + DFS_INDIR_TABLE_SIZE)) {
        if(inode->indirectBlock != 0) {
            virtual_blocknum -= DFS_DIR_TABLE_SIZE;
            if(DfsReadBlock(inode->indirectBlock, temp1) != sb.fs_block_size) return DFS_FAIL;
            base1 = (uint32 *) temp1->data;
            phy_blocknum = base1[virtual_blocknum];
            if(phy_blocknum > DFS_FBV_BLOCK_END && phy_blocknum < (DFS_REDUNDANT_SB_PHYBLOCKNUM/4)) {
                return phy_blocknum;
            } else {
                printf("DfsInodeTranslateVirtualToFilesys: Illegal physical blocknum!\n");
                return DFS_FAIL;
            }
        } else return DFS_FAIL;
    } else {
        if(inode->doubleIndirectBlock != 0) {
            virtual_blocknum -= (DFS_DIR_TABLE_SIZE + DFS_INDIR_TABLE_SIZE);
            idx1 = virtual_blocknum / DFS_INDIR_TABLE_SIZE;
            idx2 = virtual_blocknum % DFS_INDIR_TABLE_SIZE;
            if(DfsReadBlock(inode->doubleIndirectBlock, temp1) != sb.fs_block_size)
                return DFS_FAIL;
            base1 = (uint32*) temp1->data;
            if(base1[idx1] != 0) {
                if(DfsReadBlock(base1[idx1], temp2) != sb.fs_block_size)
                    return DFS_FAIL;
                base2 = (uint32*) temp2->data;
                phy_blocknum = base2[idx2];
                if(phy_blocknum > DFS_FBV_BLOCK_END && phy_blocknum < (DFS_REDUNDANT_SB_PHYBLOCKNUM/4)) {
                    return phy_blocknum;
                } else {
                    printf("DfsInodeTranslateVirtualToFilesys: Illegal physical blocknum!\n");
                    return DFS_FAIL;
                }
            } else return DFS_FAIL;
        } else return DFS_FAIL;
    }*/
    return DFS_FAIL;
}

int DfsInodeFileRename(char *oldname, char *newname) {
    /*int i = DfsInodeFilenameExists(oldname);
    int len = dstrlen(newname);

    if(i == DFS_FAIL) return DFS_FAIL;

    if(DfsInodeFilenameExists(newname) != DFS_FAIL) return DFS_FAIL;

    bzero(inodes[i].filename, DFS_MAX_FILENAME_LENGTH);
    dstrncpy(inodes[i].filename, newname, len);*/
    return DFS_SUCCESS;
}



///Functions for debug
// void DfsPrintSuperblock() {
//     printf("DFS Superblock:\n");
//     printf("  valid           : %d\n", sb.valid);
//     printf("  fsBlockSize     : %d bytes\n", sb.fs_block_size);
//     printf("  numFsBlocks     : %d\n", sb.num_fs_blocks);
//     printf("  firstInodeBlock : %d\n", sb.inode_start_block);
//     printf("  numInodes       : %d\n", sb.num_inodes);
//     printf("  firstFBVBlock   : %d\n", sb.fbv_start_block);
// }

// void DfsPrintInode(dfs_inode *inode) {
//     int i;
//     printf("DFS Inode:\n");
//     printf("  inuse                    : %d\n", inode->inuse);
//     printf("  fileSize                 : %d\n", inode->fileSize);

//     printf("  dirTransTable            : [");
//     for ( i = 0; i < 10; i++) {
//         printf("%d", inode->dirTransTable[i]);
//         if (i != 9) printf(", ");
//     }
//     printf("]\n");

//     printf("  indirTransTableBase      : %d\n", inode->indirectBlock);
//     printf("  doubleIndirTransTableBase: %d\n", inode->doubleIndirectBlock);

//     printf("  fileName                 : \"%s\"\n", inode->filename);
// }

// void DfsPrintInodeTable() {
//     int i;
//     printf("=== DFS Inode Table (%d inodes) ===\n", DFS_INODE_MAX_NUM);

//     for ( i = 0; i < DFS_INODE_MAX_NUM; i++) {
//         printf("\n--- Inode %d ---\n", i);
//         DfsPrintInode(&inodes[i]);
//     }
// }

// void DfsPrintFBVBlocks() {
//     int block;
//     printf("=== DFS Free Block Status ===\n");
//     for ( block = 0; block < DFS_FBV_MAX_NUM_WORDS; block++) {
//         printf("FBV[%d] = 0x%x\n", block, fbv[block]);
//     }
// }

// void DfsReadDiskSuperblock() {
//     int i;
//     disk_block *b;
//     dfs_superblock sb;

//     i = DiskReadBlock(DFS_SUPERBLOCK_PHYBLOCKNUM, b);
//     bcopy(b->data, (char *)&sb, sizeof(dfs_superblock));
//     printf("Disk Superblock:\n");
//     printf("  valid           : %d\n", sb.valid);
//     printf("  fsBlockSize     : %d bytes\n", sb.fs_block_size);
//     printf("  numFsBlocks     : %d\n", sb.num_fs_blocks);
//     printf("  firstInodeBlock : %d\n", sb.inode_start_block);
//     printf("  numInodes       : %d\n", sb.num_inodes);
//     printf("  firstFBVBlock   : %d\n", sb.fbv_start_block);
// }