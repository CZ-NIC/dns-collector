/*
 *	UCW Library -- Memory Pools
 *
 *	(c) 1997--2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_POOLS_H
#define _UCW_POOLS_H

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
void *mp_alloc(struct mempool *, uns) LIKE_MALLOC;
void *mp_alloc_zero(struct mempool *, uns) LIKE_MALLOC;

static inline void * LIKE_MALLOC
mp_alloc_fast(struct mempool *p, uns l)
{
  byte *f = (void *) (((uintptr_t) p->free + POOL_ALIGN - 1) & ~(uintptr_t)(POOL_ALIGN - 1));
  byte *ee = f + l;
  if (ee > p->last)
    return mp_alloc(p, l);
  p->free = ee;
  return f;
}

static inline void * LIKE_MALLOC mp_alloc_fast_noalign(struct mempool *p, uns l)
{
  byte *f = p->free;
  byte *ee = f + l;
  if (ee > p->last)
    return mp_alloc(p, l);
  p->free = ee;
  return f;
}

static inline void *
mp_start_string(struct mempool *p, uns l)
{
  ASSERT(l <= p->chunk_size);
  return mp_alloc(p, l);
}

static inline void
mp_end_string(struct mempool *p, void *stop)
{
  p->free = stop;
}

/* mempool-str.c */

char *mp_strdup(struct mempool *, char *) LIKE_MALLOC;
void *mp_memdup(struct mempool *, void *, uns) LIKE_MALLOC;
char *mp_multicat(struct mempool *, ...) LIKE_MALLOC SENTINEL_CHECK;
static inline char * LIKE_MALLOC
mp_strcat(struct mempool *mp, char *x, char *y)
{
  return mp_multicat(mp, x, y, NULL);
}
char *mp_strjoin(struct mempool *p, char **a, uns n, uns sep) LIKE_MALLOC;

/* mempool-fmt.c */

char *mp_printf(struct mempool *p, char *fmt, ...) FORMAT_CHECK(printf,2,3) LIKE_MALLOC;
char *mp_vprintf(struct mempool *p, char *fmt, va_list args) LIKE_MALLOC;

#endif
