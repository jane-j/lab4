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

// You have already been told about the most likely places where you should use locks. You may use 
// additional locks if it is really necessary.

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
    sb.valid = 0;
    DfsOpenFileSystem();
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

    if(sb.valid) {
        printf("DfsOpenFileSystem: Filesystem already open!\n");
        return DFS_FAIL; //File system already open
    }

    //Read superblock
    if(DiskReadBlock(DFS_SUPERBLOCK_PHYBLOCKNUM, b) != DISK_BLOCKSIZE) {
        printf("DfsOpenFileSystem: Failed to read superblock from disk!\n");
        return DFS_FAIL;
    }
    bcopy(b, &sb, DISK_BLOCKSIZE);

    ratio = sb.fsBlockSize/DISK_BLOCKSIZE;
    
    //Read inodes
    for(i = DFS_INODE_BLOCK_START; i <= DFS_INODE_BLOCK_END; i++) {
        for(j = 0; j < ratio; j++) {
            if(DiskReadBlock(i*ratio + j, b) != DISK_BLOCKSIZE) {
                printf("DfsOpenFileSystem: Failed to read inodes from disk!\n");
                return DFS_FAIL;
            }
        }
        bcopy(b, inodes + (i-DFS_INODE_BLOCK_START)*DISK_BLOCKSIZE, DISK_BLOCKSIZE);
    }

    //Read FBV
    for(i = DFS_FBV_BLOCK_START; i <= DFS_FBV_BLOCK_END; i++) {
        for(j = 0; j < ratio; j++) {
            if(DiskReadBlock(i*ratio + j, b) != DISK_BLOCKSIZE) {
                printf("DfsOpenFileSystem: Failed to read fbv from disk!\n");
                return DFS_FAIL;
            }
        }
        bcopy(b, fbv + (i-DFS_FBV_BLOCK_START)*DISK_BLOCKSIZE, DISK_BLOCKSIZE);
    }

    //Invalidate superblock, write to disk, then validate
    sb.valid = 0;
    bcopy(&sb, b, DISK_BLOCKSIZE);
    if(DiskWriteBlock(DFS_SUPERBLOCK_PHYBLOCKNUM, b) != DFS_BLOCKSIZE) {
        printf("DfsOpenFileSystem: Failed to write superblock to disk!\n");
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
    int ratio; //ratio of df_blocksize to disk_blocksize

    if(!sb.valid) {
        printf("DfsCloseFileSystem: Filesystem not open!");
        return DFS_FAIL;
    }

    ratio = sb.fsBlockSize/DISK_BLOCKSIZE;
    
    //Write inodes
    for(i = DFS_INODE_BLOCK_START; i <= DFS_INODE_BLOCK_END; i++) {
        for(j = 0; j < ratio; j++) {
            bcopy(inodes + (i-DFS_INODE_BLOCK_START)*DISK_BLOCKSIZE, b, DISK_BLOCKSIZE);
            if(DiskWriteBlock(i*ratio + j, b) != DISK_BLOCKSIZE) {
                printf("DfsCloseFileSystem: Failed to write inodes to disk!\n");
                return DFS_FAIL;
            }
        }
    }

    //Write FBV
    for(i = DFS_FBV_BLOCK_START; i <= DFS_FBV_BLOCK_END; i++) {
        bcopy(fbv + (i-DFS_FBV_BLOCK_START)*DISK_BLOCKSIZE, b, DISK_BLOCKSIZE);
        for(j = 0; j < ratio; j++) {
            if(DiskWriteBlock(i*ratio + j, b) != DISK_BLOCKSIZE) {
                printf("DfsCloseFileSystem: Failed to write fbv to disk!\n");
                return DFS_FAIL;
            }
        }
        
    }

    //write sb to disk, then invalidate
    bcopy(&sb, b, DISK_BLOCKSIZE);
    if(DiskWriteBlock(DFS_SUPERBLOCK_PHYBLOCKNUM, b) != DFS_BLOCKSIZE) {
        printf("DfsOpenFileSystem: Failed to write superblock to disk!\n");
        return DFS_FAIL;
    }
    sb.valid = 0;
    return DFS_SUCCESS;
}


//-----------------------------------------------------------------
// DfsAllocateBlock allocates a DFS block for use. Remember to use 
// locks where necessary.
//-----------------------------------------------------------------

uint32 DfsAllocateBlock() {
// Check that file system has been validly loaded into memory
// Find the first free block using the free block vector (FBV), mark it in use
// Return handle to block

//TODO: USE LOCKS
    int i = 0;
    int bit_position = 0;

    if(!sb.valid) {
        printf("DfsAllocateBlock: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }

    for(i = 0; i < DFS_FBV_MAX_NUM_WORDS; i++) {
        if(fbv[i] != 0) {
            //printf("MemoryAllocPage: Free page at %d!\n", i);
            for(bit_position = 0; bit_position < 32; bit_position++) {
                if( (fbv[i] & (0x1 << bit_position)) != 0 ) {
                    //Mark the block as used
                    fbv[i] &= invert(0x1 << bit_position);
                    return (i * 32 + bit_position); //Return the page number
                }
            }
        }
    }

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
    fbv[idx] |= (0x1 << bit_pos);
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
    int i;
    disk_block * db;

    if(!sb.valid) {
        printf("DfsReadBlock: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }
    if(fbv[idx] & (0x1 << bit_pos)) {
        printf("DfsReadBlock: Invalid block!\n");
        return DFS_FAIL;
    }

    for(i = 0; i < (DFS_BLOCKSIZE/DISK_BLOCKSIZE); i++) {
        if(DiskReadBlock(blocknum*(DFS_BLOCKSIZE/DISK_BLOCKSIZE) + i, db) != DISK_BLOCKSIZE) {
            printf("DfsReadBlock: Failed to read from disk!\n");
            return DFS_FAIL;
        }
        bcopy(db, b + i*DISK_BLOCKSIZE, DISK_BLOCKSIZE);
    }
    return DFS_BLOCKSIZE;
}


//-----------------------------------------------------------------
// DfsWriteBlock writes to an allocated DFS block on the disk
// (which could span multiple physical disk blocks).  The block
// must be allocated in order to write to it.  Returns DFS_FAIL
// on failure, and the number of bytes written on success.  
//-----------------------------------------------------------------

int DfsWriteBlock(uint32 blocknum, dfs_block *b){
    uint32 idx, bit_pos;
    int i;
    disk_block * db;

    if(!sb.valid) {
        printf("DfsWriteBlock: Filesystem not valid in memory!\n");
        return DFS_FAIL;
    }
    if(fbv[idx] & (0x1 << bit_pos)) {
        printf("DfsWriteBlock: Invalid block!\n");
        return DFS_FAIL;
    }

    for(i = 0; i < (DFS_BLOCKSIZE/DISK_BLOCKSIZE); i++) {
        bcopy(b + i*DISK_BLOCKSIZE, db, DISK_BLOCKSIZE);
        if(DiskWriteBlock(blocknum*(DFS_BLOCKSIZE/DISK_BLOCKSIZE) + i, db) != DISK_BLOCKSIZE) {
            printf("DfsWriteBlock: Failed to read from disk!\n");
            return DFS_FAIL;
        }
    }
    return DFS_BLOCKSIZE;
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

}


//-----------------------------------------------------------------
// DfsInodeOpen: search the list of all inuse inodes for the 
// specified filename. If the filename exists, return the handle 
// of the inode. If it does not, allocate a new inode for this 
// filename and return its handle. Return DFS_FAIL on failure. 
// Remember to use locks whenever you allocate a new inode.
//-----------------------------------------------------------------

uint32 DfsInodeOpen(char *filename) {

}


//-----------------------------------------------------------------
// DfsInodeDelete de-allocates any data blocks used by this inode, 
// including the indirect addressing block if necessary, then mark 
// the inode as no longer in use. Use locks when modifying the 
// "inuse" flag in an inode.Return DFS_FAIL on failure, and 
// DFS_SUCCESS on success.
//-----------------------------------------------------------------

int DfsInodeDelete(uint32 handle) {

}


//-----------------------------------------------------------------
// DfsInodeReadBytes reads num_bytes from the file represented by 
// the inode handle, starting at virtual byte start_byte, copying 
// the data to the address pointed to by mem. Return DFS_FAIL on 
// failure, and the number of bytes read on success.
//-----------------------------------------------------------------

int DfsInodeReadBytes(uint32 handle, void *mem, int start_byte, int num_bytes) {

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


}


//-----------------------------------------------------------------
// DfsInodeFilesize simply returns the size of an inode's file. 
// This is defined as the maximum virtual byte number that has 
// been written to the inode thus far. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

uint32 DfsInodeFilesize(uint32 handle) {

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


}



//-----------------------------------------------------------------
// DfsInodeTranslateVirtualToFilesys translates the 
// virtual_blocknum to the corresponding file system block using 
// the inode identified by handle. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

uint32 DfsInodeTranslateVirtualToFilesys(uint32 handle, uint32 virtual_blocknum) {

}
