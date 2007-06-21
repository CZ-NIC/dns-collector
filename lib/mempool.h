/*
 *	UCW Library -- Memory Pools
 *
 *	(c) 1997--2005 Martin Mares <mj@ucw.cz>
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_POOLS_H
#define _UCW_POOLS_H

/* Memory pool state (see mp_push(), ...) */
struct mempool_state {
  uns free[2];
  void *last[2];
  struct mempool_state *next;
};

/* Memory pool */
struct mempool {
  struct mempool_state state;
  void *unused, *last_big;
  uns chunk_size, threshold, idx;
};

/* Statistics (see mp_stats()) */
struct mempool_stats {
  uns total_size;			/* Real allocated size in bytes */
  uns chain_count[3];			/* Number of allocated chunks in small/big/unused chains */
  uns chain_size[3];			/* Size of allocated chunks in small/big/unused chains */
};

/* Initialize a given mempool structure. Chunk size must be in the interval [1, UINT_MAX / 2] */
void mp_init(struct mempool *pool, uns chunk_size);

/* Allocate and initialize a new memory pool. See mp_init for chunk size limitations. */
struct mempool *mp_new(uns chunk_size);

/* Cleanup mempool initialized by mp_init or mp_new */
void mp_delete(struct mempool *pool);

/* Free all data on a memory pool (saves some empty chunks for later allocations) */
void mp_flush(struct mempool *pool);

/* Compute some statistics for debug purposes. See the definition of the mempool_stats structure. */
void mp_stats(struct mempool *pool, struct mempool_stats *stats);


/*** Allocation routines ***/

/* For internal use only, do not call directly */
void *mp_alloc_internal(struct mempool *pool, uns size) LIKE_MALLOC;

/* The function allocates new <size> bytes on a given memory pool.
 * If the <size> is zero, the resulting pointer is undefined,
 * but it may be safely reallocated or used as the parameter
 * to other functions below.
 *
 * The resulting pointer is always aligned to a multiple of
 * CPU_STRUCT_ALIGN bytes and this condition remains true also
 * after future reallocations.
 */
void *mp_alloc(struct mempool *pool, uns size);

/* The same as mp_alloc, but the result may not be aligned */
void *mp_alloc_noalign(struct mempool *pool, uns size);

/* The same as mp_alloc, but fills the newly allocated data with zeroes */
void *mp_alloc_zero(struct mempool *pool, uns size);

/* Inlined version of mp_alloc() */
static inline void *
mp_alloc_fast(struct mempool *pool, uns size)
{
  uns avail = pool->state.free[0] & ~(CPU_STRUCT_ALIGN - 1);
  if (size <= avail)
    {
      pool->state.free[0] = avail - size;
      return pool->state.last[0] - avail;
    }
  else
    return mp_alloc_internal(pool, size);
}

/* Inlined version of mp_alloc_noalign() */
static inline void *
mp_alloc_fast_noalign(struct mempool *pool, uns size)
{
  if (size <= pool->state.free[0])
    {
      void *ptr = pool->state.last[0] - pool->state.free[0];
      pool->state.free[0] -= size;
      return ptr;
    }
  else
    return mp_alloc_internal(pool, size);
}


/*** Usage as a growing buffer ***/

/* For internal use only, do not call directly */
void *mp_start_internal(struct mempool *pool, uns size) LIKE_MALLOC;
void *mp_grow_internal(struct mempool *pool, uns size);
void *mp_spread_internal(struct mempool *pool, void *p, uns size);

static inline uns
mp_idx(struct mempool *pool, void *ptr)
{
  return ptr == pool->last_big;
}

/* Open a new growing buffer (at least <size> bytes long).
 * If the <size> is zero, the resulting pointer is undefined,
 * but it may be safely reallocated or used as the parameter
 * to other functions below.
 *
 * The resulting pointer is always aligned to a multiple of
 * CPU_STRUCT_ALIGN bytes and this condition remains true also
 * after future reallocations. There is an unaligned version as well.
 *
 * Keep in mind that you can't make any other <pool> allocations
 * before you "close" the growing buffer with mp_end().
 */
void *mp_start(struct mempool *pool, uns size);
void *mp_start_noalign(struct mempool *pool, uns size);

/* Inlined version of mp_start() */
static inline void *
mp_start_fast(struct mempool *pool, uns size)
{
  uns avail = pool->state.free[0] & ~(CPU_STRUCT_ALIGN - 1);
  if (size <= avail)
    {
      pool->idx = 0;
      pool->state.free[0] = avail;
      return pool->state.last[0] - avail;
    }
  else
    return mp_start_internal(pool, size);
}

/* Inlined version of mp_start_noalign() */
static inline void *
mp_start_fast_noalign(struct mempool *pool, uns size)
{
  if (size <= pool->state.free[0])
    {
      pool->idx = 0;
      return pool->state.last[0] - pool->state.free[0];
    }
  else
    return mp_start_internal(pool, size);
}

/* Return start pointer of the growing buffer allocated by mp_start() or a similar function */
static inline void *
mp_ptr(struct mempool *pool)
{
  return pool->state.last[pool->idx] - pool->state.free[pool->idx];
}

/* Return the number of bytes available for extending the growing buffer */
static inline uns
mp_avail(struct mempool *pool)
{
  return pool->state.free[pool->idx];
}

/* Grow the buffer allocated by mp_start() to be at least <size> bytes long
 * (<size> may be less than mp_avail(), even zero). Reallocated buffer may
 * change its starting position. The content will be unchanged to the minimum
 * of the old and new sizes; newly allocated memory will be uninitialized.
 * Multiple calls to mp_grow have amortized linear cost wrt. the maximum value of <size>. */
static inline void *
mp_grow(struct mempool *pool, uns size)
{
  return (size <= mp_avail(pool)) ? mp_ptr(pool) : mp_grow_internal(pool, size);
}

/* Grow the buffer by at least one byte -- equivalent to mp_grow(pool, mp_avail(pool) + 1) */
static inline void *
mp_expand(struct mempool *pool)
{
  return mp_grow_internal(pool, mp_avail(pool) + 1);
}

/* Ensure that there is at least <size> bytes free after <p>, if not, reallocate and adjust <p>. */
static inline void *
mp_spread(struct mempool *pool, void *p, uns size)
{
  return (((uns)(pool->state.last[pool->idx] - p) >= size) ? p : mp_spread_internal(pool, p, size));
}

/* Close the growing buffer. The <end> must point just behind the data, you want to keep
 * allocated (so it can be in the interval [mp_ptr(pool), mp_ptr(pool) + mp_avail(pool)]).
 * Returns a pointer to the beginning of the just closed block. */
static inline void *
mp_end(struct mempool *pool, void *end)
{
  void *p = mp_ptr(pool);
  pool->state.free[pool->idx] = pool->state.last[pool->idx] - end;
  return p;
}

/* Return size in bytes of the last allocated memory block (with mp_alloc*() or mp_end()). */
static inline uns
mp_size(struct mempool *pool, void *ptr)
{
  uns idx = mp_idx(pool, ptr);
  return pool->state.last[idx] - ptr - pool->state.free[idx];
}

/* Open the last memory block (allocated with mp_alloc*() or mp_end())
 * for growing and return its size in bytes. The contents and the start pointer
 * remain unchanged. Do not forget to call mp_end() to close it. */
uns mp_open(struct mempool *pool, void *ptr);

/* Inlined version of mp_open() */
static inline uns
mp_open_fast(struct mempool *pool, void *ptr)
{
  pool->idx = mp_idx(pool, ptr);
  uns size = pool->state.last[pool->idx] - ptr - pool->state.free[pool->idx];
  pool->state.free[pool->idx] += size;
  return size;
}

/* Reallocate the last memory block (allocated with mp_alloc*() or mp_end())
 * to the new <size>. Behavior is similar to mp_grow(), but the resulting
 * block is closed. */
void *mp_realloc(struct mempool *pool, void *ptr, uns size);

/* The same as mp_realloc(), but fills the additional bytes (if any) with zeroes */
void *mp_realloc_zero(struct mempool *pool, void *ptr, uns size);

/* Inlined version of mp_realloc() */
static inline void *
mp_realloc_fast(struct mempool *pool, void *ptr, uns size)
{
  mp_open_fast(pool, ptr);
  ptr = mp_grow(pool, size);
  mp_end(pool, ptr + size);
  return ptr;
}


/*** Usage as a stack ***/

/* Save the current state of a memory pool.
 * Do not call this function with an opened growing buffer. */
static inline void
mp_save(struct mempool *pool, struct mempool_state *state)
{
  *state = pool->state;
  pool->state.next = state;
}

/* Save the current state to a newly allocated mempool_state structure.
 * Do not call this function with an opened growing buffer. */
struct mempool_state *mp_push(struct mempool *pool);

/* Restore the state saved by mp_save() or mp_push() and free all
 * data allocated after that point (including the state structure itself).
 * You can't reallocate the last memory block from the saved state. */
void mp_restore(struct mempool *pool, struct mempool_state *state);

/* Restore the state saved by the last call to mp_push().
 * mp_pop() and mp_push() works as a stack so you can push more states safely. */
void mp_pop(struct mempool *pool);


/*** mempool-str.c ***/

char *mp_strdup(struct mempool *, const char *) LIKE_MALLOC;
void *mp_memdup(struct mempool *, const void *, uns) LIKE_MALLOC;
char *mp_multicat(struct mempool *, ...) LIKE_MALLOC SENTINEL_CHECK;
static inline char * LIKE_MALLOC
mp_strcat(struct mempool *mp, const char *x, const char *y)
{
  return mp_multicat(mp, x, y, NULL);
}
char *mp_strjoin(struct mempool *p, char **a, uns n, uns sep) LIKE_MALLOC;


/*** mempool-fmt.c ***/

char *mp_printf(struct mempool *mp, const char *fmt, ...) FORMAT_CHECK(printf,2,3) LIKE_MALLOC;
char *mp_vprintf(struct mempool *mp, const char *fmt, va_list args) LIKE_MALLOC;
char *mp_printf_append(struct mempool *mp, char *ptr, const char *fmt, ...) FORMAT_CHECK(printf,3,4);
char *mp_vprintf_append(struct mempool *mp, char *ptr, const char *fmt, va_list args);

#endif
