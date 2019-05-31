#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include <list.h>

#define WRITE_BACK_INTERVAL 5 * TIMER_FREQ
#define MAX_FILESYS_CACHE_SIZE 64

uint32_t cache_size;          /* Number of cache entry in cache. Limited to MAX_FILESYS_CACHE_SIZE. */
struct list caches;           /* List of all buffers. */
struct lock cache_lock;       /* Lock for buffer cache. */

struct cache_entry 
{
  uint8_t block[BLOCK_SECTOR_SIZE];   /* The cached sectors. */
  block_sector_t sector;              /* Index of the block device sector. */
  bool dirty;                         /* Is it writted or not. */
  bool accessed;                      /* Used in the cached replacement algorithm. */
  int open_cnt;                       /* Number of threads reading this entry. */
  struct list_elem elem;              /* Required by the linked list. */
};

void cache_init (void);
struct cache_entry *find_cached_sector (block_sector_t sector);
struct cache_entry* access_sector (block_sector_t sector, bool dirty);
struct cache_entry* replace_cached_sector (block_sector_t sector, bool dirty);
void write_all_cache_to_disk (bool halt);
void write_back (void *aux);
void read_ahead (void *aux);
void start_read_ahead (block_sector_t sector);

#endif /* filesys/cache.h */
