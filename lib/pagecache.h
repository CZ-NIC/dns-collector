/*
 *	Sherlock Library -- File Page Cache
 *
 *	(c) 1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#ifndef _SHERLOCK_PAGECACHE_H
#define _SHERLOCK_PAGECACHE_H

#include "lib/lists.h"

struct page_cache;

struct page {
  node n;				/* Node in page list */
  node hn;				/* Node in hash table */
  sh_off_t pos;
  uns fd;
  uns flags;
  uns lock_count;
  byte data[0];
};

#define PG_FLAG_DIRTY		1
#define PG_FLAG_VALID		2

struct page_cache *pgc_open(uns page_size, uns max_pages);
void pgc_close(struct page_cache *);
void pgc_debug(struct page_cache *, int mode);
void pgc_flush(struct page_cache *);				/* Write all unwritten pages */
void pgc_cleanup(struct page_cache *);				/* Deallocate all unused buffers */
struct page *pgc_read(struct page_cache *, int fd, sh_off_t);	/* Read page and lock it */
struct page *pgc_get(struct page_cache *, int fd, sh_off_t);	/* Get page for writing */
struct page *pgc_get_zero(struct page_cache *, int fd, sh_off_t); /* ... and clear it */
void pgc_put(struct page_cache *, struct page *);		/* Release page */
void pgc_mark_dirty(struct page_cache *, struct page *);	/* Mark locked page as dirty */
byte *pgc_read_data(struct page_cache *, int fd, sh_off_t, uns *);	/* Partial reading */

#endif
