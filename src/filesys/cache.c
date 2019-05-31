#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Initialize cache list and cache lock. */
void 
cache_init ()
{
  list_init (&caches);
  lock_init (&cache_lock);
  cache_size = 0;

  // Write the cache back into the disk automatically.
  thread_create ("filesys_cache_writeback", 0, write_back, NULL);
}

/* Find the block in cache list, return NULL if not exist. */
struct
cache_entry* find_cached_sector (block_sector_t sector)
{
  struct cache_entry *c;
  struct list_elem *e;

  for (e = list_begin (&caches); e != list_end (&caches); e = list_next (e))
    if ((c = list_entry (e, struct cache_entry, elem))->sector == sector)
      return c;

  return NULL;
}

/* Access sector from file system (block), check if already cached here. */
struct 
cache_entry* access_sector (block_sector_t sector, bool dirty)
{
  lock_acquire (&cache_lock);

  struct cache_entry *c = find_cached_sector (sector);
  if (c)
  {
    c->open_cnt++;
    c->dirty |= dirty;
    c->accessed = true;
    lock_release (&cache_lock);
    return c;
  }

  c = replace_cached_sector (sector, dirty);
  if (!c)
    PANIC ("Not enough memory for buffer cache.");
  
  lock_release (&cache_lock);
  
  return c;
}

/* Replace cached block here. Using second chance replacement algorithm. */
struct 
cache_entry* replace_cached_sector (block_sector_t sector, bool dirty)
{
  struct cache_entry *c;
  if (cache_size < MAX_FILESYS_CACHE_SIZE)
  {
    cache_size++;
    c = malloc (sizeof (struct cache_entry));
    if (!c)
      return NULL;
    
    c->open_cnt = 0;
    list_push_back (&caches, &c->elem);
  }
  else
  {
    bool done = false;
    while (!done)
    { //Second chance algorithm
      struct list_elem *e;
      for (e = list_begin (&caches); e != list_end (&caches); e = list_next (e))
      {
        c = list_entry (e, struct cache_entry, elem);
        if (c->open_cnt > 0)
          continue;
        
        if (c->accessed)
          c->accessed = false;
        else
        {
          if (c->dirty)
            block_write (fs_device, c->sector, &c->block);
          
          done = true;
          break;
        }
      }
    }
  }

  c->open_cnt++;
  c->sector = sector;
  block_read (fs_device, c->sector, &c->block);
  c->dirty = dirty;
  c->accessed = true;

  return c;
}

/* Write all the buffer into disk. Called at shutting down file system. 
   Clear the buffer if halt is ture.*/
void 
write_all_cache_to_disk (bool halt)
{
  lock_acquire (&cache_lock);

  struct list_elem *next;
  struct list_elem *e = list_begin (&caches);
  while (e != list_end (&caches))
  {
    next = list_next (e);
    struct cache_entry *c = list_entry (e, struct cache_entry, elem);

    if (c->dirty)
    {
      block_write (fs_device, c->sector, &c->block);
      c->dirty = false;
    }

    if (halt)
    {
      list_remove (&c->elem);
      free (c);
    }

    e = next;
  }

  lock_release (&cache_lock);
}

/* Write the cache back into the disk at fixed frequency. */
void 
write_back (void *aux UNUSED)
{
  while (true)
  {
    timer_sleep (WRITE_BACK_INTERVAL);
    write_all_cache_to_disk (false);
  }
}

/* Start a thread to call read ahead function once. */
void 
start_read_ahead (block_sector_t sector)
{
  block_sector_t *arg = malloc (sizeof (block_sector_t));
  if (arg)
  {
    *arg = sector + 1;
    thread_create ("filesys_cache_readahead", 0, read_ahead, arg);
  }
}

/* Read ahead function. */
void 
read_ahead (void *aux)
{
  block_sector_t sector = *(block_sector_t *)aux;
  lock_acquire (&cache_lock);

  struct cache_entry *c = find_cached_sector (sector);
  if (!c)
    replace_cached_sector (sector, false);
  
  lock_release (&cache_lock);
  free (aux);
}
