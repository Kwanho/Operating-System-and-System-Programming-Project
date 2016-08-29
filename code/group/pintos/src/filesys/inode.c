#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Added function prototypes */
uint32_t new_space (struct inode* inode, uint32_t new_size);
static block_sector_t byte_to_sector_direct (const struct inode *inode, off_t pos);
static block_sector_t byte_to_sector_indirect_l1 (const struct inode *inode, off_t pos);
static block_sector_t byte_to_sector_indirect_l2 (const struct inode *inode, off_t pos);
uint32_t inode_close_indirect_l1 (struct inode *inode, uint32_t dir_sector, int counter);
uint32_t inode_close_indirect_l2 (struct inode *inode, uint32_t dir_sector, uint32_t l1_sector);
int get_chunk_size (struct inode *inode, off_t size, off_t offset, block_sector_t sector_ofs);
uint32_t file_ext_direct (struct inode* inode, uint32_t size_to_add);
uint32_t file_ext_indirect_l1 (struct inode* inode, uint32_t size_to_add);
uint32_t file_ext_indirect_l2 (struct inode* inode, uint32_t size_to_add);
uint32_t file_ext (struct inode* inode, uint32_t new_size);
void free_indirect (block_sector_t* sector, int num_ptrs);

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Constants */
#define NUMBER_DIRECT 100
#define BLOCK_POINTERS 128
#define L1_PLACE 100
#define L2_PLACE 101
#define NUM_BLOCKS 102


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    /* requires 2 bytes */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */

    /* Added by group 51 */
    block_sector_t blocks[NUM_BLOCKS];  /* data (100 direct, 1 L1, 1 L2, 125 bytes */
    int cur_index;
    int l1_index;
    int l2_index;

    int is_dir;                         /* 0: file, 1: directory */

    uint32_t unused[20];                /* Filler. */

  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* Will need to modify the bytes_to_sectors function to handle indirect stuff */
/* Consider adding more functions to compute if indirect is even used */

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */

    /* added by group 51 */

    /* list of pointers to the on disk blocks */
    /* must be the same size as above */
    block_sector_t blocks[NUM_BLOCKS];

    int cur_index;                     /* maps to max block being used */
    int l1_index;                      /* cur pos in any l1 indirection block */
    int l2_index;                      /* cur pos in any l2 block */

    off_t cur_off;                      /* the current offset we have read to */
    off_t cur_size;                     /* the current file size */

    int is_dir;                         /* 0: file, 1: directory */

    struct lock inode_lock;             /* inode syncronization */

  };

/* Finds byte_to_sector for direct blocks */
static block_sector_t
byte_to_sector_direct (const struct inode *inode, off_t pos) 
{
  return inode->blocks[pos/BLOCK_SECTOR_SIZE];
}

/* Finds byte_to_sector for indirect l1 blocks */
static block_sector_t
byte_to_sector_indirect_l1 (const struct inode *inode, off_t pos) 
{
  /* buffer for block read */
  int ptr_buffer[BLOCK_POINTERS];

  /* index 100 is l1 indirection */
  cache_read(fs_device, inode->blocks[L1_PLACE], &ptr_buffer);

  /* subtract off direct ptrs from pos (100*512), mod by (128*512)*/
  pos = ((pos - (NUMBER_DIRECT*BLOCK_SECTOR_SIZE)) % (BLOCK_POINTERS*BLOCK_SECTOR_SIZE));

  return ptr_buffer[pos/BLOCK_SECTOR_SIZE];
}

/* Finds byte_to_sector for indirect l2 blocks */
static block_sector_t
byte_to_sector_indirect_l2 (const struct inode *inode, off_t pos) 
{
  /* to read the l2 block with more pointers */
  int ptr_buffer[BLOCK_POINTERS];

  /* index 101 is the l2 indirection in blocks */
  cache_read(fs_device, inode->blocks[L2_PLACE], &ptr_buffer);

  /* pos -= (ind ptrs + dir ptrs) * 512; this reps num of bytes into l2 */
  pos -= (NUMBER_DIRECT+BLOCK_POINTERS)*BLOCK_SECTOR_SIZE;

  /* reflects the index of l2: pos / (512*128ptrs) */
  cache_read(fs_device, ptr_buffer[pos/(BLOCK_POINTERS*BLOCK_SECTOR_SIZE)], &ptr_buffer);

  /* mod by indirect nodes*byte size(128*512)*/
  pos = pos % (BLOCK_POINTERS*BLOCK_SECTOR_SIZE);

  return ptr_buffer[pos/BLOCK_SECTOR_SIZE];
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos, bool is_write) 
{
  ASSERT (inode != NULL);

  off_t size_to_compare;

  if (is_write)
    size_to_compare = inode->cur_size;
  else
    size_to_compare = inode->cur_off;

  if (pos < size_to_compare) 
    {
      /* DIRECT , elif L1, else L2*/
      if (pos < NUMBER_DIRECT*BLOCK_SECTOR_SIZE)                       /* Is still in direct ptr range 100*512 */
        return byte_to_sector_direct (inode, pos);

      else if (pos < BLOCK_SECTOR_SIZE*(NUMBER_DIRECT+BLOCK_POINTERS)) /* in the l1 indirect (512 * (128l1+100dir))*/
        return byte_to_sector_indirect_l1 (inode, pos);

      else                                                             /* in the l2 indirect (128^2*512) */
        return byte_to_sector_indirect_l2 (inode, pos);
    }
  
  return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, int is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof (*disk_inode) == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_dir = is_dir;

      size_t sectors = bytes_to_sectors (length);

      /* Allocate new inode/set fields to zero (file_ext will set them to real vals) */
      struct inode node;
      node.cur_index = 0;
      node.l1_index = 0;
      node.l2_index = 0;
      node.cur_size = 0;

      file_ext (&node, length);

      if (sectors > 0) 
        {
          static char zeros[BLOCK_SECTOR_SIZE];
          size_t i;
          
          for (i = 0; i < sectors; i++) 
            {
              if (i == 100)
                break;
              cache_write (fs_device, node.blocks[i], zeros);
            }
        }
      /* copy over all the blocks from the inode in mem to inode on disk */
      memcpy (&disk_inode->blocks, &node.blocks, sizeof (block_sector_t) * NUM_BLOCKS);

      /* copy metadata created from file_ext from inode to disk inode */
      disk_inode->cur_index = node.cur_index;
      disk_inode->l1_index = node.l1_index;
      disk_inode->l2_index = node.l2_index;
      //disk_inode->is_dir = node.is_dir;

      /* write the new disk inode to disk */
      cache_write (fs_device, sector, disk_inode);

      success = true;

      /* Get disk inode out of memory */
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  lock_init (&inode->inode_lock);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  struct inode_disk data;
  cache_read (fs_device, inode->sector, &data);

  /* new data initialized */
  memcpy (&inode->blocks, &data.blocks, sizeof(block_sector_t)*NUM_BLOCKS);
  inode->cur_index = data.cur_index;
  inode->l1_index = data.l1_index;
  inode->l2_index = data.l2_index;
  inode->cur_size = data.length;
  inode->cur_off = data.length;
  inode->is_dir = data.is_dir;
  /* end inode inits here */

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

uint32_t inode_close_indirect_l1 (struct inode *inode, uint32_t dir_sector, int counter)
{
  int num_ptrs;
  int l1_sector;

  uint32_t num = inode->cur_size - BLOCK_SECTOR_SIZE * NUMBER_DIRECT;
  uint32_t denom = BLOCK_SECTOR_SIZE;
  l1_sector = (num + (denom - 1)) / denom;

  /* check if we are using any indirect blocks */
  if (l1_sector != 0 && counter == NUMBER_DIRECT)
    {
      /* num ptrs remaining is the min between 128 & dir_sector */ 
      if (dir_sector > BLOCK_POINTERS) 
        num_ptrs = BLOCK_POINTERS;
      else 
        num_ptrs = dir_sector;

      free_indirect (&inode->blocks[L1_PLACE], num_ptrs);

      l1_sector -= 1;
    }
  return l1_sector;
}

uint32_t inode_close_indirect_l2 (struct inode *inode, uint32_t dir_sector, uint32_t l1_sector)
{
  /* all direct, indirect mult by size of a block */
  /* represents if we need to go into l2 */
  int min_l2 = (BLOCK_POINTERS+NUMBER_DIRECT) * BLOCK_SECTOR_SIZE;

  if (inode->cur_size > min_l2)
    {
      block_sector_t indirect_l1[BLOCK_POINTERS];

      cache_read(fs_device, inode->blocks[L2_PLACE], &indirect_l1);

      int tmp_ptrs;
      for(tmp_ptrs = l1_sector-1; tmp_ptrs >= 0; tmp_ptrs--)
        {
          int l2_num_ptrs;
          
          if (dir_sector > BLOCK_POINTERS)
            l2_num_ptrs = BLOCK_POINTERS;
          
          else
            l2_num_ptrs = dir_sector;

          free_indirect (&indirect_l1[tmp_ptrs], l2_num_ptrs);
          dir_sector -= l2_num_ptrs;
        }

      free_map_release (inode->blocks[L2_PLACE], 1);
    }
  return dir_sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          /* This was originally included, still need it */
          free_map_release (inode->sector, 1);

          /* round up int division */
          /* num: inode->cur_size denom: BLOCK_SECTOR_SIZE */
          uint32_t dir_sector = (inode->cur_size + (BLOCK_SECTOR_SIZE - 1)) / BLOCK_SECTOR_SIZE;

          /* DIRECT BLOCK */
          int counter;
          for (counter = 0; counter < NUMBER_DIRECT && dir_sector != 0; counter++, dir_sector--)
            {
              free_map_release (inode->blocks[counter], 1);
            }
          
          /* L1 INDIRECTION */
          uint32_t l1_sector = inode_close_indirect_l1 (inode, dir_sector, counter);

          /* L2 INDIRECTION */ 
          dir_sector = inode_close_indirect_l2 (inode, dir_sector, l1_sector);
        }
      /* If not removed, write it to disk */
      else
        {
          /* ref inode_create for similar code */
          struct inode_disk disk_node;
          disk_node.magic = INODE_MAGIC;
          disk_node.length = inode->cur_size;
          disk_node.cur_index = inode->cur_index;
          disk_node.l1_index = inode->l1_index;
          disk_node.l2_index = inode->l2_index;
          disk_node.is_dir = inode->is_dir;

          /* copy the data (total num of blocks) */
          memcpy(&disk_node.blocks, &inode->blocks, sizeof(block_sector_t)*NUM_BLOCKS);

          /* Write the struct to fs_device from inode sector*/
          cache_write (fs_device, inode->sector, &disk_node);
        }
      free (inode); 
    }
}

void
free_indirect(block_sector_t* sector, int num_ptrs)
{
  block_sector_t indirect_l1[BLOCK_POINTERS];

  cache_read(fs_device, *sector, &indirect_l1);

  int tmp_ptrs;
  for (tmp_ptrs = num_ptrs-1; tmp_ptrs >= 0; tmp_ptrs--)
    { 
      free_map_release (indirect_l1[tmp_ptrs], 1);
    }

  free_map_release (*sector, 1);
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Gets the chunk size for this inode. */
int
get_chunk_size (struct inode *inode, off_t size, off_t offset, block_sector_t sector_ofs)
{
  /* Bytes left in inode, bytes left in sector, lesser of the two. */
  off_t inode_left = inode_length (inode) - offset;
  int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
  int min_left = inode_left < sector_left ? inode_left : sector_left;

  int chunk_size = size < min_left ? size : min_left;
  return chunk_size;
}


/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset, 0);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      int chunk_size = get_chunk_size (inode, size, offset, sector_ofs);
      if (chunk_size <= 0)
        break;

      /* Replace with Cache Call */
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
          /* Read full sector directly into caller's buffer. */
          cache_read (fs_device, sector_idx, buffer + bytes_read);
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          cache_read (fs_device, sector_idx, bounce);

          /* Will still need this memcpy */
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  /* Need to extend the file for a write */
  /* b/c offset is past end of file */
  if (offset + size > inode->cur_size)
    {
      if (!inode->is_dir)
        lock_acquire (&(inode->inode_lock));
      uint32_t new_len = file_ext (inode, offset+size);

      if (!inode->is_dir)
        lock_release (&(inode->inode_lock));
      inode->cur_size = new_len;
    }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset, 1);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = get_chunk_size (inode, size, offset, sector_ofs);
      if (chunk_size <= 0)
        break;

      /* Replace with Cache */
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
          /* Write full sector directly to disk. */
          cache_write (fs_device, sector_idx, buffer + bytes_written);
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < (BLOCK_SECTOR_SIZE - sector_ofs)) 
            cache_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          cache_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  inode->cur_off = inode_length (inode);
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->cur_size;
}

/* Finds file_ext for direct blocks/ */
uint32_t
file_ext_direct (struct inode* inode, uint32_t size_to_add) 
{
  /* buffer we use for block write's, size of 1 block */
  static char dir_buf[BLOCK_SECTOR_SIZE];
  /* Fills up all the direct blocks */
  for (;inode->cur_index < NUMBER_DIRECT; size_to_add--, inode->cur_index++)
    {
      if (size_to_add == 0) 
        return size_to_add;

      /* Alloc 1 sector */
      free_map_allocate (1, &inode->blocks[inode->cur_index]);
      cache_write (fs_device, inode->blocks[inode->cur_index], dir_buf);
    }
  return size_to_add;
}

/* Finds file_ext for l1 indirect blocks. */
uint32_t
file_ext_indirect_l1 (struct inode* inode, uint32_t size_to_add)
{
  /* Fill up the indirect block (at l1 indirect, and not too large)*/
  if (inode->cur_index == L1_PLACE && size_to_add != 0) 
    {
      block_sector_t indirect_l1[BLOCK_POINTERS];

      if (inode->l1_index != 0) 
        /* grab the ptrs for l1 and put them in indirect l1 */
        cache_read (fs_device, inode->blocks[L1_PLACE], &indirect_l1);

      else
        /* alloc the indirect L1 block */
        free_map_allocate (1, &inode->blocks[L1_PLACE]);

      static char ind_buff[BLOCK_SECTOR_SIZE];
      /* populate the blocks */
      for (; inode->l1_index < BLOCK_POINTERS && size_to_add > 0; size_to_add--, inode->l1_index++)
        {
          /* Alloc 1 sector, sub off 100 (num direct) since it's the total index and need relatvie */
          free_map_allocate (1, &indirect_l1[inode->l1_index]);
          cache_write (fs_device, indirect_l1[inode->l1_index], ind_buff);
        }

      /* If we're out of l1 ptrs, set cur to l2 and l1 back to 0 */
      if (inode->l1_index == BLOCK_POINTERS) 
        {
          inode->cur_index++;
          inode->l1_index = 0;
        }

      /* write the block to the blocks array */
      cache_write (fs_device, inode->blocks[L1_PLACE], &indirect_l1);
    }
  return size_to_add;
}

/* Finds file_ext for l2 indirect blocks. */
uint32_t
file_ext_indirect_l2 (struct inode* inode, uint32_t size_to_add)
{
  /* max value is total num of ptrs */
  if (inode->cur_index == L2_PLACE && size_to_add != 0)
    {
      block_sector_t indirect_l2[BLOCK_POINTERS];

      /* direct+indirect ptrs */
      if (inode->l1_index != 0 || inode->l2_index != 0) 
        /* grab the ptrs for l1 and put them in indirect l1 */
        cache_read (fs_device, inode->blocks[L2_PLACE], &indirect_l2);

      else
        /* alloc the indirect L1 block */
        free_map_allocate (1, &inode->blocks[L2_PLACE]);

      /* Will go through the 128 indirect blocks this holds */
      while (inode->l1_index < BLOCK_POINTERS && size_to_add != 0)
        {
          block_sector_t inner_indirect_l2[BLOCK_POINTERS];

          if (inode->l2_index != 0)
            cache_read (fs_device, indirect_l2[inode->l1_index], &inner_indirect_l2);

          else  
            free_map_allocate (1, &indirect_l2[inode->l1_index]);

          static char ind_l2_buff[BLOCK_SECTOR_SIZE];
          for (; inode->l2_index < BLOCK_POINTERS && size_to_add > 0; size_to_add--, inode->l2_index++)
            {
              free_map_allocate (1, &inner_indirect_l2[inode->l2_index]);
              cache_write (fs_device, inner_indirect_l2[inode->l2_index], ind_l2_buff);
            }

          /* write the block to the blocks array */
          cache_write (fs_device, indirect_l2[inode->l1_index], &inner_indirect_l2);
        
          /* If we're out of l1 ptrs, set cur to l2 and l1 back to 0 */
          if (inode->l1_index == BLOCK_POINTERS) 
            {
              inode->l1_index++;
              inode->l2_index = 0;
            }
        }

      /* write the indirect l2 to the blocks, cur_indx should be 101*/
      cache_write (fs_device, inode->blocks[inode->cur_index], &indirect_l2);
    }
  return size_to_add;
}

/* This will return the new length and take in an inode and requested length
 * Will need to do some crazy f**king indirect computation. Probably need a few helpers for the indirection
 */
uint32_t
file_ext (struct inode* inode, uint32_t new_size) 
{
  uint32_t size_to_add = new_space (inode, new_size);

  /* DIRECT */
  size_to_add = file_ext_direct (inode, size_to_add);
  if (size_to_add == 0)
    return new_size;

  /* SINGLE INDIRECT */
  size_to_add = file_ext_indirect_l1 (inode, size_to_add);

  /* DOUBLE INDIRECT */
  size_to_add = file_ext_indirect_l2 (inode, size_to_add);

  /* return new_size if we were able to add all the data */
  if (size_to_add == 0) 
    return new_size;
  else
    return new_size - (size_to_add * BLOCK_SECTOR_SIZE);
}

uint32_t 
new_space (struct inode* inode, uint32_t new_size) 
{
  uint32_t new_len = (new_size + (BLOCK_SECTOR_SIZE - 1)) / BLOCK_SECTOR_SIZE;
  uint32_t old_len = (inode->cur_size + (BLOCK_SECTOR_SIZE - 1)) / BLOCK_SECTOR_SIZE;
  return new_len - old_len;
}

int
inode_cnt (const struct inode *inode)
{
  return inode->open_cnt;
}

void
dir_lock (struct inode *inode)
{
  lock_acquire (&(inode->inode_lock));
}

void
dir_unlock (struct inode *inode)
{
  lock_release (&(inode->inode_lock));
}

/* Returns is_dir of INODE's data. 0: file, 1: directory. */
int
inode_isdir (const struct inode *inode)
{
  return inode->is_dir;
}
