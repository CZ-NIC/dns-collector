/*
 *	Sherlock Library -- Fast Database Management Routines
 *
 *	(c) 1999 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 */

/*
 *  This library uses the standard algorithm for external hashing (page directory
 *  mapping topmost K bits of hash value to page address, directory splits and
 *  so on). Peculiarities of this implementation (aka design decisions):
 *
 *   o	We allow both fixed and variable length keys and values (this includes
 *	zero size values for cases you want to represent only a set of keys).
 *   o	We assume that key_size + val_size < page_size.
 *   o	We never shrink the directory nor free empty pages. (The reason is that
 *	if the database was once large, it's likely it will again become large soon.)
 *   o	The only pages which can be freed are those of the directory (during
 *	directory split), so we keep only a simple 32-entry free block list
 *	and we assume it's sorted.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib.h"
#include "pagecache.h"
#include "db.h"
#include "db_internal.h"

struct sdbm *
sdbm_open(struct sdbm_options *o)
{
  struct sdbm *d;
  struct sdbm_root root, *r;
  uns cache_size = o->cache_size ? o->cache_size : 16;

  d = xmalloc(sizeof(struct sdbm));
  bzero(d, sizeof(*d));
  d->flags = o->flags;
  d->fd = open(o->name, ((d->flags & SDBM_WRITE) ? O_RDWR : O_RDONLY), 0666);
  if (d->fd >= 0)			/* Already exists, let's check it */
    {
      if (read(d->fd, &root, sizeof(root)) != sizeof(root))
	goto bad;
      if (root.magic != SDBM_MAGIC || root.version != SDBM_VERSION)
	goto bad;
      d->file_size = lseek(d->fd, 0, SEEK_END);
      d->page_size = 1 << root.page_order;
      d->cache = pgc_open(d->page_size, cache_size);
      d->root_page = pgc_read(d->cache, d->fd, 0);
      d->root = (void *) d->root_page->data;
    }
  else if ((d->flags & SDBM_CREAT) && (d->fd = open(o->name, O_RDWR | O_CREAT, 0666)) >= 0)
    {
      struct page *q;
      uns page_order = o->page_order;
      if (page_order < 10)
	page_order = 10;
      d->page_size = 1 << page_order;
      d->cache = pgc_open(d->page_size, cache_size);
      d->root_page = pgc_get_zero(d->cache, d->fd, 0);
      r = d->root = (void *) d->root_page->data;		 /* Build root page */
      r->magic = SDBM_MAGIC;
      r->version = SDBM_VERSION;
      r->page_order = page_order;
      r->key_size = o->key_size;
      r->val_size = o->val_size;
      r->dir_start = d->page_size;
      r->dir_order = 0;
      d->file_size = 3*d->page_size;
      q = pgc_get_zero(d->cache, d->fd, d->page_size);		/* Build page directory */
      ((u32 *)q->data)[0] = 2*d->page_size;
      pgc_put(d->cache, q);
      q = pgc_get_zero(d->cache, d->fd, 2*d->page_size);	/* Build single data page */
      pgc_put(d->cache, q);
    }
  else
    goto bad;
  d->dir_size = 1 << d->root->dir_order;
  d->dir_shift = 32 - d->root->dir_order;
  d->page_order = d->root->page_order;
  d->page_mask = d->page_size - 1;
  d->key_size = d->root->key_size;
  d->val_size = d->root->val_size;
  return d;

bad:
  sdbm_close(d);
  return NULL;
}

void
sdbm_close(struct sdbm *d)
{
  if (d->root_page)
    pgc_put(d->cache, d->root_page);
  if (d->cache)
    pgc_close(d->cache);
  if (d->fd >= 0)
    close(d->fd);
  free(d);
}

static uns
sdbm_alloc_pages(struct sdbm *d, uns number)
{
  uns where = d->file_size;
  d->file_size += number << d->page_order;
  return where;
}

static uns
sdbm_alloc_page(struct sdbm *d)
{
  uns pos;

  if (!d->root->free_pool[0].count)
    return sdbm_alloc_pages(d, 1);
  pos = d->root->free_pool[0].first;
  d->root->free_pool[0].first += d->page_size;
  if (!--d->root->free_pool[0].count)
    {
      memmove(d->root->free_pool, d->root->free_pool+1, SDBM_NUM_FREE_PAGE_POOLS * sizeof(d->root->free_pool[0]));
      d->root->free_pool[SDBM_NUM_FREE_PAGE_POOLS-1].count = 0;
    }
  pgc_mark_dirty(d->cache, d->root_page);
  return pos;
}

static void
sdbm_free_pages(struct sdbm *d, uns start, uns number)
{
  uns i = 0;

  while (d->root->free_pool[i].count)
    i++;
  d->root->free_pool[i].first = start;
  d->root->free_pool[i].count = number;
  pgc_mark_dirty(d->cache, d->root_page);
}

static u32
sdbm_hash(byte *key, uns keylen)
{
  /*
   *  This is the same hash function as GDBM uses.
   *  It seems to work well.
   */
  u32 value;
  uns index;

  /* Set the initial value from key. */
  value = 0x238F13AF * keylen;
  for (index = 0; index < keylen; index++)
    value = value + (key[index] << (index*5 % 24));
  return (1103515243 * value + 12345);
}

static int
sdbm_get_entry(struct sdbm *d, byte *pos, byte **key, uns *keylen, byte **val, uns *vallen)
{
  byte *p = pos;

  if (d->key_size >= 0)
    *keylen = d->key_size;
  else
    {
      *keylen = (p[0] << 8) | p[1];
      p += 2;
    }
  *key = p;
  p += *keylen;
  if (d->val_size >= 0)
    *vallen = d->val_size;
  else
    {
      *vallen = (p[0] << 8) | p[1];
      p += 2;
    }
  *val = p;
  p += *vallen;
  return p - pos;
}

static int
sdbm_entry_len(struct sdbm *d, uns keylen, uns vallen)
{
  uns len = keylen + vallen;
  if (d->key_size < 0)
    len += 2;
  if (d->val_size < 0)
    len += 2;
  return len;
}

static void
sdbm_store_entry(struct sdbm *d, byte *pos, byte *key, uns keylen, byte *val, uns vallen)
{
  if (d->key_size < 0)
    {
      *pos++ = keylen >> 8;
      *pos++ = keylen;
    }
  memmove(pos, key, keylen);
  pos += keylen;
  if (d->val_size < 0)
    {
      *pos++ = vallen >> 8;
      *pos++ = vallen;
    }
  memmove(pos, val, vallen);
}

static uns
sdbm_page_rank(struct sdbm *d, uns dirpos)
{
  struct page *b;
  u32 pg, x;
  uns l, r;
  uns pm = d->page_mask;

  b = pgc_read(d->cache, d->fd, d->root->dir_start + (dirpos & ~pm));
  pg = GET32(b->data, dirpos & pm);
  l = dirpos;
  while ((l & pm) && GET32(b->data, (l - 4) & pm) == pg)
    l -= 4;
  r = dirpos + 4;
  /* We heavily depend on unused directory entries being zero */
  while ((r & pm) && GET32(b->data, r & pm) == pg)
    r += 4;
  pgc_put(d->cache, b);

  if (!(l & pm) && !(r & pm))
    {
      /* Note that if it spans page boundary, it must contain an integer number of pages */
      while (l)
	{
	  b = pgc_read(d->cache, d->fd, d->root->dir_start + ((l - 4) & ~pm));
	  x = GET32(b->data, 0);
	  pgc_put(d->cache, b);
	  if (x != pg)
	    break;
	  l -= d->page_size;
	}
      while (r < 4*d->dir_size)
	{
	  b = pgc_read(d->cache, d->fd, d->root->dir_start + (r & ~pm));
	  x = GET32(b->data, 0);
	  pgc_put(d->cache, b);
	  if (x != pg)
	    break;
	  r += d->page_size;
	}
    }
  return (r - l) >> 2;
}

static void
sdbm_expand_directory(struct sdbm *d)
{
  struct page *b, *c;
  int i, ent;
  u32 *dir, *t;

  if (4*d->dir_size < d->page_size)
    {
      /* It still fits within single page */
      b = pgc_read(d->cache, d->fd, d->root->dir_start);
      dir = (u32 *) b->data;
      for(i=d->dir_size-1; i>=0; i--)
	dir[2*i] = dir[2*i+1] = dir[i];
      pgc_mark_dirty(d->cache, b);
      pgc_put(d->cache, b);
    }
  else
    {
      uns old_dir = d->root->dir_start;
      uns old_dir_pages = 1 << (d->root->dir_order + 2 - d->page_order);
      uns page, new_dir;
      new_dir = d->root->dir_start = sdbm_alloc_pages(d, 2*old_dir_pages);
      ent = 1 << (d->page_order - 3);
      for(page=0; page < old_dir_pages; page++)
	{
	  b = pgc_read(d->cache, d->fd, old_dir + (page << d->page_order));
	  dir = (u32 *) b->data;
	  c = pgc_get(d->cache, d->fd, new_dir + (page << (d->page_order + 1)));
	  t = (u32 *) c->data;
	  for(i=0; i<ent; i++)
	    t[2*i] = t[2*i+1] = dir[i];
	  pgc_put(d->cache, c);
	  c = pgc_get(d->cache, d->fd, new_dir + (page << (d->page_order + 1)) + d->page_size);
	  t = (u32 *) c->data;
	  for(i=0; i<ent; i++)
	    t[2*i] = t[2*i+1] = dir[ent+i];
	  pgc_put(d->cache, c);
	  pgc_put(d->cache, b);
	}
      if (!(d->flags & SDBM_FAST))
	{
	  /*
	   *  Unless in super-fast mode, fill old directory pages with zeroes.
	   *  This slows us down a bit, but allows database reconstruction after
	   *  the free list is lost.
	   */
	  for(page=0; page < old_dir_pages; page++)
	    {
	      b = pgc_get_zero(d->cache, d->fd, old_dir + (page << d->page_order));
	      pgc_put(d->cache, b);
	    }
	}
      sdbm_free_pages(d, old_dir, old_dir_pages);
    }

  d->root->dir_order++;
  d->dir_size = 1 << d->root->dir_order;
  d->dir_shift = 32 - d->root->dir_order;
  pgc_mark_dirty(d->cache, d->root_page);
  if (!(d->flags & SDBM_FAST))
    sdbm_sync(d);
}

static void
sdbm_split_data(struct sdbm *d, struct sdbm_bucket *s, struct sdbm_bucket *d0, struct sdbm_bucket *d1, uns sigbit)
{
  byte *sp = s->data;
  byte *dp[2] = { d0->data, d1->data };
  byte *K, *D;
  uns Kl, Dl, sz, i;

  while (sp < s->data + s->used)
    {
      sz = sdbm_get_entry(d, sp, &K, &Kl, &D, &Dl);
      sp += sz;
      i = (sdbm_hash(K, Kl) & (1 << sigbit)) ? 1 : 0;
      sdbm_store_entry(d, dp[i], K, Kl, D, Dl);
      dp[i] += sz;
    }
  d0->used = dp[0] - d0->data;
  d1->used = dp[1] - d1->data;
}

static void
sdbm_split_dir(struct sdbm *d, uns dirpos, uns count, uns pos)
{
  struct page *b;
  uns i;

  count *= 4;
  while (count)
    {
      b = pgc_read(d->cache, d->fd, d->root->dir_start + (dirpos & ~d->page_mask));
      i = d->page_size - (dirpos & d->page_mask);
      if (i > count)
	i = count;
      count -= i;
      while (i)
	{
	  GET32(b->data, dirpos & d->page_mask) = pos;
	  dirpos += 4;
	  i -= 4;
	}
      pgc_mark_dirty(d->cache, b);
      pgc_put(d->cache, b);
    }
}

static struct page *
sdbm_split_page(struct sdbm *d, struct page *b, u32 hash, uns dirpos)
{
  struct page *p[2];
  uns i, rank, sigbit, rank_log;

  rank = sdbm_page_rank(d, dirpos);	/* rank = # of pointers to this page */
  if (rank == 1)
    {
      sdbm_expand_directory(d);
      rank = 2;
      dirpos *= 2;
    }
  rank_log = 1;				/* rank_log = log2(rank) */
  while ((1U << rank_log) < rank)
    rank_log++;
  sigbit = d->dir_shift + rank_log - 1;	/* sigbit = bit we split on */
  p[0] = b;
  p[1] = pgc_get(d->cache, d->fd, sdbm_alloc_page(d));
  sdbm_split_data(d, (void *) b->data, (void *) p[0]->data, (void *) p[1]->data, sigbit);
  sdbm_split_dir(d, (dirpos & ~(4*rank - 1))+2*rank, rank/2, pgc_page_pos(d->cache, p[1]));
  pgc_mark_dirty(d->cache, p[0]);
  i = (hash & (1 << sigbit)) ? 1 : 0;
  pgc_put(d->cache, p[!i]);
  return p[i];
}

static int
sdbm_put_user(byte *D, uns Dl, byte *val, uns *vallen)
{
  if (vallen)
    {
      if (*vallen < Dl)
	return 1;
      *vallen = Dl;
    }
  if (val)
    memcpy(val, D, Dl);
  return 0;
}

static int
sdbm_access(struct sdbm *d, byte *key, uns keylen, byte *val, uns *vallen, uns mode)	/* 0=read, 1=store, 2=replace */
{
  struct page *p, *q;
  u32 hash, h, pos, size;
  struct sdbm_bucket *b;
  byte *c, *e;
  int rc;

  if ((d->key_size >= 0 && keylen != (uns) d->key_size) || keylen > 65535)
    return SDBM_ERROR_BAD_KEY_SIZE;
  if (val && ((d->val_size >= 0 && *vallen != (uns) d->val_size) || *vallen >= 65535) && mode)
    return SDBM_ERROR_BAD_VAL_SIZE;
  if (!mode && !(d->flags & SDBM_WRITE))
    return SDBM_ERROR_READ_ONLY;
  hash = sdbm_hash(key, keylen);
  if (d->dir_shift != 32)		/* avoid shifting by 32 bits */
    h = (hash >> d->dir_shift) << 2;	/* offset in the directory */
  else
    h = 0;
  p = pgc_read(d->cache, d->fd, d->root->dir_start + (h & ~d->page_mask));
  pos = GET32(p->data, h & d->page_mask);
  pgc_put(d->cache, p);
  q = pgc_read(d->cache, d->fd, pos);
  b = (void *) q->data;
  c = b->data;
  e = c + b->used;
  while (c < e)
    {
      byte *K, *D;
      uns Kl, Dl, s;
      s = sdbm_get_entry(d, c, &K, &Kl, &D, &Dl);
      if (Kl == keylen && !memcmp(K, key, Kl))
	{
	  /* Gotcha! */
	  switch (mode)
	    {
	    case 0:			/* fetch: found */
	      rc = sdbm_put_user(D, Dl, val, vallen);
	      pgc_put(d->cache, q);
	      return rc ? SDBM_ERROR_TOO_LARGE : 1;
	    case 1:			/* store: already present */
	      pgc_put(d->cache, q);
	      return 0;
	    default:			/* replace: delete the old one */
	      memmove(c, c+s, e-(c+s));
	      b->used -= s;
	      goto insert;
	    }
	}
      c += s;
    }
  if (!mode || !val)		/* fetch or delete: no success */
    {
      pgc_put(d->cache, q);
      return 0;
    }

insert:
  if (val)
    {
      size = sdbm_entry_len(d, keylen, *vallen);
      while (b->used + size > d->page_size - sizeof(struct sdbm_bucket))
	{
	  /* Page overflow, need to split */
	  if (size >= d->page_size - sizeof(struct sdbm_bucket))
	    {
	      pgc_put(d->cache, q);
	      return SDBM_ERROR_GIANT;
	    }
	  q = sdbm_split_page(d, q, hash, h);
	  b = (void *) q->data;
	}
      sdbm_store_entry(d, b->data + b->used, key, keylen, val, *vallen);
      b->used += size;
    }
  pgc_mark_dirty(d->cache, q);
  pgc_put(d->cache, q);
  if (d->flags & SDBM_SYNC)
    sdbm_sync(d);
  return 1;
}

int
sdbm_store(struct sdbm *d, byte *key, uns keylen, byte *val, uns vallen)
{
  return sdbm_access(d, key, keylen, val, &vallen, 1);
}

int
sdbm_replace(struct sdbm *d, byte *key, uns keylen, byte *val, uns vallen)
{
  return sdbm_access(d, key, keylen, val, &vallen, 2);
}

int
sdbm_delete(struct sdbm *d, byte *key, uns keylen)
{
  return sdbm_access(d, key, keylen, NULL, NULL, 2);
}

int
sdbm_fetch(struct sdbm *d, byte *key, uns keylen, byte *val, uns *vallen)
{
  return sdbm_access(d, key, keylen, val, vallen, 0);
}

void
sdbm_rewind(struct sdbm *d)
{
  d->find_pos = d->page_size;
  d->find_free_list = 0;
}

int
sdbm_get_next(struct sdbm *d, byte *key, uns *keylen, byte *val, uns *vallen)
{
  uns pos = d->find_pos;
  byte *K, *V;
  uns c, Kl, Vl;
  struct page *p;
  struct sdbm_bucket *b;

  for(;;)
    {
      c = pos & d->page_mask;
      if (!c)
	{
	  if (pos >= d->file_size)
	    break;
	  if (pos == d->root->dir_start)
	    pos += (4*d->dir_size + d->page_size - 1) & ~d->page_mask;
	  else if (pos == d->root->free_pool[d->find_free_list].first)
	    pos += d->root->free_pool[d->find_free_list++].count << d->page_order;
	  else
	    pos += 4;
	  continue;
	}
      p = pgc_read(d->cache, d->fd, pos & ~d->page_mask);
      b = (void *) p->data;
      if (c - 4 >= b->used)
	{
	  pos = (pos & ~d->page_mask) + d->page_size;
	  pgc_put(d->cache, p);
	  continue;
	}
      c = sdbm_get_entry(d, p->data + c, &K, &Kl, &V, &Vl);
      d->find_pos = pos + c;
      c = sdbm_put_user(K, Kl, key, keylen) ||
	  sdbm_put_user(V, Vl, val, vallen);
      pgc_put(d->cache, p);
      return c ? SDBM_ERROR_TOO_LARGE : 1;
    }
  d->find_pos = pos;
  return 0;
}

void
sdbm_sync(struct sdbm *d)
{
  pgc_flush(d->cache);
  if (d->flags & SDBM_FSYNC)
    fsync(d->fd);
}
