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
static cache_block caches[CACHE_SIZE]; // cache blocks

////////////////////////////////////////////

static int num_disk_reads;
static int num_disk_writes;
static int num_hit;
static int num_total_access;
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
  num_hit = 0;
  num_total_access = 0;
  miss_latency = 0.0;

  for(i = 0; i < CACHE_SIZE; i++)
  {
    caches[i].valid = 0;
    caches[i].dirty = 0;
    caches[i].timestamp = 0;
    caches[i].blocknum = 0; 
    bzero(caches[i].data, DFS_BLOCKSIZE);
    caches[i].l = NULL;
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
  int disk_block_size;
  int dfs_block_size;
  disk_block new_disk_block;
  dfs_block new_dfs_block;
  int i;
  int fbv_block_num;

  if(sb.valid == 1)
  {
    return DFS_FAIL;
  }

  disk_block_size = DiskReadBlock(1 * (DFS_BLOCKSIZE / DiskBytesPerBlock()), &new_disk_block);

  if(disk_block_size == DISK_FAIL)
  {
    return DFS_FAIL;
  }

  bcopy(new_disk_block.data, (char *)&sb, sizeof(dfs_superblock));

  for(i = sb.start_block_inode; i <= sb.start_block_fbv - 1; i++)
  {
    dfs_block_size = DfsReadBlockUncached(i, &new_dfs_block);

    if(dfs_block_size == DFS_FAIL)
    {
        return DFS_FAIL;
    }

    bcopy(new_dfs_block.data, (char *)(inodes + ((i - sb.start_block_inode) * (sb.block_size / 128))), sb.block_size);
  }

  fbv_block_num = (sb.num_block / 8) / sb.block_size;

  for(i = sb.start_block_fbv; i <= sb.start_block_fbv + fbv_block_num - 1; i++)
  {
    dfs_block_size = DfsReadBlockUncached(i, &new_dfs_block);
    
    if(dfs_block_size == DFS_FAIL)
    {
        return DFS_FAIL;
    }

    bcopy(new_dfs_block.data, (char *)(fbv + ((i - sb.start_block_fbv) * (sb.block_size / 4))), sb.block_size);
  }

  DfsInvalidate();

  bzero(new_disk_block.data, DiskBytesPerBlock());
  bcopy((char *)&sb, new_disk_block.data, sizeof(dfs_superblock));
  disk_block_size = DiskWriteBlock(1 * (sb.block_size / DiskBytesPerBlock()), &new_disk_block);

  if(disk_block_size == DISK_FAIL)
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
  int disk_block_size;
  int dfs_block_size;
  disk_block new_disk_block;
  dfs_block new_dfs_block;
  int i;
  int fbv_block_num;
    
  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  for(i = sb.start_block_inode; i <= sb.start_block_fbv - 1; i++)
  {
    bcopy((char *)(inodes + ((i - sb.start_block_inode) * (sb.block_size / 128))), new_dfs_block.data, sb.block_size);
    dfs_block_size = DfsWriteBlockUncached(i, &new_dfs_block);

    if(dfs_block_size == DFS_FAIL)
    {
      return DFS_FAIL;
    }
  }

  fbv_block_num = (sb.num_block / 8) / sb.block_size;
  
  for(i = sb.start_block_fbv; i <= sb.start_block_fbv + fbv_block_num - 1; i++)
  {
    bcopy((char *)(fbv + ((i - sb.start_block_fbv) * (sb.block_size / 4))), new_dfs_block.data, sb.block_size);
    dfs_block_size = DfsWriteBlockUncached(i, &new_dfs_block);
    
    if(dfs_block_size == DFS_FAIL)
    {
      return DFS_FAIL;
    }
  }
  
  bzero(new_disk_block.data, DiskBytesPerBlock());
  bcopy((char *)&sb, new_disk_block.data, sizeof(dfs_superblock));
  disk_block_size = DiskWriteBlock(1 * (sb.block_size / DiskBytesPerBlock()), &new_disk_block);

  if(disk_block_size == DISK_FAIL)
  {
    return DFS_FAIL;
  }

  disk_block_size = DiskWriteBlock(65535 * (sb.block_size / DiskBytesPerBlock()), &new_disk_block);

  if(disk_block_size == DISK_FAIL)
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
  uint32 fbv_temp;

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
          fbv_temp = fbv[i];
          fbv[i] = fbv[i] & invert(0x1 << j);

          if(LockHandleRelease(fbv_lock) == SYNC_FAIL)
          {
            fbv[i] = fbv_temp;
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
    if(caches[idx].valid == 1 && caches[idx].blocknum == blocknum)
    {
      caches[idx].valid = 0;
      caches[idx].dirty = 0;
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
int DfsReadBlock(int blocknum, dfs_block *b) {
  int fbv_idx = blocknum / 32;
  int cache_handle;
  int total_read;
  double latency_time;
  int miss_latency_int;
  double hit_rate;
  double miss_rate;
  int i;
  cache_block *c=NULL;
  dfs_block *temp;
  int cache_handle_temp;

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

  if(LockHandleAcquire(cache_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  cache_handle = DfsCacheHit(blocknum);
  
  if(cache_handle != DFS_FAIL)
  {
    bcopy(caches[cache_handle].data, b->data, sb.block_size);
    caches[cache_handle].timestamp = ClkGetCurTime();
    AQueueRemove(&(caches[cache_handle].l));

    if(LockHandleRelease(cache_lock) == SYNC_FAIL)
    {
      return DFS_FAIL;
    } 

    if(not_translation != 0)
    {
      last_blocknum_read = blocknum;
    }
    return sb.block_size;
  }
  
  latency_time = ClkGetCurTime();

  total_read = DfsReadBlockUncached(blocknum, b);

  if(total_read == DFS_FAIL)
  {
    LockHandleRelease(cache_lock);
    return DFS_FAIL;
  }

  cache_handle = DfsCacheAllocateSlot(blocknum);

  if(cache_handle == DFS_FAIL)
  {
    LockHandleRelease(cache_lock);
    return DFS_FAIL;
  }
  
  caches[cache_handle].valid = 1;
  caches[cache_handle].dirty = 0;
  caches[cache_handle].blocknum = blocknum;
  bcopy(b->data, caches[cache_handle].data, sb.block_size);
  caches[cache_handle].timestamp = ClkGetCurTime();

  // Added for bulk read ////////////////////////////////////////////
  if(not_translation != 0)
  {
    if(blocknum == last_blocknum_read + 1)
    {
      cur_wnd_read = ((cur_wnd_read * 2) > MAX_WND) ? MAX_WND : (cur_wnd_read * 2);
    }
    else 
    {
      cur_wnd_read = DEF_WND;
      while (!AQueueEmpty(&prefetch_queue_read)) {
        c = (cache_block *)AQueueObject(AQueueFirst(&prefetch_queue_read));
        c->valid = 0;
        c->dirty = 0;
        if (AQueueRemove(&(c->l)) != QUEUE_SUCCESS)
        {
          return DFS_FAIL;
        }
      }
    }

    for(i = blocknum + 1; i <= blocknum + cur_wnd_read - 1; i++)
    {
      if(DfsReadBlockUncached(i, temp) == DFS_FAIL)
      {
        LockHandleRelease(cache_lock);
        return DFS_FAIL;
      }

      cache_handle_temp = DfsCacheAllocateSlot(i);

      if(cache_handle_temp == DFS_FAIL)
      {
        LockHandleRelease(cache_lock);
        return DFS_FAIL;
      }
      
      caches[cache_handle_temp].valid = 1;
      caches[cache_handle_temp].dirty = 0;
      caches[cache_handle_temp].blocknum = i;
      bcopy(temp->data, caches[cache_handle_temp].data, sb.block_size);
      caches[cache_handle_temp].timestamp = latency_time;

      if ((caches[cache_handle_temp].l = AQueueAllocLink(&caches[cache_handle_temp])) == NULL)
      {
        return DFS_FAIL;
      }
      if (AQueueInsertLast(&prefetch_queue_read, caches[cache_handle_temp].l) != QUEUE_SUCCESS)
      {
        return DFS_FAIL;
      }
    }
  }
  /////////////////////////////////////////////////////////////////
  miss_latency = miss_latency + ((caches[cache_handle].timestamp - latency_time) - miss_latency) / (num_total_access - num_hit);
  // miss_latency = (miss_latency * (num_total_access - num_hit - 1) + (caches[cache_handle].timestamp - latency_time)) / (num_total_access - num_hit);
  miss_latency_int = (int)miss_latency;
  hit_rate = (double)num_hit / (double)num_total_access * 100;
  miss_rate = 100.0 - hit_rate;
  printf("Cache Miss: Hit Rate = %.3f%%, Miss Rate = %.3f%%, ", hit_rate, miss_rate);
  printf("Disk Reads = %d, Disk Writes = %d, Miss Handling Latency = %dms\n", num_disk_reads, num_disk_writes, miss_latency_int);

  if(LockHandleRelease(cache_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  if(not_translation != 0)
  {
    last_blocknum_read = blocknum;
  }
  return total_read;
}

int DfsReadBlockUncached(int blocknum, dfs_block *b) {
  int i;
  int physical_block_num = sb.block_size / DiskBytesPerBlock();
  int data_index;
  disk_block new_disk_block;
  int fbv_idx = blocknum / 32;
  int total_read = 0;
  int one_read;

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

  bzero(new_disk_block.data, DiskBytesPerBlock());
  
  for(i = 0; i <= physical_block_num - 1; i++) {
    data_index = i * DiskBytesPerBlock();
    one_read = DiskReadBlock(blocknum * physical_block_num + i, &new_disk_block);
    bcopy(new_disk_block.data, &(b->data[data_index]), DiskBytesPerBlock());

    if(one_read == DISK_FAIL) {
      return DFS_FAIL;
    }
    
    total_read += one_read;
    bzero(new_disk_block.data, DiskBytesPerBlock());
  }

  num_disk_reads += 1;
  return total_read;
}


//-----------------------------------------------------------------
// DfsWriteBlock writes to an allocated DFS block on the disk
// (which could span multiple physical disk blocks).  The block
// must be allocated in order to write to it.  Returns DFS_FAIL
// on failure, and the number of bytes written on success.  
//-----------------------------------------------------------------
int DfsWriteBlock(int blocknum, dfs_block *b){
  // dfs_block new_dfs_block;
  int fbv_idx = blocknum / 32;
  int cache_handle;
  int total_write;
  double latency_time;
  int miss_latency_int;
  double hit_rate;
  double miss_rate;
  int i;
  cache_block *c=NULL;
  int cache_handle_temp;

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

  if(LockHandleAcquire(cache_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  cache_handle = DfsCacheHit(blocknum);
  
  if(cache_handle != DFS_FAIL)
  {
    bcopy(b->data, caches[cache_handle].data, sb.block_size);
    caches[cache_handle].dirty = 1;
    caches[cache_handle].timestamp = ClkGetCurTime();
    AQueueRemove(&(caches[cache_handle].l));

    if(LockHandleRelease(cache_lock) == SYNC_FAIL)
    {
      return DFS_FAIL;
    } 

    if(not_translation != 0)
    {
      last_blocknum_write = blocknum;
    }
    return sb.block_size;
  }
  
  latency_time = ClkGetCurTime();
  cache_handle = DfsCacheAllocateSlot(blocknum);

  if(cache_handle == DFS_FAIL)
  {
    LockHandleRelease(cache_lock);
    return DFS_FAIL;
  }

  // total_read = DfsReadBlockUncached(blocknum, &new_dfs_block);

  // if(total_read == DFS_FAIL)
  // {
  //   LockHandleRelease(cache_lock);
  //   return DFS_FAIL;
  // }

  // bcopy(new_dfs_block.data, caches[cache_handle].data, sb.block_size);
  caches[cache_handle].valid = 1;
  caches[cache_handle].dirty = 1;
  caches[cache_handle].blocknum = blocknum;
  bcopy(b->data, caches[cache_handle].data, sb.block_size);
  total_write = sb.block_size;
  caches[cache_handle].timestamp = ClkGetCurTime();

  // Added for bulk write ////////////////////////////////////////////
  if(not_translation != 0)
  {
    if(blocknum == last_blocknum_write + 1)
    {
      cur_wnd_write = ((cur_wnd_write * 2) > MAX_WND) ? MAX_WND : (cur_wnd_write * 2);
    }
    else 
    {
      cur_wnd_write = DEF_WND;
      while (!AQueueEmpty(&prefetch_queue_write)) {
        c = (cache_block *)AQueueObject(AQueueFirst(&prefetch_queue_write));
        c->valid = 0;
        c->dirty = 0;
        if (AQueueRemove(&(c->l)) != QUEUE_SUCCESS)
        {
          return DFS_FAIL;
        }
      }
    }

    for(i = blocknum + 1; i <= blocknum + cur_wnd_write - 1; i++)
    {
      cache_handle_temp = DfsCacheAllocateSlot(i);

      if(cache_handle_temp == DFS_FAIL)
      {
        LockHandleRelease(cache_lock);
        return DFS_FAIL;
      }
      
      caches[cache_handle_temp].valid = 1;
      caches[cache_handle_temp].dirty = 0;
      caches[cache_handle_temp].blocknum = i;
      caches[cache_handle_temp].timestamp = latency_time;

      if ((caches[cache_handle_temp].l = AQueueAllocLink(&caches[cache_handle_temp])) == NULL)
      {
        return DFS_FAIL;
      }
      if (AQueueInsertLast(&prefetch_queue_write, caches[cache_handle_temp].l) != QUEUE_SUCCESS)
      {
        return DFS_FAIL;
      }
    }
  }
  /////////////////////////////////////////////////////////////////
  miss_latency = (miss_latency * (num_total_access - num_hit - 1) + (caches[cache_handle].timestamp - latency_time)) / (num_total_access - num_hit);
  miss_latency_int = (int)miss_latency;
  hit_rate = (double)num_hit / (double)num_total_access * 100;
  miss_rate = 100.0 - hit_rate;
  printf("Cache Miss: Hit Rate = %.3f%%, Miss Rate = %.3f%%, ", hit_rate, miss_rate);
  printf("Disk Reads = %d, Disk Writes = %d, Miss Handling Latency = %dms\n", num_disk_reads, num_disk_writes, miss_latency_int);
  
  if(LockHandleRelease(cache_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  if(not_translation != 0)
  {
    last_blocknum_write = blocknum;
  }
  return total_write;
}

int DfsWriteBlockUncached(int blocknum, dfs_block *b){
  int i;
  int physical_block_num = sb.block_size / DiskBytesPerBlock();
  int data_index;
  disk_block new_disk_block;
  int fbv_idx = blocknum / 32;
  int total_write = 0;
  int one_write;

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

  bzero(new_disk_block.data, DiskBytesPerBlock());
  
  for(i = 0; i <= physical_block_num - 1; i++) {
    data_index = i * DiskBytesPerBlock();
    bcopy(&(b->data[data_index]), new_disk_block.data, DiskBytesPerBlock());
    one_write = DiskWriteBlock(blocknum * physical_block_num + i, &new_disk_block);

    if(one_write == DISK_FAIL) {
      return DFS_FAIL;
    }

    total_write += one_write;
    bzero(new_disk_block.data, DiskBytesPerBlock());
  }

  num_disk_writes += 1;
  return total_write;
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

  for(i = 0; i <= DFS_INODE_MAX_NUM - 1; i++)
  {
    if(inodes[i].valid == 1 && dstrncmp(inodes[i].filename, filename, 71) == 0)
    {
      return i;
    }
  }

  return DFS_FAIL;
}

int DfsInodeRename(char *oldname, char *newname) {
  int old_inode;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }
  
  if(DfsInodeFilenameExists(newname) != DFS_FAIL)
  {
    return DFS_FAIL;
  }

  old_inode = DfsInodeFilenameExists(oldname);

  if(old_inode == DFS_FAIL)
  {
    return DFS_FAIL;
  }

  bzero(inodes[old_inode].filename, 71);
  dstrncpy(inodes[old_inode].filename, newname, dstrlen(newname));
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
    int filename_idx;
    int i;

    if(sb.valid == 0)
    {
        return DFS_FAIL;
    }

    filename_idx = DfsInodeFilenameExists(filename);

    if(filename_idx != DFS_FAIL)
    {
      return filename_idx;
    }

    if(LockHandleAcquire(inode_lock) == SYNC_FAIL)
    {
      return DFS_FAIL;
    }

    for(i = 0; i <= DFS_INODE_MAX_NUM - 1; i++)
    {
      if(inodes[i].valid == 0)
      {
        inodes[i].valid = 1;

        if(LockHandleRelease(inode_lock) == SYNC_FAIL)
        {
          inodes[i].valid = 0;
          return DFS_FAIL;
        }

        inodes[i].file_size = 0;
        dstrncpy(inodes[i].filename, filename, dstrlen(filename));
        bzero((char *)inodes[i].direct_table, sizeof(inodes[i].direct_table));
        inodes[i].num_indirect_blocks = 0;
        inodes[i].num_double_indirect_blocks = 0;

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
    int i;
    int j;
    dfs_block temp;
    dfs_block temp2;
    uint32 * int_temp;
    uint32 * int_temp2;
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

    inodes[handle].file_size = 0;
    bzero(inodes[handle].filename, 71);

    for(i = 0; i <= 9; i++)
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

    if(inodes[handle].num_indirect_blocks != 0)
    {
      not_translation = 0;
      dfs_block_size = DfsReadBlock(inodes[handle].num_indirect_blocks, &temp);
      not_translation = 1;
      if(dfs_block_size == DFS_FAIL)
      {
        LockHandleRelease(inode_lock);
        return DFS_FAIL;
      }

      int_temp = (uint32*)temp.data;

      for(i = 0; i <= 9; i++)
      {
        if(int_temp[i] != 0)
        {
          if(DfsFreeBlock(int_temp[i]) == DFS_FAIL)
          {
            LockHandleRelease(inode_lock);
            return DFS_FAIL;
          }
        }
      }

      if(DfsFreeBlock(inodes[handle].num_indirect_blocks) == DFS_FAIL)
      {
        LockHandleRelease(inode_lock);
        return DFS_FAIL;
      }

      inodes[handle].num_indirect_blocks = 0;
    }

    if(inodes[handle].num_double_indirect_blocks != 0)
    {
      not_translation = 0;
      dfs_block_size = DfsReadBlock(inodes[handle].num_double_indirect_blocks, &temp);
      not_translation = 1;
    
      if(dfs_block_size == DFS_FAIL)
      {
        LockHandleRelease(inode_lock);
        return DFS_FAIL;
      }

      int_temp = (uint32*)temp.data;

      for(i = 0; i <= 9; i++)
      {
        if(int_temp[i] != 0)
        {
          not_translation = 0;
          dfs_block_size = DfsReadBlock(int_temp[i], &temp2);
          not_translation = 1;
          
          if(dfs_block_size == DFS_FAIL)
          {
            LockHandleRelease(inode_lock);
            return DFS_FAIL;
          }
          
          int_temp2 = (uint32*)temp2.data;

          for(j = 0; j <= 9; j++)
          {
            if(int_temp2[j] != 0)
            {
              if(DfsFreeBlock(int_temp2[j]) == DFS_FAIL)
              {
                LockHandleRelease(inode_lock);
                return DFS_FAIL;
              }
            }
          }

          if(DfsFreeBlock(int_temp[i]) == DFS_FAIL)
          {
            LockHandleRelease(inode_lock);
            return DFS_FAIL;
          }
        }
      }
    
      if(DfsFreeBlock(inodes[handle].num_double_indirect_blocks) == DFS_FAIL)
      {
        LockHandleRelease(inode_lock);
        return DFS_FAIL;
      }
      
      inodes[handle].num_double_indirect_blocks = 0;
    }
    
    inodes[i].valid = 0;

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
    int start_block;
    int end_block;
    int end_byte;
    int total_read = 0;
    int one_read;
    int physical_block;
    int block_num;
    int block_offset;
    dfs_block temp;

    if(sb.valid == 0)
    {
       return DFS_FAIL;
    }

    if(handle < 0 || handle > DFS_INODE_MAX_NUM - 1 || start_byte < 0 || num_bytes < 0 || start_byte > inodes[handle].file_size || inodes[handle].valid == 0)
    {
      return DFS_FAIL;
    }

    end_byte = (start_byte + num_bytes - 1) > inodes[handle].file_size ? inodes[handle].file_size : (start_byte + num_bytes - 1);
    start_block = start_byte / sb.block_size;
    block_num = start_block;
    end_block = end_byte / sb.block_size;

    while(block_num <= end_block)
    {
      physical_block = DfsInodeTranslateVirtualToFilesys(handle, block_num);
      if(physical_block == DFS_FAIL)
      {
        return DFS_FAIL;
      }
      one_read = DfsReadBlock(physical_block, &temp);
      if(one_read == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      if(start_block == end_block)
      {
        one_read = end_byte - start_byte + 1;
        block_offset = start_byte % sb.block_size;
      }
      else if(block_num == start_block)
      {
        block_offset = start_byte % sb.block_size;
        one_read = sb.block_size - block_offset;
      }
      else if(block_num == end_block)
      {
        block_offset = 0;
        one_read = (end_byte % sb.block_size) + 1;
      }
      else
      {
        block_offset = 0;
        one_read = sb.block_size;
      }

      bcopy(temp.data + block_offset, (char *)mem + total_read, one_read);
      total_read = total_read + one_read;
      block_num = block_num + 1;
    }

    return total_read;
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
    int one_write;
    int total_write = 0;
    int start_block;
    int end_block;
    int end_byte;
    int physical_block;
    int block_num;
    int block_offset;
    dfs_block temp;
    int i;

    if(sb.valid == 0)
    {
      return DFS_FAIL;
    }

    if(handle < 0 || handle > DFS_INODE_MAX_NUM - 1 || start_byte < 0 || num_bytes < 0 || inodes[handle].valid == 0)
    {
      return DFS_FAIL;
    }

    end_byte = start_byte + num_bytes - 1;
    start_block = start_byte / sb.block_size;
    block_num = start_block;
    end_block = end_byte / sb.block_size;

    // CHANGE: allocate blocks before writing
    for(i = 0; i <= start_block - 1; i++)
    {
      physical_block = DfsInodeTranslateVirtualToFilesys(handle, i);
      
      if(physical_block == DFS_FAIL)
      {
        physical_block = DfsInodeAllocateVirtualBlock(handle, i);

        if(physical_block == DFS_FAIL)
        {
          return DFS_FAIL;
        }

        bzero(temp.data, sb.block_size);

        if(DfsWriteBlock(physical_block, &temp) == DFS_FAIL)
        {
          return DFS_FAIL;
        }
      }
    }
    
    while(block_num <= end_block)
    {
      physical_block = DfsInodeTranslateVirtualToFilesys(handle, block_num);
      
      if(physical_block == DFS_FAIL)
      {
        physical_block = DfsInodeAllocateVirtualBlock(handle, block_num);

        if(physical_block == DFS_FAIL)
        {
          return DFS_FAIL;
        }

        bzero(temp.data, sb.block_size);

        if(DfsWriteBlock(physical_block, &temp) == DFS_FAIL)
        {
          return DFS_FAIL;
        }
      }

      one_write = DfsReadBlock(physical_block, &temp);
      
      if(one_write == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      if(start_block == end_block)
      {
        one_write = end_byte - start_byte + 1;
        block_offset = start_byte % sb.block_size;
      }
      else if(block_num == start_block)
      {
        block_offset = start_byte % sb.block_size;
        one_write = sb.block_size - block_offset;
      }
      else if(block_num == end_block)
      {
        block_offset = 0;
        one_write = (end_byte % sb.block_size) + 1;
      }
      else
      {
        block_offset = 0;
        one_write = sb.block_size;
      }

      bcopy((char *)mem + total_write, temp.data + block_offset, one_write);
      
      if(DfsWriteBlock(physical_block, &temp) == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      total_write = total_write + one_write;
      block_num = block_num + 1;
    }

    if((start_byte + total_write) > inodes[handle].file_size)
    {
      inodes[handle].file_size = start_byte + total_write;
    }
    
    return total_write;
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

    if(handle > DFS_INODE_MAX_NUM - 1 || inodes[handle].valid == 0)
    {
      return DFS_FAIL;
    }

    return inodes[handle].file_size;
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
  dfs_block temp;
  dfs_block temp2;
  uint32 * int_temp;
  uint32 * int_temp2;
  int check_num;
  int dfs_block_size;
  int first_indirect_idx;
  int double_indirect_idx;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  if(handle < 0 || handle > DFS_INODE_MAX_NUM - 1 || inodes[handle].valid == 0 || virtual_blocknum < 0 || virtual_blocknum > (10 + (sb.block_size / 4) + (sb.block_size / 4) * (sb.block_size / 4) - 1))
  {
    return DFS_FAIL;
  }

  physical_block_num = DfsAllocateBlock();

  if(physical_block_num == DFS_FAIL)
  {
    return DFS_FAIL;
  }

  if(virtual_blocknum <= 9)
  {
    inodes[handle].direct_table[virtual_blocknum] = physical_block_num;
  }
  else if(virtual_blocknum <= 9 + (sb.block_size / 4))
  {
    if(inodes[handle].num_indirect_blocks == 0)
    {
      inodes[handle].num_indirect_blocks = DfsAllocateBlock();

      if(inodes[handle].num_indirect_blocks == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      bzero(temp.data, sb.block_size);

      not_translation = 0;
      if(DfsWriteBlock(inodes[handle].num_indirect_blocks, &temp) == DFS_FAIL)
      {
        not_translation = 1;
        return DFS_FAIL;
      }
      not_translation = 1;
    }

    not_translation = 0;
    dfs_block_size = DfsReadBlock(inodes[handle].num_indirect_blocks, &temp);
    not_translation = 1;
    
    if(dfs_block_size == DFS_FAIL)
    {
      return DFS_FAIL;
    }

    int_temp = (uint32*)temp.data;
    first_indirect_idx = virtual_blocknum - 10;
    int_temp[first_indirect_idx] = physical_block_num;
    
    not_translation = 0;
    if(DfsWriteBlock(inodes[handle].num_indirect_blocks, &temp) == DFS_FAIL)
    {
      not_translation = 1;
      return DFS_FAIL;
    }
    not_translation = 1;
  }
  else
  {
    if(inodes[handle].num_double_indirect_blocks == 0)
    {
      inodes[handle].num_double_indirect_blocks = DfsAllocateBlock();

      if(inodes[handle].num_double_indirect_blocks == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      bzero(temp.data, sb.block_size);

      not_translation = 0;
      if(DfsWriteBlock(inodes[handle].num_double_indirect_blocks, &temp) == DFS_FAIL)
      {
        not_translation = 1;
        return DFS_FAIL;
      }
      not_translation = 1;
    }

    not_translation = 0;
    dfs_block_size = DfsReadBlock(inodes[handle].num_double_indirect_blocks, &temp);
    not_translation = 1;
    
    if(dfs_block_size == DFS_FAIL)
    {
      return DFS_FAIL;
    }

    int_temp = (uint32*)temp.data;
    first_indirect_idx = (virtual_blocknum - 10 - (sb.block_size / 4)) / (sb.block_size / 4);

    if(int_temp[first_indirect_idx] == 0)
    {
      check_num = DfsAllocateBlock();

      if(check_num == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      int_temp[first_indirect_idx] = (uint32)check_num;
      bzero(temp2.data, sb.block_size);
      
      not_translation = 0;
      if(DfsWriteBlock(int_temp[first_indirect_idx], &temp2) == DFS_FAIL)
      {
        not_translation = 1;
        return DFS_FAIL;
      }
      not_translation = 1;
    }

    not_translation = 0;
    if(DfsWriteBlock(inodes[handle].num_double_indirect_blocks, &temp) == DFS_FAIL)
    {
      not_translation = 1;
      return DFS_FAIL;
    }
    not_translation = 1;

    not_translation = 0;
    dfs_block_size = DfsReadBlock(int_temp[first_indirect_idx], &temp2);
    not_translation = 1;
    if(dfs_block_size == DFS_FAIL)
    {
      return DFS_FAIL;
    }

    int_temp2 = (uint32*)temp2.data;
    double_indirect_idx = (virtual_blocknum - 10 - (sb.block_size / 4)) % (sb.block_size / 4);
    int_temp2[double_indirect_idx] = physical_block_num;

    not_translation = 0;
    if(DfsWriteBlock(int_temp[first_indirect_idx], &temp2) == DFS_FAIL)
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
  dfs_block temp;
  dfs_block temp2;
  uint32 * int_temp;
  uint32 * int_temp2;
  int dfs_block_size;
  int first_indirect_idx;
  int double_indirect_idx;

  if(sb.valid == 0)
  {
    return DFS_FAIL;
  }

  if(handle < 0 || handle > DFS_INODE_MAX_NUM - 1 || inodes[handle].valid == 0 || virtual_blocknum < 0 || virtual_blocknum > (10 + (sb.block_size / 4) + (sb.block_size / 4) * (sb.block_size / 4) - 1) || virtual_blocknum > (inodes[handle].file_size / sb.block_size))
  {
    return DFS_FAIL;
  }

  if(virtual_blocknum <= 9)
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
  else if(virtual_blocknum <= 9 + (sb.block_size / 4))
  {
    if(inodes[handle].num_indirect_blocks != 0)
    {
      not_translation = 0;
      dfs_block_size = DfsReadBlock(inodes[handle].num_indirect_blocks, &temp);
      not_translation = 1;
    
      if(dfs_block_size == DFS_FAIL)
      {
        return DFS_FAIL;
      }
    }
    else
    {
      return DFS_FAIL;
    }

    int_temp = (uint32*)temp.data;
    first_indirect_idx = virtual_blocknum - 10;

    if(int_temp[first_indirect_idx] != 0)
    {
      return int_temp[first_indirect_idx];
    }
    else
    {
      return DFS_FAIL;
    }
  }
  else
  {
    if(inodes[handle].num_double_indirect_blocks != 0)
    {
      not_translation = 0;
      dfs_block_size = DfsReadBlock(inodes[handle].num_double_indirect_blocks, &temp);
      not_translation = 1;
    
      if(dfs_block_size == DFS_FAIL)
      {
        return DFS_FAIL;
      }

      int_temp = (uint32*)temp.data;
      first_indirect_idx = (virtual_blocknum - 10 - (sb.block_size / 4)) / (sb.block_size / 4);

      if(int_temp[first_indirect_idx] != 0)
      {
        not_translation = 0;
        dfs_block_size = DfsReadBlock(int_temp[first_indirect_idx], &temp2);
        not_translation = 1;

        if(dfs_block_size == DFS_FAIL)
        {
          return DFS_FAIL;
        }

        int_temp2 = (uint32*)temp2.data;
        double_indirect_idx = (virtual_blocknum - 10 - (sb.block_size / 4)) % (sb.block_size / 4);

        if(int_temp2[double_indirect_idx] != 0)
        {
          return int_temp2[double_indirect_idx];
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
  int cache_idx = blocknum & CACHE_IDX_MASK;

  num_total_access += 1;
  
  for(i = cache_idx; i <= cache_idx + CACHE_WAYS - 1; i++)
  {
    if(caches[i].valid == 1 && caches[i].blocknum == blocknum)
    {
      num_hit += 1;
      return i;
    }
  }
  
  return DFS_FAIL;
}


int DfsCacheAllocateSlot(int blocknum) {
  int i;
  int cache_dest = -1;
  dfs_block temp;
  int cache_idx = blocknum & CACHE_IDX_MASK;
  
  for(i = cache_idx; i <= cache_idx + CACHE_WAYS - 1; i++)
  {
    if(caches[i].valid == 0)
    {
      cache_dest = i;
      break;
    }
  }

  if(cache_dest == -1)
  {
    cache_dest = DfsCacheReplacePolicy(blocknum);
  }

  if(cache_dest < 0 || cache_dest > CACHE_SIZE - 1)
  {
    cache_dest = 0;
  }

  if(caches[cache_dest].valid == 1 && caches[cache_dest].dirty == 1)
  {
    bcopy(caches[cache_dest].data, temp.data, sb.block_size);

    if(DfsWriteBlockUncached(caches[cache_dest].blocknum, &temp) == DFS_FAIL)
    {
      return DFS_FAIL;
    }

    caches[cache_dest].dirty = 0;
  }

  return cache_dest;
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
    if(caches[i].valid == 1 && caches[i].dirty == 1)
    {
      bcopy(caches[i].data, temp.data, sb.block_size);
      
      if(DfsWriteBlockUncached(caches[i].blocknum, &temp) == DFS_FAIL)
      {
        LockHandleRelease(cache_lock);
        return DFS_FAIL;
      }

      caches[i].dirty = 0;
    }
  }

  if(LockHandleRelease(cache_lock) == SYNC_FAIL)
  {
    return DFS_FAIL;
  }

  return DFS_SUCCESS;
}


int DfsCacheReplacePolicy(int blocknum) {
  int i;
  int cache_idx = blocknum & CACHE_IDX_MASK;
  int cache_dest = 0;
  double min_time = ClkGetCurTime();

  for(i = cache_idx; i <= cache_idx + CACHE_WAYS - 1; i++)
  {
    if(caches[i].timestamp < min_time)
    {
      min_time = caches[i].timestamp;
      cache_dest = i;
    }
  }

  return cache_dest;
}
