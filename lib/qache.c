/*
 *      Simple and Quick Shared Memory Cache
 *
 *	(c) 2005 Martin Mares <mj@ucw.cz>
 */

#include "sherlock/sherlock.h"

#include <sys/mman.h>

/* FIXME: do we really need to msync() the regions on Linux? */

/*
 *  On-disk format:
 *	qache_header
 *	qache_entry[max_entries]	table of entries and their keys
 *	u32 qache_hash[hash_size]	hash table pointing to keys
 *	padding				to a multiple of block size
 *	blocks[]			data blocks, each block starts with u32 next_ptr
 */

struct qache_header {
  u32 magic;				/* QCACHE_MAGIC */
  u32 block_size;			/* Parameters as in qache_params */
  u32 num_blocks;
  u32 format_id;
  u32 entry_table_start;		/* Array of qache_entry's */
  u32 max_entries;
  u32 hash_table_start;			/* Hash table containing all keys */
  u32 hash_size;
  u32 lru_first;			/* First entry in the LRU */
  u32 first_free_entry;			/* Head of the list of free entries */
  u32 first_free_block;			/* Head of the list of free blocks */
};

#define QACHE_MAGIC 0xb79f6d12

struct qache_entry {
  u32 lru_prev, lru_next;
  u32 data_len;				/* ~0 if a free entry */
  u32 first_data_block;			/* next free if a free entry */
  qache_key_t key;
  u32 hash_next;
};

struct qache {
  struct qache_header *hdr;
  int fd;
  byte *mmap_data;
  uns file_size;
};
