#include "filesys/cache.h"
#include "devices/block.h"


#define  MAX_NUM_ENTRIES 64
#define  N_CHANCE				 5

/* construct array */
static struct cache_entry *cache[MAX_NUM_ENTRIES];
bool is_cache_init = false;

/* student testing-1 */
int cache_read_cnt = 0;
int cache_hit_cnt = 0;

struct lock entry_lock;
struct lock clock_lock;

/* Initializes cache array with 64 cache entries. Each cache
   entry is initiaized with default values. */
void
cache_init ()
{
  int i;
  /* construct entry and add to each index of array. Init each entry. */
	for (i = 0; i < MAX_NUM_ENTRIES; i++) {
		cache[i] = (struct cache_entry *) malloc (sizeof (struct cache_entry));
		cache_entry_init (cache[i]);
    cache[i]->ref_count = 0;
	}
  /* Lock for the rotating hand of the clock algorithm */
  lock_init (&entry_lock);
  /* Lock for the rotating hand of the clock algorithm */
  lock_init (&clock_lock);
}

/* Initializes a cache entry with default values for all of its fields.
   Note: ref_count is not reset. This is on purpose for accurate synchronization. */
void
cache_entry_init (struct cache_entry *entry)
{
	entry->accessed = false;
	entry->modified = false;

  entry->block = NULL;
  entry->sector = 4294967295;

	entry->n_chance = 0;

	entry->read_cnt = 0;
	entry->write_cnt = 0;
}

/* Called when the cache is full, and there is a call to read_data that needs an empty
   cache entry. get_cache_entry () determines which entry in the cache should 
   be evicted based on a Clock Algorithm with Nth Chance. The function returns a pointer 
   to the cache entry that should be evicted. */
struct cache_entry *
get_cache_entry ()
{
  int i;
  struct cache_entry *entry;

  lock_acquire (&clock_lock);
  
  while (true)
    {
      for (i = 0; i < MAX_NUM_ENTRIES; i++)
        {
          entry = cache[i];
          /* Recently Accessed */
          if (entry->accessed == true)
            /* Set to not (i.e. no longer) recently accessed */
            entry->accessed = false;
          /* Not recently accessed */
          else
            {
              if (entry->ref_count == 0)
                {
                  entry->n_chance++;
                  if (entry->n_chance == N_CHANCE)
                    {
                      lock_release (&clock_lock);
                      return entry;
                    }
                }
            }
        }
    }
}

/* Takes in a block sector number, loops through the cache, and returns the
   cache entry that corresponds to the block sector number. If the block is
   not in cache, returns NULL. */
struct cache_entry *
find_block_in_cache (struct block *block, block_sector_t sector)
{
  struct cache_entry *entry;
  int i;
  for (i = 0; i < MAX_NUM_ENTRIES; i++)
    {
      entry = cache[i];
      if (entry->block == block && entry->sector == sector)
        return entry;
    }
  return NULL;
}

/* First checks if data is in cache. If it is, copy data from
   cache to buffer. If it isn't, find an entry to evict in cache
	 and if the entry is dirty, we write back to disk from cache to disk.
	 Then, copy the data from cache to buffer. */
void
cache_read (struct block *block, block_sector_t sector, void *buffer)
{
  /* initialize cache list */
  if (!is_cache_init) 
    {
      cache_init ();
      is_cache_init = true;
    }

  /* increment cache_read_count */
  cache_read_cnt++;

  /* check if it's in cache */
  struct cache_entry *entry = find_block_in_cache (block, sector);

  /* found in cache */
	if (entry != NULL) 
    {
      cache_hit_cnt++;

      /* increment ref_count */
      lock_acquire (&entry_lock);
      entry->ref_count++;
      lock_release (&entry_lock);

  		/* copy from cache to buffer */
  		memcpy (buffer, entry->data, BLOCK_SECTOR_SIZE);

      /* update fields */
  		entry->accessed = true;
  		entry->read_cnt++;

      /* decrement ref_count */
      lock_acquire (&entry_lock);
      entry->ref_count--;
      lock_release (&entry_lock);
  	} 
  /* not in cache */
  else
    {
  		/* find entry to evict */
  		entry = get_cache_entry ();

      /* increment ref_count */
      lock_acquire (&entry_lock);
      entry->ref_count++;
      lock_release (&entry_lock);

  		/* if dirty */
  		if (entry->modified)
  			/* write back */
  			block_write (entry->block, entry->sector, entry->data);

  		/* reset all fields (except ref_count) */
  		cache_entry_init (entry);

      /* update fields */
      entry->accessed = true;
      entry->read_cnt++;
      entry->block = block;
      entry->sector = sector;

  		/* read from disk to cache */
  		block_read (block, sector, entry->data);

      /* read from disk to thread */
      memcpy (buffer, entry->data, BLOCK_SECTOR_SIZE);

      /* decrement ref_count */
      lock_acquire (&entry_lock);
      entry->ref_count--;
      lock_release (&entry_lock);
  	}
}

/* First checks if data is in cache. If it isn't, write data
   directly to disk. If it is, write data to cache. */
void
cache_write (struct block *block, block_sector_t sector, const void *buffer)
{
  /* initialize cache list */
  if (!is_cache_init) 
    {
      cache_init ();
      is_cache_init = true;
    } 

  /* check if it's in cache */
  struct cache_entry *entry = find_block_in_cache (block, sector);

  if (entry != NULL)  /* found in cache */
    {
      /* increment ref_count (no eviction allowed while ref_count is non-zero) */
      lock_acquire (&entry_lock);
      entry->ref_count++;
      lock_release (&entry_lock);

      /* Write (copy) buffer data into cache entry, and update fields. */
      memcpy (entry->data, buffer, BLOCK_SECTOR_SIZE);

      /* update fields */
      entry->accessed = true;
      entry->modified = true;
      entry->write_cnt++;

      /* decrement ref_count */
      lock_acquire (&entry_lock);
      entry->ref_count--;
      lock_release (&entry_lock);
    }

  /* not in cache */
  else
    /* Write buffer data to directly to disk. */
    block_write (block, sector, buffer);
}

/* student testing-1 */
void
reset_cache_cnt ()
{
  cache_read_cnt = 0;
  cache_hit_cnt = 0;
}

int
get_cache_read_cnt ()
{
  return cache_read_cnt;
}

int
get_cache_hit_cnt ()
{
  return cache_hit_cnt;
}

