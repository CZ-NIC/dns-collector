/*
 *	Sherlock Library -- Memory Pools
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#ifndef POOL_ALIGN
#define POOL_ALIGN CPU_STRUCT_ALIGN
#endif

struct mempool {
  struct memchunk *chunks;
  byte *free, *last;
  uns chunk_size, threshold;
};

struct mempool *new_pool(uns);
void free_pool(struct mempool *);
void *pool_alloc(struct mempool *, uns);

extern inline void *fast_alloc(struct mempool *p, uns l)
{
  void *f = (void *) (((uns) p->free + POOL_ALIGN - 1) & ~(POOL_ALIGN - 1));
  byte *ee = f + l;
  if (ee > p->last)
    return pool_alloc(p, l);
  p->free = ee;
  return f;
}

extern inline void *fast_alloc_noalign(struct mempool *p, uns l)
{
  void *f = p->free;
  byte *ee = f + l;
  if (ee > p->last)
    return pool_alloc(p, l);
  p->free = ee;
  return f;
}
