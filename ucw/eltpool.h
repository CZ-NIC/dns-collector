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

#ifdef CONFIG_UCW_CLEAN_ABI
#define ep_alloc_slow ucw_ep_alloc_slow
#define ep_delete ucw_ep_delete
#define ep_new ucw_ep_new
#define ep_total_size ucw_ep_total_size
#endif

/***
 * [[defs]]
 * Definitions
 * -----------
 ***/

/**
 * Memory pool of fixed-sized elements.
 * You should use this one as an opaque handle only, the insides are internal.
 **/
struct eltpool {
  struct eltpool_chunk *first_chunk;
  struct eltpool_free *first_free;
  uint elt_size;
  uint chunk_size;
  uint elts_per_chunk;
  uint num_allocated;		// Just for debugging
  uint num_chunks;
};

struct eltpool_chunk {
  struct eltpool_chunk *next;
  /* Chunk data continue here */
};

struct eltpool_free {
  struct eltpool_free *next;
};

/***
 * [[basic]]
 * Basic manipulation
 * ------------------
 ***/

/**
 * Create a new memory pool for elements of @elt_size bytes.
 * The pool will allocate chunks of at least @elts_per_chunk elements.
 * Higher numbers lead to better allocation times but also to bigger
 * unused memory blocks. Call @ep_delete() to free all pool's resources.
 *
 * Element pools can be treated as <<trans:respools,resources>>, see <<trans:res_eltpool()>>.
 **/
struct eltpool *ep_new(uint elt_size, uint elts_per_chunk);

/**
 * Release a memory pool created by @ep_new() including all
 * elements allocated from that pool.
 **/
void ep_delete(struct eltpool *pool);

/**
 * Return the total number of bytes allocated by a given
 * memory pool including all internals.
 **/
u64 ep_total_size(struct eltpool *pool);

/***
 * [[alloc]]
 * Allocation routines
 * -------------------
 ***/

void *ep_alloc_slow(struct eltpool *pool); /* Internal. Do not call directly. */
/**
 * Allocate a new element on a given memory pool.
 * The results is always aligned to a multiple of the element's size.
 **/
static inline void *ep_alloc(struct eltpool *pool)
{
  pool->num_allocated++;
#ifdef CONFIG_UCW_FAKE_ELTPOOL
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

/**
 * Release an element previously allocated by @ep_alloc().
 * Note thet the memory is not really freed (until @mp_delete()),
 * but it can be reused by future @ep_alloc()'s.
 **/
static inline void ep_free(struct eltpool *pool, void *p)
{
  pool->num_allocated--;
#ifdef CONFIG_UCW_FAKE_ELTPOOL
  (void) pool;
  xfree(p);
#else
  struct eltpool_free *elt = p;
  elt->next = pool->first_free;
  pool->first_free = elt;
#endif
}

#endif
