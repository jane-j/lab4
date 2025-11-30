#include "ostraps.h"
#include "dlxos.h"
#include "traps.h"
#include "queue.h"
#include "disk.h"
#include "dfs.h"
#include "synch.h"
#include "clock.h"

static dfs_superblock sb; // superblock
static dfs_inode inodes[DFS_INODE_MAX_NUM]; // all inodes
static uint32 fbv[DFS_FBV_MAX_NUM_WORDS]; // Free block vector
static cache_block cache[CACHE_SIZE]; // cache blocks

////////////////////////////////////////////

static int num_disk_reads;
static int num_disk_writes;
static int num_hits;
static int num_total_accesses;
static double miss_latency;

//////////////////////////////////////////////

static int last_blocknum_read;
static int cur_wnd_read;
static Queue prefetch_queue_read;
static int last_blocknum_write;
static int cur_wnd_write;
static Queue prefetch_queue_write;
static char not_translation;

////////////////////////////////////////////////

static uint32 negativeone = 0xFFFFFFFF;
static inline uint32 invert(uint32 n) { return n ^ negativeone; }

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
  int i;

  DfsInvalidate();
  DfsOpenFileSystem();

  inode_lock = LockCreate();
  fbv_lock = LockCreate();
  cache_lock = LockCreate();

  num_disk_reads = 0;
  num_disk_writes = 0;
  num_hits = 0;
  num_total_accesses = 0;
  miss_latency = 0.0;

  for(i = 0; i < CACHE_SIZE; i++)
  {
    cache[i].valid = 0;
    cache[i].dirty = 0;
    cache[i].timestamp = 0;
    cache[i].blocknum = 0; 
    bzero(cache[i].data, DFS_BLOCKSIZE);
    cache[i].l = NULL;
  }

  last_blocknum_read = -1;
  cur_wnd_read = DEF_WND;
  AQueueInit(&prefetch_queue_read);
  last_blocknum_write = -1;
  cur_wnd_write = DEF_WND;
  AQueueInit(&prefetch_queue_write);
  not_translation = 1;
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
  disk_block temp_db;
  dfs_block temp_b;
  int i, retval;

  if(sb.valid == 1)
  {
    return DFS_FAIL;
  }

  //Read superblock
  retval = DiskReadBlock(DFS_SUPERBLOCK_PHY_BLOCKNUM, &temp_db);

  if(retval == DISK_FAIL)
  {
    return DFS_FAIL;
  }

  bcopy(temp_db.data, (char *)&sb, sizeof(dfs_superblock));

  //Read inodes
  for(i = sb.start_block_inode; i < sb.start_block_fbv; i++)
  {
    retval = DfsReadBlock(i, &temp_b);

    if(retval == DFS_FAIL)
    {
        return DFS_FAIL;
    }

    bcopy(temp_b.data, (char *)(inodes + ((i - sb.start_block_inode) * (sb.fs_blocksize / sizeof(dfs_inode)))), sb.fs_blocksize);
  }

  for(i = sb.start_block_fbv; i < sb.start_block_data; i++)
  {
    retval = DfsReadBlock(i, &temp_b);
    
    if(retval == DFS_FAIL)
    {
        return DFS_FAIL;
    }

    bcopy(temp_b.data, (char *)(fbv + ((i - sb.start_block_fbv) * (sb.fs_blocksize / 4))), sb.fs_blocksize);
  }

  DfsInvalidate();

  bzero(temp_db.data, DiskBytesPerBlock());
  bcopy((char *)&sb, temp_db.data, sizeof(dfs_superblock));
  retval = DiskWriteBlock(DFS_SUPERBLOCK_PHY_BLOCKNUM, &temp_db);
  if(retval == DISK_FAIL)
  {
    return DFS_FAIL;
  }
  retval = DiskWriteBlock(DFS_REDUNDANT_SB_PHY_BLOCKNUM, &temp_db);
  if(retval == DISK_FAIL)
  {
    return DFS_FAIL;
  }

  sb.valid = 1;
  num_disk_reads = 0;
  num_disk_writes = 0;
  return DFS_SUCCESS;
}


//-------------------------------------------------------------------
// DfsCloseFileSystem writes the current memory version of the
// filesystem metadata to the disk, and invalidates the memory's 
// version.
//-------------------------------------------------------------------

int DfsCloseFileSystem() {
  int retval;
  disk_block temp_db;
  dfs_block temp_b;
  int i;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  for(i = sb.start_block_inode; i < sb.start_block_fbv; i++)
  {
    bcopy((char *)(inodes + ((i - sb.start_block_inode) * (sb.fs_blocksize / sizeof(dfs_inode)))), temp_b.data, sb.fs_blocksize);
    retval = DfsWriteBlock(i, &temp_b);

    if(retval == DFS_FAIL)
    {
      return DFS_FAIL;
    }
  }

  for(i = sb.start_block_fbv; i < sb.start_block_data; i++)
  {
    bcopy((char *)(fbv + ((i - sb.start_block_fbv) * (sb.fs_blocksize / 4))), temp_b.data, sb.fs_blocksize);
    retval = DfsWriteBlock(i, &temp_b);
    
    if(retval == DFS_FAIL)
    {
      return DFS_FAIL;
    }
  }
  
  bzero(temp_db.data, DiskBytesPerBlock());
  bcopy((char *)&sb, temp_db.data, sizeof(dfs_superblock));
  retval = DiskWriteBlock(DFS_SUPERBLOCK_PHY_BLOCKNUM, &temp_db);
  if(retval == DISK_FAIL)
  {
    return DFS_FAIL;
  }

  retval = DiskWriteBlock(DFS_REDUNDANT_SB_PHY_BLOCKNUM, &temp_db);
  if(retval == DISK_FAIL)
  {
    return DFS_FAIL;
  }

  if(DfsCacheFlush() == DFS_FAIL)
  {
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
  int j = 0;
  uint32 temp;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  if(LockHandleAcquire(fbv_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  for(i = 0; i <= DFS_FBV_MAX_NUM_WORDS; i++) {
    if(fbv[i] != 0) {
      for(j = 0; j <= 31; j++) {
        if((fbv[i] >> j) & 0x1) {
          temp = fbv[i];
          fbv[i] = fbv[i] & invert(0x1 << j);

          if(LockHandleRelease(fbv_lock) == SYNC_FAIL)
          {
            fbv[i] = temp;
            return DFS_FAIL;
          }

          return (i * 32 + j);
        }
      }
    }
  }

  LockHandleRelease(fbv_lock);
  return DFS_FAIL;
}


//-----------------------------------------------------------------
// DfsFreeBlock deallocates a DFS block.
//-----------------------------------------------------------------

int DfsFreeBlock(int blocknum) {
  int i = blocknum / 32;
  int j = blocknum % 32;
  int idx;

  if(i < 0 || i > DFS_FBV_MAX_NUM_WORDS - 1)
  {
    return DFS_FAIL;
  }

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  if(LockHandleAcquire(fbv_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  fbv[i] = fbv[i] | (0x1 << j);

  if(LockHandleRelease(fbv_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  if(LockHandleAcquire(cache_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  for(idx = 0; idx <= CACHE_SIZE - 1; idx++)
  {
    if(cache[idx].valid == 1 && cache[idx].blocknum == blocknum)
    {
      cache[idx].valid = 0;
      cache[idx].dirty = 0;
      break;
    }
  } 

  if(LockHandleRelease(cache_lock) == SYNC_FAIL)
  {
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
// int DfsReadBlock(int blocknum, dfs_block *b) {
//   int fbv_idx = blocknum / 32;
//   int cache_handle;
//   int total_read;
//   double latency_time;
//   int miss_latency_int;
//   double hit_rate;
//   double miss_rate;
//   int i;
//   cache_block *c=NULL;
//   dfs_block *temp;
//   int cache_handle_temp;

//   if(sb.valid == 0)
//   {
//     return DFS_FAIL;
//   }

//   if(fbv_idx < 0 || fbv_idx > DFS_FBV_MAX_NUM_WORDS - 1)
//   {
//     return DFS_FAIL;
//   }

//   if((fbv[fbv_idx] & ((0x1) << (blocknum % 32))) != 0)
//   {
//     return DFS_FAIL;
//   }

//   if(LockHandleAcquire(cache_lock) == SYNC_FAIL)
//   {
//     return DFS_FAIL;
//   }

//   cache_handle = DfsCacheHit(blocknum);
  
//   if(cache_handle != DFS_FAIL)
//   {
//     bcopy(cache[cache_handle].data, b->data, sb.fs_blocksize);
//     cache[cache_handle].timestamp = ClkGetCurTime();
//     AQueueRemove(&(cache[cache_handle].l));

//     if(LockHandleRelease(cache_lock) == SYNC_FAIL)
//     {
//       return DFS_FAIL;
//     } 

//     if(not_translation != 0)
//     {
//       last_blocknum_read = blocknum;
//     }
//     return sb.fs_blocksize;
//   }
  
//   latency_time = ClkGetCurTime();

//   total_read = DfsReadBlockUncached(blocknum, b);

//   if(total_read == DFS_FAIL)
//   {
//     LockHandleRelease(cache_lock);
//     return DFS_FAIL;
//   }

//   cache_handle = DfsCacheAllocateSlot(blocknum);

//   if(cache_handle == DFS_FAIL)
//   {
//     LockHandleRelease(cache_lock);
//     return DFS_FAIL;
//   }
  
//   cache[cache_handle].valid = 1;
//   cache[cache_handle].dirty = 0;
//   cache[cache_handle].blocknum = blocknum;
//   bcopy(b->data, cache[cache_handle].data, sb.fs_blocksize);
//   cache[cache_handle].timestamp = ClkGetCurTime();

//   // Added for bulk read ////////////////////////////////////////////
//   if(not_translation != 0)
//   {
//     if(blocknum == last_blocknum_read + 1)
//     {
//       cur_wnd_read = ((cur_wnd_read * 2) > MAX_WND) ? MAX_WND : (cur_wnd_read * 2);
//     }
//     else 
//     {
//       cur_wnd_read = DEF_WND;
//       while (!AQueueEmpty(&prefetch_queue_read)) {
//         c = (cache_block *)AQueueObject(AQueueFirst(&prefetch_queue_read));
//         c->valid = 0;
//         c->dirty = 0;
//         if (AQueueRemove(&(c->l)) != QUEUE_SUCCESS)
//         {
//           return DFS_FAIL;
//         }
//       }
//     }

//     for(i = blocknum + 1; i <= blocknum + cur_wnd_read - 1; i++)
//     {
//       if(DfsReadBlockUncached(i, temp) == DFS_FAIL)
//       {
//         LockHandleRelease(cache_lock);
//         return DFS_FAIL;
//       }

//       cache_handle_temp = DfsCacheAllocateSlot(i);

//       if(cache_handle_temp == DFS_FAIL)
//       {
//         LockHandleRelease(cache_lock);
//         return DFS_FAIL;
//       }
      
//       cache[cache_handle_temp].valid = 1;
//       cache[cache_handle_temp].dirty = 0;
//       cache[cache_handle_temp].blocknum = i;
//       bcopy(temp->data, cache[cache_handle_temp].data, sb.fs_blocksize);
//       cache[cache_handle_temp].timestamp = latency_time;

//       if ((cache[cache_handle_temp].l = AQueueAllocLink(&cache[cache_handle_temp])) == NULL)
//       {
//         return DFS_FAIL;
//       }
//       if (AQueueInsertLast(&prefetch_queue_read, cache[cache_handle_temp].l) != QUEUE_SUCCESS)
//       {
//         return DFS_FAIL;
//       }
//     }
//   }
//   /////////////////////////////////////////////////////////////////
//   miss_latency = miss_latency + ((cache[cache_handle].timestamp - latency_time) - miss_latency) / (num_total_accesses - num_hits);
//   // miss_latency = (miss_latency * (num_total_accesses - num_hits - 1) + (cache[cache_handle].timestamp - latency_time)) / (num_total_accesses - num_hits);
//   miss_latency_int = (int)miss_latency;
//   hit_rate = (double)num_hits / (double)num_total_accesses * 100;
//   miss_rate = 100.0 - hit_rate;
//   printf("Cache Miss: Hit Rate = %.3f%%, Miss Rate = %.3f%%, ", hit_rate, miss_rate);
//   printf("Disk Reads = %d, Disk Writes = %d, Miss Handling Latency = %dms\n", num_disk_reads, num_disk_writes, miss_latency_int);

//   if(LockHandleRelease(cache_lock) == SYNC_FAIL)
//   {
//     return DFS_FAIL;
//   }

//   if(not_translation != 0)
//   {
//     last_blocknum_read = blocknum;
//   }
//   return total_read;
// }

int DfsReadBlock(int blocknum, dfs_block *b) {
  int i;
  int ratio = sb.fs_blocksize / DiskBytesPerBlock();
  int data_index;
  disk_block temp;
  int fbv_idx = blocknum / 32;
  int total_bytes_read = 0;
  int bytes_read;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  if(fbv_idx < 0 || fbv_idx > DFS_FBV_MAX_NUM_WORDS - 1)
  {
    return DFS_FAIL;
  }

  if((fbv[fbv_idx] & ((0x1) << (blocknum % 32))) != 0)
  {
    return DFS_FAIL;
  }

  bzero(temp.data, DiskBytesPerBlock());
  
  for(i = 0; i <= ratio - 1; i++) {
    data_index = i * DiskBytesPerBlock();
    bytes_read = DiskReadBlock(blocknum * ratio + i, &temp);
    bcopy(temp.data, &(b->data[data_index]), DiskBytesPerBlock());

    if(bytes_read == DISK_FAIL) {
      return DFS_FAIL;
    }
    
    total_bytes_read += bytes_read;
    bzero(temp.data, DiskBytesPerBlock());
  }

  num_disk_reads += 1;
  return total_bytes_read;
}


//-----------------------------------------------------------------
// DfsWriteBlock writes to an allocated DFS block on the disk
// (which could span multiple physical disk blocks).  The block
// must be allocated in order to write to it.  Returns DFS_FAIL
// on failure, and the number of bytes written on success.  
//-----------------------------------------------------------------
// int DfsWriteBlock(int blocknum, dfs_block *b){
//   // dfs_block new_dfs_block;
//   int fbv_idx = blocknum / 32;
//   int cache_handle;
//   int total_write;
//   double latency_time;
//   int miss_latency_int;
//   double hit_rate;
//   double miss_rate;
//   int i;
//   cache_block *c=NULL;
//   int cache_handle_temp;

//   if(sb.valid == 0)
//   {
//     return DFS_FAIL;
//   }

//   if(fbv_idx < 0 || fbv_idx > DFS_FBV_MAX_NUM_WORDS - 1)
//   {
//     return DFS_FAIL;
//   }

//   if((fbv[fbv_idx] & ((0x1) << (blocknum % 32))) != 0)
//   {
//     return DFS_FAIL;
//   }

//   if(LockHandleAcquire(cache_lock) == SYNC_FAIL)
//   {
//     return DFS_FAIL;
//   }

//   cache_handle = DfsCacheHit(blocknum);
  
//   if(cache_handle != DFS_FAIL)
//   {
//     bcopy(b->data, cache[cache_handle].data, sb.fs_blocksize);
//     cache[cache_handle].dirty = 1;
//     cache[cache_handle].timestamp = ClkGetCurTime();
//     AQueueRemove(&(cache[cache_handle].l));

//     if(LockHandleRelease(cache_lock) == SYNC_FAIL)
//     {
//       return DFS_FAIL;
//     } 

//     if(not_translation != 0)
//     {
//       last_blocknum_write = blocknum;
//     }
//     return sb.fs_blocksize;
//   }
  
//   latency_time = ClkGetCurTime();
//   cache_handle = DfsCacheAllocateSlot(blocknum);

//   if(cache_handle == DFS_FAIL)
//   {
//     LockHandleRelease(cache_lock);
//     return DFS_FAIL;
//   }

//   // total_read = DfsReadBlockUncached(blocknum, &new_dfs_block);

//   // if(total_read == DFS_FAIL)
//   // {
//   //   LockHandleRelease(cache_lock);
//   //   return DFS_FAIL;
//   // }

//   // bcopy(new_dfs_block.data, cache[cache_handle].data, sb.fs_blocksize);
//   // cache[cache_handle].valid = 1;
//   // cache[cache_handle].dirty = 1;
//   // cache[cache_handle].blocknum = blocknum;
//   // bcopy(b->data, cache[cache_handle].data, sb.fs_blocksize);
//   // total_write = sb.fs_blocksize;
//   // cache[cache_handle].timestamp = ClkGetCurTime();

//   // Added for bulk write ////////////////////////////////////////////
//   if(not_translation != 0)
//   {
//     if(blocknum == last_blocknum_write + 1)
//     {
//       cur_wnd_write = ((cur_wnd_write * 2) > MAX_WND) ? MAX_WND : (cur_wnd_write * 2);
//     }
//     else 
//     {
//       cur_wnd_write = DEF_WND;
//       while (!AQueueEmpty(&prefetch_queue_write)) {
//         c = (cache_block *)AQueueObject(AQueueFirst(&prefetch_queue_write));
//         c->valid = 0;
//         c->dirty = 0;
//         if (AQueueRemove(&(c->l)) != QUEUE_SUCCESS)
//         {
//           return DFS_FAIL;
//         }
//       }
//     }

//     for(i = blocknum + 1; i <= blocknum + cur_wnd_write - 1; i++)
//     {
//       cache_handle_temp = DfsCacheAllocateSlot(i);

//       if(cache_handle_temp == DFS_FAIL)
//       {
//         LockHandleRelease(cache_lock);
//         return DFS_FAIL;
//       }
      
//       cache[cache_handle_temp].valid = 1;
//       cache[cache_handle_temp].dirty = 0;
//       cache[cache_handle_temp].blocknum = i;
//       cache[cache_handle_temp].timestamp = latency_time;

//       if ((cache[cache_handle_temp].l = AQueueAllocLink(&cache[cache_handle_temp])) == NULL)
//       {
//         return DFS_FAIL;
//       }
//       if (AQueueInsertLast(&prefetch_queue_write, cache[cache_handle_temp].l) != QUEUE_SUCCESS)
//       {
//         return DFS_FAIL;
//       }
//     }
//   }
//   /////////////////////////////////////////////////////////////////
//   miss_latency = (miss_latency * (num_total_accesses - num_hits - 1) + (cache[cache_handle].timestamp - latency_time)) / (num_total_accesses - num_hits);
//   miss_latency_int = (int)miss_latency;
//   hit_rate = (double)num_hits / (double)num_total_accesses * 100;
//   miss_rate = 100.0 - hit_rate;
//   printf("Cache Miss: Hit Rate = %.3f%%, Miss Rate = %.3f%%, ", hit_rate, miss_rate);
//   printf("Disk Reads = %d, Disk Writes = %d, Miss Handling Latency = %dms\n", num_disk_reads, num_disk_writes, miss_latency_int);
  
//   if(LockHandleRelease(cache_lock) == SYNC_FAIL)
//   {
//     return DFS_FAIL;
//   }

//   if(not_translation != 0)
//   {
//     last_blocknum_write = blocknum;
//   }
//   return total_write;
// }

int DfsWriteBlock(int blocknum, dfs_block *b){
  int i;
  int ratio = sb.fs_blocksize / DiskBytesPerBlock();
  int data_index;
  disk_block temp;
  int fbv_idx = blocknum / 32;
  int total_bytes_written = 0;
  int bytes_written;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  if(fbv_idx < 0 || fbv_idx > DFS_FBV_MAX_NUM_WORDS - 1)
  {
    return DFS_FAIL;
  }

  if((fbv[fbv_idx] & ((0x1) << (blocknum % 32))) != 0)
  {
    return DFS_FAIL;
  }

  bzero(temp.data, DiskBytesPerBlock());
  
  for(i = 0; i <= ratio - 1; i++) {
    data_index = i * DiskBytesPerBlock();
    bcopy(&(b->data[data_index]), temp.data, DiskBytesPerBlock());
    bytes_written = DiskWriteBlock(blocknum * ratio + i, &temp);

    if(bytes_written == DISK_FAIL) {
      return DFS_FAIL;
    }

    total_bytes_written += bytes_written;
    bzero(temp.data, DiskBytesPerBlock());
  }

  num_disk_writes += 1;
  return total_bytes_written;
}


////////////////////////////////////////////////////////////////////////////////
// Inode-based functions
////////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------
// DfsInodeFilenameExists looks through all the inuse inodes for 
// the given filename. If the filename is found, return the handle 
// of the inode. If it is not found, return DFS_FAIL.
//-----------------------------------------------------------------

int DfsInodeFilenameExists(char *filename) {
  int i;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  for(i = 0; i < DFS_INODE_MAX_NUM; i++)
  {
    if(inodes[i].inuse == 1 && dstrncmp(inodes[i].filename, filename, DFS_MAX_FILENAME_LENGTH) == 0)
    {
      return i;
    }
  }

  return DFS_FAIL;
}

int DfsInodeRename(char *oldname, char *newname) {
  int handle;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }
  
  if(DfsInodeFilenameExists(newname) != DFS_FAIL)
  {
    return DFS_FAIL;
  }

  handle = DfsInodeFilenameExists(oldname);

  if(handle == DFS_FAIL)
  {
    return DFS_FAIL;
  }

  bzero(inodes[handle].filename, 71);
  dstrncpy(inodes[handle].filename, newname, dstrlen(newname));
  return DFS_SUCCESS;
}
//-----------------------------------------------------------------
// DfsInodeOpen: search the list of all inuse inodes for the 
// specified filename. If the filename exists, return the handle 
// of the inode. If it does not, allocate a new inode for this 
// filename and return its handle. Return DFS_FAIL on failure. 
// Remember to use locks whenever you allocate a new inode.
//-----------------------------------------------------------------

int DfsInodeOpen(char *filename) {
    int inode_handle;
    int i;

    if(sb.valid == 0)
    {
        return DFS_FAIL;
    }

    inode_handle = DfsInodeFilenameExists(filename);

    if(inode_handle != DFS_FAIL)
    {
      return inode_handle;
    }

    if(LockHandleAcquire(inode_lock) == SYNC_FAIL)
    {
      return DFS_FAIL;
    }

    for(i = 0; i < DFS_INODE_MAX_NUM; i++)
    {
      if(inodes[i].inuse == 0)
      {
        inodes[i].inuse = 1;

        if(LockHandleRelease(inode_lock) == SYNC_FAIL)
        {
          inodes[i].inuse = 0;
          return DFS_FAIL;
        }

        inodes[i].filesize = 0;
        dstrncpy(inodes[i].filename, filename, dstrlen(filename));
        bzero((char *)inodes[i].direct_table, sizeof(inodes[i].direct_table));
        inodes[i].indirect_block = 0;
        inodes[i].double_indirect_block = 0;

        return i;
      }
    }

    LockHandleRelease(inode_lock);
    return DFS_FAIL;
}


//-----------------------------------------------------------------
// DfsInodeDelete de-allocates any data blocks used by this inode, 
// including the indirect addressing block if necessary, then mark 
// the inode as no longer in use. Use locks when modifying the 
// "inuse" flag in an inode.Return DFS_FAIL on failure, and 
// DFS_SUCCESS on success.
//-----------------------------------------------------------------

int DfsInodeDelete(int handle) {
    int i, j;
    dfs_block temp1, temp2;
    uint32 * data_temp1, * data_temp2;
    int dfs_block_size;

    if(sb.valid == 0)
    {
      return DFS_FAIL;
    }

    if(handle < 0 || handle > DFS_INODE_MAX_NUM - 1)
    {
      return DFS_FAIL;
    }

    if(LockHandleAcquire(inode_lock) == SYNC_FAIL)
    {
      return DFS_FAIL;
    }

    inodes[handle].filesize = 0;
    bzero(inodes[handle].filename, DFS_MAX_FILENAME_LENGTH);

    for(i = 0; i < DFS_MAX_DIR_TABLE_SIZE; i++)
    {
      if(inodes[handle].direct_table[i] != 0)
      {
        if(DfsFreeBlock(inodes[handle].direct_table[i]) == DFS_FAIL)
        {
          LockHandleRelease(inode_lock);
          return DFS_FAIL;
        }
      }
    }
    
    bzero((char *)(inodes[i].direct_table), sizeof(inodes[i].direct_table));

    if(inodes[handle].indirect_block != 0)
    {
      not_translation = 0;
      dfs_block_size = DfsReadBlock(inodes[handle].indirect_block, &temp1);
      not_translation = 1;
      if(dfs_block_size == DFS_FAIL)
      {
        LockHandleRelease(inode_lock);
        return DFS_FAIL;
      }

      data_temp1 = (uint32*)temp1.data;

      for(i = 0; i <= 9; i++)
      {
        if(data_temp1[i] != 0)
        {
          if(DfsFreeBlock(data_temp1[i]) == DFS_FAIL)
          {
            LockHandleRelease(inode_lock);
            return DFS_FAIL;
          }
        }
      }

      if(DfsFreeBlock(inodes[handle].indirect_block) == DFS_FAIL)
      {
        LockHandleRelease(inode_lock);
        return DFS_FAIL;
      }

      inodes[handle].indirect_block = 0;
    }

    if(inodes[handle].double_indirect_block != 0)
    {
      not_translation = 0;
      dfs_block_size = DfsReadBlock(inodes[handle].double_indirect_block, &temp1);
      not_translation = 1;
    
      if(dfs_block_size == DFS_FAIL)
      {
        LockHandleRelease(inode_lock);
        return DFS_FAIL;
      }

      data_temp1 = (uint32*)temp1.data;

      for(i = 0; i <= 9; i++)
      {
        if(data_temp1[i] != 0)
        {
          not_translation = 0;
          dfs_block_size = DfsReadBlock(data_temp1[i], &temp2);
          not_translation = 1;
          
          if(dfs_block_size == DFS_FAIL)
          {
            LockHandleRelease(inode_lock);
            return DFS_FAIL;
          }
          
          data_temp2 = (uint32*)temp2.data;

          for(j = 0; j <= 9; j++)
          {
            if(data_temp2[j] != 0)
            {
              if(DfsFreeBlock(data_temp2[j]) == DFS_FAIL)
              {
                LockHandleRelease(inode_lock);
                return DFS_FAIL;
              }
            }
          }

          if(DfsFreeBlock(data_temp1[i]) == DFS_FAIL)
          {
            LockHandleRelease(inode_lock);
            return DFS_FAIL;
          }
        }
      }
    
      if(DfsFreeBlock(inodes[handle].double_indirect_block) == DFS_FAIL)
      {
        LockHandleRelease(inode_lock);
        return DFS_FAIL;
      }
      
      inodes[handle].double_indirect_block = 0;
    }
    
    inodes[i].inuse = 0;

    if(LockHandleRelease(inode_lock) == SYNC_FAIL)
    {
      return DFS_FAIL;
    }

    return DFS_SUCCESS;
}


//-----------------------------------------------------------------
// DfsInodeReadBytes reads num_bytes from the file represented by 
// the inode handle, starting at virtual byte start_byte, copying 
// the data to the address pointed to by mem. Return DFS_FAIL on 
// failure, and the number of bytes read on success.
//-----------------------------------------------------------------

int DfsInodeReadBytes(int handle, void *mem, int start_byte, int num_bytes) {
    int first_block, last_block, end_byte;
    int total_bytes_read = 0, bytes_read;
    int phy_block, i, offset;
    dfs_block temp;

    if(sb.valid == 0)
    {
       return DFS_FAIL;
    }

    if(handle < 0 || handle > DFS_INODE_MAX_NUM - 1)
    {
      return DFS_FAIL;
    }

    if(start_byte < 0 || num_bytes < 0 || start_byte > inodes[handle].filesize || inodes[handle].inuse == 0)
    {
      return DFS_FAIL;
    }

    end_byte = ((start_byte + num_bytes - 1) > inodes[handle].filesize) ? inodes[handle].filesize : (start_byte + num_bytes - 1);
    first_block = start_byte / sb.fs_blocksize;
    i = first_block;
    last_block = end_byte / sb.fs_blocksize;

    while(i <= last_block)
    {
      phy_block = DfsInodeTranslateVirtualToFilesys(handle, i);
      if(phy_block == DFS_FAIL)
      {
        return DFS_FAIL;
      }
      bytes_read = DfsReadBlock(phy_block, &temp);
      if(bytes_read == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      if(first_block == last_block)
      {
        bytes_read = end_byte - start_byte + 1;
        offset = start_byte % sb.fs_blocksize;
      }
      else if(i == first_block)
      {
        offset = start_byte % sb.fs_blocksize;
        bytes_read = sb.fs_blocksize - offset;
      }
      else if(i == last_block)
      {
        offset = 0;
        bytes_read = (end_byte % sb.fs_blocksize) + 1;
      }
      else
      {
        offset = 0;
        bytes_read = sb.fs_blocksize;
      }

      bcopy(temp.data + offset, (char *)mem + total_bytes_read, bytes_read);
      total_bytes_read = total_bytes_read + bytes_read;
      i = i + 1;
    }

    return total_bytes_read;
}


//-----------------------------------------------------------------
// DfsInodeWriteBytes writes num_bytes from the memory pointed to 
// by mem to the file represented by the inode handle, starting at 
// virtual byte start_byte. Note that if you are only writing part 
// of a given file system block, you'll need to read that block 
// from the disk first. Return DFS_FAIL on failure and the number 
// of bytes written on success.
//-----------------------------------------------------------------

int DfsInodeWriteBytes(int handle, void *mem, int start_byte, int num_bytes) {
    int bytes_written, total_bytes_written = 0;
    int first_block, last_block, end_byte;
    int phy_block, offset;
    dfs_block temp;
    int i;

    if(sb.valid == 0)
    {
      return DFS_FAIL;
    }

    if(handle < 0 || handle > DFS_INODE_MAX_NUM - 1 || inodes[handle].inuse == 0)
    {
      return DFS_FAIL;
    }

    if(start_byte < 0 || num_bytes < 0)
    {
      return DFS_FAIL;
    }

    end_byte = start_byte + num_bytes - 1;
    first_block = start_byte / sb.fs_blocksize;
    i = first_block;
    last_block = end_byte / sb.fs_blocksize;

    for(i = 0; i <= first_block - 1; i++)
    {
      phy_block = DfsInodeTranslateVirtualToFilesys(handle, i);
      
      if(phy_block == DFS_FAIL)
      {
        phy_block = DfsInodeAllocateVirtualBlock(handle, i);

        if(phy_block == DFS_FAIL)
        {
          return DFS_FAIL;
        }

        bzero(temp.data, sb.fs_blocksize);

        if(DfsWriteBlock(phy_block, &temp) == DFS_FAIL)
        {
          return DFS_FAIL;
        }
      }
    }
    
    while(i <= last_block)
    {
      phy_block = DfsInodeTranslateVirtualToFilesys(handle, i);
      
      if(phy_block == DFS_FAIL)
      {
        phy_block = DfsInodeAllocateVirtualBlock(handle, i);

        if(phy_block == DFS_FAIL)
        {
          return DFS_FAIL;
        }

        bzero(temp.data, sb.fs_blocksize);

        if(DfsWriteBlock(phy_block, &temp) == DFS_FAIL)
        {
          return DFS_FAIL;
        }
      }

      bytes_written = DfsReadBlock(phy_block, &temp);
      
      if(bytes_written == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      if(first_block == last_block)
      {
        bytes_written = end_byte - start_byte + 1;
        offset = start_byte % sb.fs_blocksize;
      }
      else if(i == first_block)
      {
        offset = start_byte % sb.fs_blocksize;
        bytes_written = sb.fs_blocksize - offset;
      }
      else if(i == last_block)
      {
        offset = 0;
        bytes_written = (end_byte % sb.fs_blocksize) + 1;
      }
      else
      {
        offset = 0;
        bytes_written = sb.fs_blocksize;
      }

      bcopy((char *)mem + total_bytes_written, temp.data + offset, bytes_written);
      
      if(DfsWriteBlock(phy_block, &temp) == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      total_bytes_written = total_bytes_written + bytes_written;
      i = i + 1;
    }

    if((start_byte + total_bytes_written) > inodes[handle].filesize)
    {
      inodes[handle].filesize += (start_byte + total_bytes_written - inodes[handle].filesize);
    }
    
    return total_bytes_written;
}


//-----------------------------------------------------------------
// DfsInodeFilesize simply returns the size of an inode's file. 
// This is defined as the maximum virtual byte number that has 
// been written to the inode thus far. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

int DfsInodeFilesize(int handle) {
    if(sb.valid == 0)
    {
      return DFS_FAIL;
    }

    if(handle < 0 || handle >= DFS_INODE_MAX_NUM || inodes[handle].inuse == 0)
    {
      return DFS_FAIL;
    }

    return inodes[handle].filesize;
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

int DfsInodeAllocateVirtualBlock(int handle, int virtual_blocknum) {
  int physical_block_num;
  dfs_block temp1, temp2;
  uint32 * data_temp1, * data_temp2;
  int check_num, retval;
  int indirect_idx, double_indirect_idx;
  int indirect_table_size = sb.fs_blocksize/sizeof(uint32);

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  if(handle < 0 || handle > DFS_INODE_MAX_NUM - 1 || inodes[handle].inuse == 0)
  {
    return DFS_FAIL;
  }

  if(virtual_blocknum < 0 || virtual_blocknum > (DFS_MAX_DIR_TABLE_SIZE + indirect_table_size + indirect_table_size * indirect_table_size - 1))
  {
    return DFS_FAIL;
  }

  physical_block_num = DfsAllocateBlock();

  if(physical_block_num == DFS_FAIL)
  {
    return DFS_FAIL;
  }

  if(virtual_blocknum < DFS_MAX_DIR_TABLE_SIZE)
  {
    inodes[handle].direct_table[virtual_blocknum] = physical_block_num;
  }
  else if(virtual_blocknum < DFS_MAX_DIR_TABLE_SIZE + indirect_table_size)
  {
    if(inodes[handle].indirect_block == 0)
    {
      inodes[handle].indirect_block = DfsAllocateBlock();

      if(inodes[handle].indirect_block == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      bzero(temp1.data, sb.fs_blocksize);

      not_translation = 0;
      if(DfsWriteBlock(inodes[handle].indirect_block, &temp1) == DFS_FAIL)
      {
        not_translation = 1;
        return DFS_FAIL;
      }
      not_translation = 1;
    }

    not_translation = 0;
    retval = DfsReadBlock(inodes[handle].indirect_block, &temp1);
    not_translation = 1;
    
    if(retval == DFS_FAIL)
    {
      return DFS_FAIL;
    }

    data_temp1 = (uint32*)temp1.data;
    indirect_idx = virtual_blocknum - DFS_MAX_DIR_TABLE_SIZE;
    data_temp1[indirect_idx] = physical_block_num;
    
    not_translation = 0;
    if(DfsWriteBlock(inodes[handle].indirect_block, &temp1) == DFS_FAIL)
    {
      not_translation = 1;
      return DFS_FAIL;
    }
    not_translation = 1;
  }
  else
  {
    if(inodes[handle].double_indirect_block == 0)
    {
      inodes[handle].double_indirect_block = DfsAllocateBlock();

      if(inodes[handle].double_indirect_block == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      bzero(temp1.data, sb.fs_blocksize);

      not_translation = 0;
      if(DfsWriteBlock(inodes[handle].double_indirect_block, &temp1) == DFS_FAIL)
      {
        not_translation = 1;
        return DFS_FAIL;
      }
      not_translation = 1;
    }

    not_translation = 0;
    retval = DfsReadBlock(inodes[handle].double_indirect_block, &temp1);
    not_translation = 1;
    
    if(retval == DFS_FAIL)
    {
      return DFS_FAIL;
    }

    data_temp1 = (uint32*)temp1.data;
    indirect_idx = (virtual_blocknum - DFS_MAX_DIR_TABLE_SIZE - indirect_table_size) / indirect_table_size;

    if(data_temp1[indirect_idx] == 0)
    {
      check_num = DfsAllocateBlock();

      if(check_num == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      data_temp1[indirect_idx] = (uint32)check_num;
      bzero(temp2.data, sb.fs_blocksize);
      
      not_translation = 0;
      if(DfsWriteBlock(data_temp1[indirect_idx], &temp2) == DFS_FAIL)
      {
        not_translation = 1;
        return DFS_FAIL;
      }
      not_translation = 1;
    }

    not_translation = 0;
    if(DfsWriteBlock(inodes[handle].double_indirect_block, &temp1) == DFS_FAIL)
    {
      not_translation = 1;
      return DFS_FAIL;
    }
    not_translation = 1;

    not_translation = 0;
    retval = DfsReadBlock(data_temp1[indirect_idx], &temp2);
    not_translation = 1;
    if(retval == DFS_FAIL)
    {
      return DFS_FAIL;
    }

    data_temp2 = (uint32*)temp2.data;
    double_indirect_idx = (virtual_blocknum - DFS_MAX_DIR_TABLE_SIZE - indirect_table_size) % indirect_table_size;
    data_temp2[double_indirect_idx] = physical_block_num;

    not_translation = 0;
    if(DfsWriteBlock(data_temp1[indirect_idx], &temp2) == DFS_FAIL)
    {
      not_translation = 1;
      return DFS_FAIL;
    }
    not_translation = 1;
  }

  return physical_block_num;
}


//-----------------------------------------------------------------
// DfsInodeTranslateVirtualToFilesys translates the 
// virtual_blocknum to the corresponding file system block using 
// the inode identified by handle. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

int DfsInodeTranslateVirtualToFilesys(int handle, int virtual_blocknum) {
  dfs_block temp1, temp2;
  uint32 * data_temp1, * data_temp2;
  int retval;
  int indirect_idx, double_indirect_idx;
  int indirect_table_size = sb.fs_blocksize/sizeof(uint32);
  int total_virtual_blocks;
  uint32 double_indirect_table_size = indirect_table_size*indirect_table_size;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  if(handle < 0 || handle > DFS_INODE_MAX_NUM - 1 || inodes[handle].inuse == 0)
  {
    return DFS_FAIL;
  }

  total_virtual_blocks = 1 + ((inodes[handle].filesize - 1) / sb.fs_blocksize);
  if(virtual_blocknum < 0 || virtual_blocknum > (DFS_MAX_DIR_TABLE_SIZE + indirect_table_size + double_indirect_table_size - 1) || virtual_blocknum >= total_virtual_blocks)
  {
    return DFS_FAIL;
  }

  if(virtual_blocknum < DFS_MAX_DIR_TABLE_SIZE)
  {
    if(inodes[handle].direct_table[virtual_blocknum] != 0)
    {
      return inodes[handle].direct_table[virtual_blocknum];
    }
    else
    {
      return DFS_FAIL;
    }
  }
  else if(virtual_blocknum < (DFS_MAX_DIR_TABLE_SIZE + indirect_table_size))
  {
    if(inodes[handle].indirect_block != 0)
    {
      not_translation = 0;
      retval = DfsReadBlock(inodes[handle].indirect_block, &temp1);
      not_translation = 1;
    
      if(retval == DFS_FAIL)
      {
        return DFS_FAIL;
      }
    }
    else
    {
      return DFS_FAIL;
    }

    data_temp1 = (uint32*)temp1.data;
    indirect_idx = virtual_blocknum - DFS_MAX_DIR_TABLE_SIZE;

    if(data_temp1[indirect_idx] != 0)
    {
      return data_temp1[indirect_idx];
    }
    else
    {
      return DFS_FAIL;
    }
  }
  else
  {
    if(inodes[handle].double_indirect_block != 0)
    {
      not_translation = 0;
      retval = DfsReadBlock(inodes[handle].double_indirect_block, &temp1);
      not_translation = 1;
    
      if(retval == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      data_temp1 = (uint32*)temp1.data;
      indirect_idx = (virtual_blocknum - DFS_MAX_DIR_TABLE_SIZE - indirect_table_size) / indirect_table_size;

      if(data_temp1[indirect_idx] != 0)
      {
        not_translation = 0;
        retval = DfsReadBlock(data_temp1[indirect_idx], &temp2);
        not_translation = 1;

        if(retval == DFS_FAIL)
        {
          return DFS_FAIL;
        }

        data_temp2 = (uint32*)temp2.data;
        double_indirect_idx = (virtual_blocknum - DFS_MAX_DIR_TABLE_SIZE - indirect_table_size) % indirect_table_size;

        if(data_temp2[double_indirect_idx] != 0)
        {
          return data_temp2[double_indirect_idx];
        }
        else
        {
          return DFS_FAIL;
        }
      }
      else
      {
        return DFS_FAIL;
      }
    }
    else
    {
      return DFS_FAIL;
    }
  }
  
  return DFS_FAIL;
}

int DfsCacheHit(int blocknum) {
  int i;
  int index = blocknum & CACHE_IDX_MASK;

  num_total_accesses += 1;

  if(LockHandleAcquire(cache_lock) != SYNC_SUCCESS)
  {
    return DFS_FAIL;
  }
  
  for(i = index; i < (index + CACHE_WAYS); i++)
  {
    if((cache[i].valid == 1) && (cache[i].blocknum == blocknum))
    {
      num_hits += 1;
      if(LockHandleRelease(cache_lock) != SYNC_SUCCESS)
      {
        num_hits -= 1;
        return DFS_FAIL;
      }
      return i;
    }
  }
  LockHandleRelease(cache_lock);
  return DFS_FAIL;
}


int DfsCacheAllocateSlot(int blocknum) {
  int i;
  int cache_line = -1;
  dfs_block temp;
  int index = blocknum & CACHE_IDX_MASK;

  if(LockHandleAcquire(cache_lock) != SYNC_SUCCESS)
  {
    return DFS_FAIL;
  }
  
  for(i = index; i < (index + CACHE_WAYS); i++)
  {
    if(cache[i].valid == 0)
    {
        if(LockHandleRelease(cache_lock) != SYNC_SUCCESS)
        {
            return DFS_FAIL;
        }
        return i;
    }
  }

  cache_line = DfsCacheReplacementPolicy(blocknum);
  if(cache_line == DFS_FAIL)
  {
    LockHandleRelease(cache_lock);
    return DFS_FAIL;
  }

  if((cache[cache_line].valid == 1) && (cache[cache_line].dirty == 1))
  {
    bcopy(cache[cache_line].data, temp.data, sb.fs_blocksize);
    if(DfsWriteBlock(cache[cache_line].blocknum, &temp) == DFS_FAIL)
    {
      LockHandleRelease(cache_lock);
      return DFS_FAIL;
    }
    cache[cache_line].dirty = 0;
  }

  if(LockHandleRelease(cache_lock) != SYNC_SUCCESS)
  {
    return DFS_FAIL;
  }

  return cache_line;
}

int DfsCacheFlush() {
  int i;
  dfs_block temp;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  if(LockHandleAcquire(cache_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  for(i = 0; i <= CACHE_SIZE - 1; i++)
  {
    if(cache[i].valid == 1 && cache[i].dirty == 1)
    {
      bcopy(cache[i].data, temp.data, sb.fs_blocksize);
      
      if(DfsWriteBlock(cache[i].blocknum, &temp) == DFS_FAIL)
      {
        LockHandleRelease(cache_lock);
        return DFS_FAIL;
      }

      cache[i].dirty = 0;
    }
  }

  if(LockHandleRelease(cache_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  return DFS_SUCCESS;
}


int DfsCacheReplacementPolicy(int blocknum) {
  //LRU
  int i;
  int index = blocknum & CACHE_IDX_MASK;
  int cache_line = 0;
  double min_time = ClkGetCurTime();

  for(i = index; i < (index + CACHE_WAYS); i++)
  {
    if(cache[i].timestamp < min_time)
    {
      min_time = cache[i].timestamp;
      cache_line = i;
    }
  }

  return cache_line;
}
