/*
 *	Sherlock Library -- Fast Database Management Routines -- Internal Declarations
 *
 *	(c) 1999--2001 Martin Mares <mj@ucw.cz>
 */

#define SDBM_NUM_FREE_PAGE_POOLS 32

struct sdbm_root {			/* Must fit in 1K which is minimum page size */
  u32 magic;
  u32 version;
  u32 page_order;			/* Binary logarithm of page size */
  s32 key_size;				/* Key/val size, -1=variable */
  s32 val_size;
  u32 dir_start;			/* First page of the page directory */
  u32 dir_order;			/* Binary logarithm of directory size */
  /*
   *  As we know the only thing which can be freed is the page directory
   *  and it can grow only a limited number of times, we can use a very
   *  simple-minded representation of the free page pool. We also assume
   *  these entries are sorted by start position.
   */
  struct {
    u32 first;
    u32 count;
  } free_pool[SDBM_NUM_FREE_PAGE_POOLS];
};

struct sdbm_bucket {
  u32 used;				/* Bytes used in this bucket */
  byte data[0];
};

struct sdbm {
  struct page_cache *cache;
  int fd;
  struct sdbm_root *root;
  struct page *root_page;
  int key_size;				/* Cached values from root page */
  int val_size;
  uns page_order;
  uns page_size;
  uns page_mask;			/* page_size - 1 */
  uns dir_size;				/* Page directory size in entries */
  uns dir_shift;			/* Number of significant bits of hash function */
  uns file_size;			/* in pages */
  uns flags;
  uns find_page, find_pos;		/* Current pointer for sdbm_find_next() */
  uns find_free_list;			/* First free list entry not skipped by sdbm_find_next() */
};

#define SDBM_MAGIC 0x5344424d
#define SDBM_VERSION 2

#define GET32(p,o) *((u32 *)((p)+(o)))
