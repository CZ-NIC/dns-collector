/*
 *	UCW Library -- Optimized Array Sorter
 *
 *	(c) 2003--2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/sorter/common.h"

#include <string.h>
#include <alloca.h>

#define ASORT_MIN_SHIFT 2

static void
asort_radix(struct asort_context *ctx, void *array, void *buffer, uns num_elts, uns hash_bits, uns swapped_output)
{
  // swap_output == 0 if result should be returned in `array', otherwise in `buffer'
  uns buckets = (1 << ctx->radix_bits);
  uns shift = (hash_bits > ctx->radix_bits) ? (hash_bits - ctx->radix_bits) : 0;
  uns cnt[buckets];

#if 0
  static int reported[64];
  if (!reported[hash_bits]++)
#endif
  DBG(">>> n=%d h=%d s=%d sw=%d", num_elts, hash_bits, shift, swapped_output);

  bzero(cnt, sizeof(cnt));
  ctx->radix_count(array, num_elts, cnt, shift);

  uns pos = 0;
  for (uns i=0; i<buckets; i++)
    {
      uns j = cnt[i];
      cnt[i] = pos;
      pos += j;
    }
  ASSERT(pos == num_elts);

  ctx->radix_split(array, buffer, num_elts, cnt, shift);
  pos = 0;
  for (uns i=0; i<buckets; i++)
    {
      uns n = cnt[i] - pos;
      if (n * cts->elt_size < sorter_radix_threshold || shift < ASORT_MIN_SHIFT)
	{
	  ctx->quicksort(buffer, n);
	  if (!swapped_output)
	    memcpy(array, buffer, n * ctx->elt_size);
	}
      else
	asort_radix(ctx, buffer, array, n, shift, !swapped_output);
      array += n * ctx->elt_size;
      buffer += n * ctx->elt_size;
      pos = cnt[i];
    }
}

#ifdef CONFIG_UCW_THREADS

#include "lib/threads.h"
#include "lib/workqueue.h"
#include "lib/eltpool.h"

static uns asort_threads_use_count;
static uns asort_threads_ready;
static struct worker_pool asort_thread_pool;

void
asort_start_threads(uns run)
{
  ucwlib_lock();
  asort_threads_use_count++;
  if (run && !asort_threads_ready)
    {
      SORT_XTRACE(2, "Initializing thread pool (%d threads)", sorter_threads);
      asort_thread_pool.num_threads = sorter_threads;
      worker_pool_init(&asort_thread_pool);
      asort_threads_ready = 1;
    }
  ucwlib_unlock();
}

void
asort_stop_threads(void)
{
  ucwlib_lock();
  if (!--asort_threads_use_count && asort_threads_ready)
    {
      SORT_XTRACE(2, "Shutting down thread pool");
      worker_pool_cleanup(&asort_thread_pool);
      asort_threads_ready = 0;
    }
  ucwlib_unlock();
}

struct qs_work {
  struct work w;
  struct asort_context *ctx;
  void *array;
  uns num_elts;
  int left, right;
#define LR_UNDEF -100
};

static void
qs_handle_work(struct worker_thread *thr UNUSED, struct work *ww)
{
  struct qs_work *w = (struct qs_work *) ww;

  DBG("Thread %d: got %d elts", thr->id, w->num_elts);
  if (w->num_elts * w->ctx->elt_size < sorter_thread_threshold)
    {
      w->ctx->quicksort(w->array, w->num_elts);
      w->left = w->right = LR_UNDEF;
    }
  else
    w->ctx->quicksplit(w->array, w->num_elts, &w->left, &w->right);
  DBG("Thread %d: returning l=%d r=%d", thr->id, w->left, w->right);
}

static struct qs_work *
qs_alloc_work(struct asort_context *ctx)
{
  struct qs_work *w = ep_alloc(ctx->eltpool);
  w->w.priority = 0;
  w->w.go = qs_handle_work;
  w->ctx = ctx;
  return w;
}

static void
threaded_quicksort(struct asort_context *ctx)
{
  struct work_queue q;
  struct qs_work *v, *w;

  asort_start_threads(1);
  work_queue_init(&asort_thread_pool, &q);
  ctx->eltpool = ep_new(sizeof(struct qs_work), 1000);

  w = qs_alloc_work(ctx);
  w->array = ctx->array;
  w->num_elts = ctx->num_elts;
  work_submit(&q, &w->w);

  while (v = (struct qs_work *) work_wait(&q))
    {
      if (v->left != LR_UNDEF)
	{
	  if (v->right > 0)
	    {
	      w = qs_alloc_work(ctx);
	      w->array = v->array;
	      w->num_elts = v->right + 1;
	      w->w.priority = v->w.priority + 1;
	      work_submit(&q, &w->w);
	    }
	  if (v->left < (int)v->num_elts - 1)
	    {
	      w = qs_alloc_work(ctx);
	      w->array = v->array + v->left * ctx->elt_size;
	      w->num_elts = v->num_elts - v->left;
	      w->w.priority = v->w.priority + 1;
	      work_submit(&q, &w->w);
	    }
	}
      ep_free(ctx->eltpool, v);
    }

  ep_delete(ctx->eltpool);
  work_queue_cleanup(&q);
  asort_stop_threads();
}

struct rs_work {
  struct work w;
  struct asort_context *ctx;
  void *in, *out;
  uns num_elts;
  uns shift;
  uns swap_output;
  uns cnt[0];
};

static void
rs_count(struct worker_thread *thr UNUSED, struct work *ww)
{
  struct rs_work *w = (struct rs_work *) ww;

  DBG("Thread %d: Counting %d items, shift=%d", thr->id, w->num_elts, w->shift);
  w->ctx->radix_count(w->in, w->num_elts, w->cnt, w->shift);
  DBG("Thread %d: Counting done", thr->id);
}

static void
rs_split(struct worker_thread *thr UNUSED, struct work *ww)
{
  struct rs_work *w = (struct rs_work *) ww;

  DBG("Thread %d: Splitting %d items, shift=%d", thr->id, w->num_elts, w->shift);
  w->ctx->radix_split(w->in, w->out, w->num_elts, w->cnt, w->shift);
  DBG("Thread %d: Splitting done", thr->id);
}

static void
rs_finish(struct worker_thread *thr UNUSED, struct work *ww)
{
  struct rs_work *w = (struct rs_work *) ww;

  if (thr)
    DBG("Thread %d: Finishing %d items, shift=%d", thr->id, w->num_elts, w->shift);
  if (w->shift < ASORT_MIN_SHIFT || w->num_elts * ctx->elt_size < sorter_radix_threshold)
    {
      w->ctx->quicksort(w->in, w->num_elts);
      if (w->swap_output)
	memcpy(w->out, w->in, w->num_elts * w->ctx->elt_size);
    }
  else
    asort_radix(w->ctx, w->in, w->out, w->num_elts, w->shift, w->swap_output);
  if (thr)
    DBG("Thread %d: Finishing done", thr->id);
}

static void
rs_wait_small(struct asort_context *ctx)
{
  struct rs_work *w;

  while (w = (struct rs_work *) work_wait(ctx->rs_work_queue))
    {
      DBG("Reaping small chunk of %d items", w->num_elts);
      ep_free(ctx->eltpool, w);
    }
}

static void
rs_radix(struct asort_context *ctx, void *array, void *buffer, uns num_elts, uns hash_bits, uns swapped_output)
{
  uns buckets = (1 << ctx->radix_bits);
  uns shift = (hash_bits > ctx->radix_bits) ? (hash_bits - ctx->radix_bits) : 0;
  uns cnt[buckets];
  uns blksize = num_elts / sorter_threads;
  DBG(">>> n=%d h=%d s=%d blk=%d sw=%d", num_elts, hash_bits, shift, blksize, swapped_output);

  // If there are any small chunks in progress, wait for them to finish
  rs_wait_small(ctx);

  // Start parallel counting
  void *iptr = array;
  for (uns i=0; i<sorter_threads; i++)
    {
      struct rs_work *w = ctx->rs_works[i];
      w->w.priority = 0;
      w->w.go = rs_count;
      w->ctx = ctx;
      w->in = iptr;
      w->out = buffer;
      w->num_elts = blksize;
      if (i == sorter_threads-1)
	w->num_elts += num_elts % sorter_threads;
      w->shift = shift;
      iptr += w->num_elts * ctx->elt_size;
      bzero(w->cnt, sizeof(uns) * buckets);
      work_submit(ctx->rs_work_queue, &w->w);
    }

  // Get bucket sizes from the counts
  bzero(cnt, sizeof(cnt));
  for (uns i=0; i<sorter_threads; i++)
    {
      struct rs_work *w = (struct rs_work *) work_wait(ctx->rs_work_queue);
      ASSERT(w);
      for (uns j=0; j<buckets; j++)
	cnt[j] += w->cnt[j];
    }

  // Calculate bucket starts
  uns pos = 0;
  for (uns i=0; i<buckets; i++)
    {
      uns j = cnt[i];
      cnt[i] = pos;
      pos += j;
    }
  ASSERT(pos == num_elts);

  // Start parallel splitting
  for (uns i=0; i<sorter_threads; i++)
    {
      struct rs_work *w = ctx->rs_works[i];
      w->w.go = rs_split;
      for (uns j=0; j<buckets; j++)
	{
	  uns k = w->cnt[j];
	  w->cnt[j] = cnt[j];
	  cnt[j] += k;
	}
      work_submit(ctx->rs_work_queue, &w->w);
    }
  ASSERT(cnt[buckets-1] == num_elts);

  // Wait for splits to finish
  while (work_wait(ctx->rs_work_queue))
    ;

  // Recurse on buckets
  pos = 0;
  for (uns i=0; i<buckets; i++)
    {
      uns n = cnt[i] - pos;
      if (!n)
	continue;
      if (n * ctx->elt_size < sorter_thread_threshold)
	{
	  struct rs_work *w = ep_alloc(ctx->eltpool);
	  w->w.priority = 0;
	  w->w.go = rs_finish;
	  w->ctx = ctx;
	  w->in = buffer;
	  w->out = array;
	  w->num_elts = n;
	  w->shift = shift;
	  w->swap_output = !swapped_output;
	  if (n * ctx->elt_size < sorter_thread_chunk)
	    {
	      DBG("Sorting block %d+%d inline", pos, n);
	      rs_finish(NULL, &w->w);
	      ep_free(ctx->eltpool, w);
	    }
	  else
	    {
	      DBG("Scheduling block %d+%d", pos, n);
	      work_submit(ctx->rs_work_queue, &w->w);
	    }
	}
      else
	rs_radix(ctx, buffer, array, n, shift, !swapped_output);
      pos = cnt[i];
      array += n * ctx->elt_size;
      buffer += n * ctx->elt_size;
    }
}

static void
threaded_radixsort(struct asort_context *ctx)
{
  struct work_queue q;

  asort_start_threads(1);
  work_queue_init(&asort_thread_pool, &q);

  // Prepare work structures for counting and splitting.
  // We use big_alloc(), because we want to avoid cacheline aliasing between threads.
  ctx->rs_work_queue = &q;
  ctx->rs_works = alloca(sizeof(struct rs_work *) * sorter_threads);
  for (uns i=0; i<sorter_threads; i++)
    ctx->rs_works[i] = big_alloc(sizeof(struct rs_work) + sizeof(uns) * (1 << ctx->radix_bits));

  // Prepare a pool for all remaining small bits which will be sorted on background.
  ctx->eltpool = ep_new(sizeof(struct rs_work), 1000);

  // Do the big splitting
  // FIXME: Set the swap bit carefully.
  rs_radix(ctx, ctx->array, ctx->buffer, ctx->num_elts, ctx->hash_bits, 0);
  for (uns i=0; i<sorter_threads; i++)
    big_free(ctx->rs_works[i], sizeof(struct rs_work) + sizeof(uns) * (1 << ctx->radix_bits));

  // Finish the small blocks
  rs_wait_small(ctx);

  ASSERT(!ctx->eltpool->num_allocated);
  ep_delete(ctx->eltpool);
  work_queue_cleanup(&q);
  asort_stop_threads();
}

#else

void asort_start_threads(uns run UNUSED) { }
void asort_stop_threads(void) { }

#endif

void
asort_run(struct asort_context *ctx)
{
  SORT_XTRACE(10, "Array-sorting %d items per %d bytes, hash_bits=%d", ctx->num_elts, ctx->elt_size, ctx->hash_bits);
  uns allow_threads UNUSED = (sorter_threads > 1 &&
			      ctx->num_elts * ctx->elt_size >= sorter_thread_threshold &&
			      !(sorter_debug & SORT_DEBUG_ASORT_NO_THREADS));

  if (ctx->num_elts * ctx->elt_size < sorter_radix_threshold ||
      ctx->hash_bits <= ASORT_MIN_SHIFT ||
      !ctx->radix_split ||
      (sorter_debug & SORT_DEBUG_ASORT_NO_RADIX))
    {
#ifdef CONFIG_UCW_THREADS
      if (allow_threads)
	{
	  SORT_XTRACE(12, "Decided to use parallel quicksort");
	  threaded_quicksort(ctx);
	  return;
	}
#endif
      SORT_XTRACE(12, "Decided to use sequential quicksort");
      ctx->quicksort(ctx->array, ctx->num_elts);
    }
  else
    {
#ifdef CONFIG_UCW_THREADS
      if (allow_threads)
	{
	  SORT_XTRACE(12, "Decided to use parallel radix-sort");
	  threaded_radixsort(ctx);
	  return;
	}
#endif
      SORT_XTRACE(12, "Decided to use sequential radix-sort");
      // FIXME: select dest buffer
      asort_radix(ctx, ctx->array, ctx->buffer, ctx->num_elts, ctx->hash_bits, 0);
    }

  SORT_XTRACE(11, "Array-sort finished");
}
