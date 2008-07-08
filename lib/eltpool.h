/*
 *	UCW Library -- Fast Allocator for Fixed-Size Elements
 *
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_ELTPOOL_H
#define _UCW_ELTPOOL_H

struct eltpool {
  struct eltpool_chunk *first_chunk;
  struct eltpool_free *first_free;
  uns elt_size;
  uns chunk_size;
  uns elts_per_chunk;
  uns num_allocated;		// Just for debugging
  uns num_chunks;
};

struct eltpool_chunk {
  struct eltpool_chunk *next;
  /* Chunk data continue here */
};

struct eltpool_free {
  struct eltpool_free *next;
};

struct eltpool *ep_new(uns elt_size, uns elts_per_chunk);
void ep_delete(struct eltpool *pool);
void *ep_alloc_slow(struct eltpool *pool);
u64 ep_total_size(struct eltpool *pool);

static inline void *
ep_alloc(struct eltpool *pool)
{
  pool->num_allocated++;
#ifdef CONFIG_FAKE_ELTPOOL
  return xmalloc(pool->elt_size);
#else
  struct eltpool_free *elt;
  if (elt = pool->first_free)
    pool->first_free = elt->next;
  else
    elt = ep_alloc_slow(pool);
  return elt;
#endif
}

static inline void
ep_free(struct eltpool *pool, void *p)
{
  pool->num_allocated--;
#ifdef CONFIG_FAKE_ELTPOOL
  (void) pool;
  xfree(p);
#else
  struct eltpool_free *elt = p;
  elt->next = pool->first_free;
  pool->first_free = elt;
#endif
}

#endif
