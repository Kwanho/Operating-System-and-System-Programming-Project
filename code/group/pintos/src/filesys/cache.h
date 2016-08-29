#include <list.h>
#include <string.h>
#include "threads/malloc.h"
#include "devices/block.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"

struct cache_entry 
  {
    bool accessed;				/* recently accessed */
    bool modified;				/* dirty bit */

    int ref_count;				/* value > 0 indicates operation in progress */
    int n_chance;				/* for clock algorithm with N chances */

    int read_cnt;			    /* count reads to this cache_entry, initially 0 */
    int write_cnt;		    	/* count writes to this cache_entry, initially 0 */

    struct block *block;        
    block_sector_t sector;      /* Sector number of disk location */
    char data[512];				/* 512 bytes of data */
  };

void cache_init (void);
void cache_entry_init (struct cache_entry *entry);

struct cache_entry * get_cache_entry (void);
struct cache_entry * find_block_in_cache (struct block *block, block_sector_t sector);

void cache_read (struct block *block, block_sector_t sector, void *buffer);
void cache_write (struct block *block, block_sector_t sector, const void *buffer);


/* student testing-1 */
void reset_cache_cnt (void);
int get_cache_read_cnt (void);
int get_cache_hit_cnt (void);