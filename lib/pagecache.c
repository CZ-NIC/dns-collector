/*
 *	Sherlock Library -- File Page Cache
 *
 *	(c) 1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib.h"
#include "pagecache.h"
#include "lfs.h"

struct page_cache {
  list free_pages;			/* LRU queue of free non-dirty pages */
  list locked_pages;			/* List of locked pages (starts with dirty ones) */
  list dirty_pages;			/* List of free dirty pages */
  uns page_size;			/* Bytes per page (must be a power of two) */
  uns free_count;			/* Number of free / dirty pages */
  uns total_count;			/* Total number of pages */
  uns max_pages;			/* Maximum number of free pages */
  uns hash_size;			/* Hash table size */
  uns stat_hit;				/* Number of cache hits */
  uns stat_miss;			/* Number of cache misses */
  uns stat_write;			/* Number of writes */
  list *hash_table;			/* List heads corresponding to hash buckets */
#ifndef SHERLOCK_HAVE_PREAD
  sh_off_t pos;				/* Current position in the file */
#endif
};

#define PAGE_NUMBER(pos) ((pos) & ~(sh_off_t)(c->page_size - 1))
#define PAGE_OFFSET(pos) ((pos) & (c->page_size - 1))

#define KEY_PAGE(key) PAGE_NUMBER(key)
#define KEY_FD(key) PAGE_OFFSET(key)

struct page_cache *
pgc_open(uns page_size, uns max_pages)
{
  struct page_cache *c = xmalloc(sizeof(struct page_cache));
  uns i;

  bzero(c, sizeof(*c));
  init_list(&c->free_pages);
  init_list(&c->locked_pages);
  init_list(&c->dirty_pages);
  c->page_size = page_size;
  c->max_pages = max_pages;
  c->hash_size = nextprime(c->max_pages);
  c->hash_table = xmalloc(sizeof(list) * c->hash_size);
  for(i=0; i<c->hash_size; i++)
    init_list(&c->hash_table[i]);
  return c;
}

void
pgc_close(struct page_cache *c)
{
  pgc_cleanup(c);
  ASSERT(EMPTY_LIST(c->locked_pages));
  ASSERT(EMPTY_LIST(c->dirty_pages));
  ASSERT(EMPTY_LIST(c->free_pages));
  free(c->hash_table);
  free(c);
}

static void
pgc_debug_page(struct page *p)
{
  printf("\tk=%08x f=%x c=%d\n", (uns) p->key, p->flags, p->lock_count);
}

void
pgc_debug(struct page_cache *c, int mode)
{
  struct page *p;

  printf(">> Page cache dump: pgsize=%d, pages=%d, freepages=%d of %d, hash=%d\n", c->page_size, c->total_count, c->free_count, c->max_pages, c->hash_size);
  printf(">> stats: %d hits, %d misses, %d writes\n", c->stat_hit, c->stat_miss, c->stat_write);
  if (mode)
    {
      puts("LRU list:");
      WALK_LIST(p, c->free_pages)
	pgc_debug_page(p);
      puts("Locked list:");
      WALK_LIST(p, c->locked_pages)
	pgc_debug_page(p);
      puts("Dirty list:");
      WALK_LIST(p, c->dirty_pages)
	pgc_debug_page(p);
    }
}

static void
flush_page(struct page_cache *c, struct page *p)
{
  int fd = KEY_FD(p->key);
  sh_off_t pos = KEY_PAGE(p->key);
  int s;

  ASSERT(p->flags & PG_FLAG_DIRTY);
#ifdef SHERLOCK_HAVE_PREAD
  s = pwrite(fd, p->data, c->page_size, pos);
#else
  if (c->pos != pos)
    sh_seek(fd, pos, SEEK_SET);
  s = write(fd, p->data, c->page_size);
  c->pos += s;
#endif
  if (s < 0)
    die("pgc_write(%d): %m", fd);
  if (s != (int) c->page_size)
    die("pgc_write(%d): incomplete page (only %d of %d)", s, c->page_size);
  p->flags &= ~PG_FLAG_DIRTY;
  c->stat_write++;
}

static inline uns
hash_page(struct page_cache *c, sh_off_t key)
{
  return key % c->hash_size;
}

static struct page *
get_page(struct page_cache *c, sh_off_t key)
{
  node *n;
  struct page *p;
  uns hash = hash_page(c, key);

  /*
   *  Return locked buffer for given page.
   */

  WALK_LIST(n, c->hash_table[hash])
    {
      p = SKIP_BACK(struct page, hn, n);
      if (p->key == key)
	{
	  /* Found in the cache */
	  rem_node(&p->n);
	  if (!p->lock_count)
	    c->free_count--;
	  return p;
	}
    }
  if (c->total_count < c->max_pages || !c->free_count)
    {
      /* Enough free space, expand the cache */
      p = xmalloc(sizeof(struct page) + c->page_size);
      c->total_count++;
    }
  else
    {
      /* Discard the oldest unlocked page */
      p = HEAD(c->free_pages);
      if (!p->n.next)
	{
	  /* There are only dirty pages here */
	  p = HEAD(c->dirty_pages);
	  flush_page(c, p);
	}
      ASSERT(!p->lock_count);
      rem_node(&p->n);
      rem_node(&p->hn);
      c->free_count--;
    }
  p->key = key;
  p->flags = 0;
  p->lock_count = 0;
  add_tail(&c->hash_table[hash], &p->hn);
  return p;
}

void
pgc_flush(struct page_cache *c)
{
  struct page *p;
  node *n;

  WALK_LIST_DELSAFE(p, n, c->dirty_pages)
    {
      flush_page(c, p);
      rem_node(&p->n);
      add_tail(&c->free_pages, &p->n);
    }
  WALK_LIST(p, c->locked_pages)
    if (p->flags & PG_FLAG_DIRTY)
      flush_page(c, p);
    else
      break;
}

void
pgc_cleanup(struct page_cache *c)
{
  struct page *p;
  node *n;

  pgc_flush(c);
  WALK_LIST_DELSAFE(p, n, c->free_pages)
    {
      ASSERT(!(p->flags & PG_FLAG_DIRTY) && !p->lock_count);
      rem_node(&p->n);
      c->free_count--;
      c->total_count--;
      free(p);
    }
  ASSERT(!c->free_count);
}

static inline struct page *
get_and_lock_page(struct page_cache *c, sh_off_t key)
{
  struct page *p = get_page(c, key);

  add_tail(&c->locked_pages, &p->n);
  p->lock_count++;
  return p;
}

struct page *
pgc_read(struct page_cache *c, int fd, sh_off_t pos)
{
  sh_off_t key;
  struct page *p;
  int s;

  ASSERT(!PAGE_OFFSET(pos));
  ASSERT(!PAGE_NUMBER(fd));
  key = pos | fd;
  p = get_and_lock_page(c, key);
  if (p->flags & PG_FLAG_VALID)
    c->stat_hit++;
  else
    {
      c->stat_miss++;
#ifdef SHERLOCK_HAVE_PREAD
      s = pread(fd, p->data, c->page_size, pos);
#else
      if (c->pos != pos)
	sh_seek(fd, pos, SEEK_SET);
      s = read(fd, p->data, c->page_size);
      c->pos += s;
#endif
      if (s < 0)
	die("pgc_read(%d): %m", fd);
      if (s != (int) c->page_size)
	die("pgc_read(%d): incomplete page (only %d of %d)", s, c->page_size);
      p->flags |= PG_FLAG_VALID;
    }
  return p;
}

struct page *
pgc_get(struct page_cache *c, int fd, sh_off_t pos)
{
  sh_off_t key;
  struct page *p;

  ASSERT(!PAGE_OFFSET(pos));
  ASSERT(!PAGE_NUMBER(fd));
  key = pos | fd;
  p = get_and_lock_page(c, key);
  p->flags |= PG_FLAG_VALID | PG_FLAG_DIRTY;
  return p;
}

void
pgc_put(struct page_cache *c, struct page *p)
{
  ASSERT(p->lock_count);
  if (--p->lock_count)
    return;
  rem_node(&p->n);
  if (p->flags & PG_FLAG_DIRTY)
    {
      add_tail(&c->dirty_pages, &p->n);
      c->free_count++;
    }
  else if (c->free_count < c->max_pages)
    {
      add_tail(&c->free_pages, &p->n);
      c->free_count++;
    }
  else
    {
      free(p);
      c->total_count--;
    }
}

void
pgc_mark_dirty(struct page_cache *c, struct page *p)
{
  ASSERT(p->lock_count);
  if (!(p->flags & PG_FLAG_DIRTY))
    {
      p->flags |= PG_FLAG_DIRTY;
      rem_node(&p->n);
      add_head(&c->locked_pages, &p->n);
    }
}

byte *
pgc_read_data(struct page_cache *c, int fd, sh_off_t pos, uns *len)
{
  struct page *p;
  sh_off_t page = PAGE_NUMBER(pos);
  uns offset = PAGE_OFFSET(pos);

  p = pgc_read(c, fd, page);
  pgc_put(c, p);
  *len = c->page_size - offset;
  return p->data + offset;
}

#ifdef TEST

int main(int argc, char **argv)
{
  struct page_cache *c = pgc_open(1024, 2);
  struct page *p, *q, *r;
  int fd = open("test", O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0)
    die("open: %m");
  pgc_debug(c, 1);
  p = pgc_get(c, fd, 0);
  pgc_debug(c, 1);
  strcpy(p->data, "one");
  pgc_put(c, p);
  pgc_debug(c, 1);
  p = pgc_get(c, fd, 1024);
  pgc_debug(c, 1);
  strcpy(p->data, "two");
  pgc_put(c, p);
  pgc_debug(c, 1);
  p = pgc_get(c, fd, 2048);
  pgc_debug(c, 1);
  strcpy(p->data, "three");
  pgc_put(c, p);
  pgc_debug(c, 1);
  pgc_flush(c);
  pgc_debug(c, 1);
  p = pgc_read(c, fd, 0);
  pgc_debug(c, 1);
  strcpy(p->data, "odin");
  pgc_mark_dirty(c, p);
  pgc_debug(c, 1);
  pgc_flush(c);
  pgc_debug(c, 1);
  q = pgc_read(c, fd, 1024);
  pgc_debug(c, 1);
  r = pgc_read(c, fd, 2048);
  pgc_debug(c, 1);
  pgc_put(c, p);
  pgc_put(c, q);
  pgc_put(c, r);
  pgc_debug(c, 1);
  p = pgc_get(c, fd, 3072);
  pgc_debug(c, 1);
  strcpy(p->data, "four");
  pgc_put(c, p);
  pgc_debug(c, 1);
  pgc_cleanup(c);
  pgc_debug(c, 1);
  pgc_close(c);
  return 0;
}

#endif
