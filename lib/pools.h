/*
 *	Sherlock Library -- Memory Pools
 *
 *	(c) 1997--2001 Martin Mares <mj@ucw.cz>
 */

#ifndef POOL_ALIGN
#define POOL_ALIGN CPU_STRUCT_ALIGN
#endif

struct mempool {
  byte *free, *last;
  struct memchunk *first, *current, **plast;
  struct memchunk *first_large;
  uns chunk_size, threshold;
};

struct mempool *mp_new(uns);
void mp_delete(struct mempool *);
void mp_flush(struct mempool *);
void *mp_alloc(struct mempool *, uns);

static inline void *mp_alloc_fast(struct mempool *p, uns l)
{
  byte *f = (void *) (((uns) p->free + POOL_ALIGN - 1) & ~(POOL_ALIGN - 1));
  byte *ee = f + l;
  if (ee > p->last)
    return mp_alloc(p, l);
  p->free = ee;
  return f;
}

static inline void *mp_alloc_fast_noalign(struct mempool *p, uns l)
{
  byte *f = p->free;
  byte *ee = f + l;
  if (ee > p->last)
    return mp_alloc(p, l);
  p->free = ee;
  return f;
}
