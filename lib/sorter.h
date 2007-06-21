/*
 *	UCW Library -- Universal Sorter
 *
 *	(c) 2001--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 *  This is not a normal header file, it's a generator of sorting
 *  routines.  Each time you include it with parameters set in the
 *  corresponding preprocessor macros, it generates a file sorter
 *  with the parameters given.
 *
 *  Recognized parameter macros: (those marked with [*] are mandatory)
 *
 *  SORT_KEY	    [*]	data type capable of storing a single key
 *  SORT_PREFIX(x)  [*] add a name prefix (used on all global names
 *			defined by the sorter)
 *  SORT_PRESORT	include an in-core pre-sorting pass. Beware, when in
 *			the pre-sorting mode, it's quite possible that the
 *			comparison function will be called with both arguments
 *			identical.
 *  SORT_UP_TO		a C expression, if defined, sorting is stopped after the
 *			average run length in the file exceeds the value of this
 *			expression (in bytes)
 *  SORT_UNIFY		merge items with identical keys
 *  SORT_UNIQUE		all items have distinct keys (checked in debug mode)
 *  SORT_REGULAR	all items are equally long and they don't contain
 *			anything else than the key. In this case, the sorter
 *			automatically supplies fetch_key, copy_data, fetch_item
 *			and store_item functions. Item size is also expected
 *			to be small.
 *  SORT_DELETE_INPUT	a C expression, if true, the input files are
 *			deleted as soon as possible
 *  SORT_INPUT_FILE	input is a file with this name
 *  SORT_INPUT_FB	input is a fastbuf stream
 *			(can be safely NULL if you want to treat original
 *			input in a different way by file read functions)
 *  SORT_INPUT_FBPAIR	input is a pair of fastbuf streams
 *			(not supported by the presorter)
 *  SORT_OUTPUT_FILE	output is a file with this name
 *  SORT_OUTPUT_FB	output is a temporary fastbuf stream
 *
 *  You also need to define some (usually inline) functions which
 *  are called by the sorter to process your data:
 *
 *  int PREFIX_compare(SORT_KEY *a, *b)
 *			compare two keys, result like strcmp
 *  int PREFIX_fetch_key(struct fastbuf *f, SORT_KEY *k)
 *			fetch next key, returns nonzero=ok, 0=eof
 *  void PREFIX_copy_data(struct fastbuf *src, *dest, SORT_KEY *k)
 *			write just fetched key k to dest and copy all data
 *			belonging to this key from src to dest.
 *  void PREFIX_merge_data(struct fastbuf *src1, *src2, *dest, SORT_KEY *k1, *k2)
 *			[used only in case SORT_UNIFY is defined]
 *			write just fetched key k to dest and merge data from
 *			two records with the same key (k1 and k2 are key occurences
 *			in the corresponding streams).
 *  byte * PREFIX_fetch_item(struct fastbuf *f, SORT_KEY *k, byte *limit)
 *			[used only with SORT_PRESORT]
 *			fetch data belonging to a just fetched key and store
 *			them to memory following the key, but not over limit.
 *			Returns a pointer to first byte after the data
 *			or NULL if the data don't fit. For variable-length
 *			keys, it can use the rest of SORT_KEY and even return
 *			pointer before end of the key.
 *			Important: before PREFIX_fetch_item() succeeds, the key
 *			must be position independent, the sorter can copy it.
 *  void PREFIX_store_item(struct fastbuf *f, SORT_KEY *k)
 *			[used only with SORT_PRESORT]
 *			write key and all its data read with PREFIX_fetch_data
 *			to the stream given.
 *  SORT_KEY * PREFIX_merge_items(SORT_KEY *a, SORT_KEY *b)
 *			[used only with SORT_PRESORT && SORT_UNIFY]
 *			merge two items with the same key, returns pointer
 *			to at most one of the items, the rest will be removed
 *			from the list of items, but not deallocated, so
 *			the remaining item can freely reference data of the
 *			other one.
 *
 *  After including this file, all parameter macros are automatically
 *  undef'd.
 */

#include "lib/sorter-globals.h"
#include "lib/fastbuf.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#if !defined(SORT_KEY) || !defined(SORT_PREFIX)
#error Some of the mandatory configuration macros are missing.
#endif

#define P(x) SORT_PREFIX(x)
#define SWAP(x,y,z) do { z=x; x=y; y=z; } while(0)

#if defined(SORT_UNIFY) || defined(SORT_UNIQUE)
#define LESS <
#else
#define LESS <=
#endif

#if defined(SORT_UNIQUE) && defined(DEBUG_ASSERTS)
#define SORT_ASSERT_UNIQUE
#endif

#ifdef SORT_REGULAR

static inline int
P(fetch_key)(struct fastbuf *in, SORT_KEY *x)
{
  return breadb(in, x, sizeof(*x));
}

static inline void
P(copy_data)(struct fastbuf *in UNUSED, struct fastbuf *out, SORT_KEY *x)
{
  bwrite(out, x, sizeof(*x));
}

static inline byte *
P(fetch_item)(struct fastbuf *in UNUSED, SORT_KEY *x UNUSED, byte *limit UNUSED)
{
  return (byte *)(x+1);
}

static inline void
P(store_item)(struct fastbuf *out, SORT_KEY *x)
{
  bwrite(out, x, sizeof(*x));
}

#endif

static struct fastbuf *
P(flush_out)(struct fastbuf *out)
{
  if (out)
    brewind(out);
  return out;
}

static uns
P(pass)(struct fastbuf **fb1, struct fastbuf **fb2
#ifdef SORT_UP_TO
    , uns stop_sorting
#endif
)
{
  struct fastbuf *in1 = *fb1;
  struct fastbuf *in2 = *fb2;
  struct fastbuf *out1 = NULL;
  struct fastbuf *out2 = NULL;
  SORT_KEY kbuf1, kbuf2, kbuf3, kbuf4;
  SORT_KEY *kin1 = &kbuf1;
  SORT_KEY *kprev1 = &kbuf2;
  SORT_KEY *kin2 = &kbuf3;
  SORT_KEY *kprev2 = &kbuf4;
  SORT_KEY *kout = NULL;
  SORT_KEY *ktmp;
  int next1, next2, comp;
  int run1, run2;
  uns run_count = 0;

  run1 = next1 = in1 ? P(fetch_key)(in1, kin1) : 0;
  run2 = next2 = in2 ? P(fetch_key)(in2, kin2) : 0;
  while (next1 || next2)
    {
      if (!run1)
	comp = 1;
      else if (!run2)
	comp = -1;
      else
	comp = P(compare)(kin1, kin2);
      ktmp = (comp <= 0) ? kin1 : kin2;
      if (!kout || !(P(compare)(kout, ktmp) LESS 0))
	{
	  struct fastbuf *t;
#ifdef SORT_UP_TO
	  if (!stop_sorting)
#endif
	    SWAP(out1, out2, t);
	  if (!out1)
	    out1 = bopen_tmp(sorter_stream_bufsize);
	  run_count++;
	}
      if (comp LESS 0)
	{
	  P(copy_data)(in1, out1, kin1);
	  SWAP(kin1, kprev1, ktmp);
	  next1 = P(fetch_key)(in1, kin1);
	  run1 = next1 && (P(compare)(kprev1, kin1) LESS 0);
	  kout = kprev1;
	}
#ifdef SORT_UNIFY
      else if (comp == 0)
	{
	  P(merge_data)(in1, in2, out1, kin1, kin2);
	  SWAP(kin1, kprev1, ktmp);
	  next1 = P(fetch_key)(in1, kin1);
	  run1 = next1 && (P(compare)(kprev1, kin1) LESS 0);
	  SWAP(kin2, kprev2, ktmp);
	  next2 = P(fetch_key)(in2, kin2);
	  run2 = next2 && (P(compare)(kprev2, kin2) LESS 0);
	  kout = kprev2;
	}
#endif
#ifdef SORT_ASSERT_UNIQUE
      else if (unlikely(comp == 0))
	ASSERT(0);
#endif
      else
	{
	  P(copy_data)(in2, out1, kin2);
	  SWAP(kin2, kprev2, ktmp);
	  next2 = P(fetch_key)(in2, kin2);
	  run2 = next2 && (P(compare)(kprev2, kin2) LESS 0);
	  kout = kprev2;
	}
      if (!run1 && !run2)
	{
	  run1 = next1;
	  run2 = next2;
	}
    }
  bclose(in1);
  bclose(in2);
  if (sorter_trace)
    msg(L_INFO, "Pass %d: %d runs, %d+%d KB", sorter_pass_counter, run_count,
	(out1 ? (int)((btell(out1) + 1023) / 1024) : 0),
	(out2 ? (int)((btell(out2) + 1023) / 1024) : 0));
  *fb1 = P(flush_out)(out1);
  *fb2 = P(flush_out)(out2);
  sorter_pass_counter++;
  return run_count;
}

#ifdef SORT_PRESORT

#if defined(SORT_REGULAR) && !defined(SORT_UNIFY)

/* If we are doing a simple sort on a regular file, we can use a faster presorting strategy */

static SORT_KEY *P(array);

#define ASORT_PREFIX(x) SORT_PREFIX(x##_array)
#define ASORT_KEY_TYPE SORT_KEY
#define ASORT_ELT(i) P(array)[i]
#define ASORT_LT(x,y) (P(compare)(&(x),&(y)) < 0)

#include "lib/arraysort.h"

static void
P(presort)(struct fastbuf **fb1, struct fastbuf **fb2)
{
  struct fastbuf *in = *fb1;
  struct fastbuf *out1 = NULL;
  struct fastbuf *out2 = NULL;
  struct fastbuf *tbuf;
  uns buf_items = sorter_presort_bufsize / sizeof(SORT_KEY);
  uns run_count = 0;
  SORT_KEY last_out = { }, *array;

  ASSERT(!*fb2);
  if (buf_items < 2)
    die("PresortBuffer set too low");
  P(array) = array = xmalloc(buf_items * sizeof(SORT_KEY));

  for(;;)
    {
      uns s = bread(in, array, buf_items * sizeof(SORT_KEY));
      uns n = s / sizeof(SORT_KEY);
      ASSERT(!(s % sizeof(SORT_KEY)));
      if (!n)
	break;
      P(sort_array)(n);
#ifdef SORT_ASSERT_UNIQUE
      for (uns i=0; i<n-1; i++)
	if (unlikely(P(compare)(&array[i], &array[i+1]) >= 0))
	  ASSERT(0);
      ASSERT(!run_count || P(compare)(&last_out, &array[0]));
#endif
      if (!run_count || P(compare)(&last_out, &array[0]) > 0)
	{
	  run_count++;
#ifdef SORT_UP_TO
	  if (sorter_presort_bufsize < (uns) SORT_UP_TO)
#endif
	    SWAP(out1, out2, tbuf);
	  if (!out1)
	    out1 = bopen_tmp(sorter_stream_bufsize);
	}
      last_out = array[n-1];
      bwrite(out1, array, n * sizeof(SORT_KEY));
    }

  bclose(in);
  if (sorter_trace)
    msg(L_INFO, "Pass 0: %d runs, %d+%d KB",
	run_count,
	(out1 ? (int)((btell(out1) + 1023) / 1024) : 0),
	(out2 ? (int)((btell(out2) + 1023) / 1024) : 0));
  *fb1 = P(flush_out)(out1);
  *fb2 = P(flush_out)(out2);
  xfree(array);
}

#else

#define SORT_NODE struct P(presort_node)

SORT_NODE {
  SORT_NODE *next;
  SORT_KEY key;
};

static SORT_NODE *
P(mergesort)(SORT_NODE *x)
{
  SORT_NODE *f1, **l1, *f2, **l2, **l;

  l1 = &f1;
  l2 = &f2;
  while (x)
    {
      *l1 = x;
      l1 = &x->next;
      x = x->next;
      if (!x)
	break;
      *l2 = x;
      l2 = &x->next;
      x = x->next;
    }
  *l1 = *l2 = NULL;

  if (f1 && f1->next)
    f1 = P(mergesort)(f1);
  if (f2 && f2->next)
    f2 = P(mergesort)(f2);
  l = &x;
  while (f1 && f2)
    {
      if (P(compare)(&f1->key, &f2->key) <= 0)
	{
	  *l = f1;
	  l = &f1->next;
	  f1 = f1->next;
	}
      else
	{
	  *l = f2;
	  l = &f2->next;
	  f2 = f2->next;
	}
    }
  *l = f1 ? : f2;
  return x;
}

static void
P(presort)(struct fastbuf **fb1, struct fastbuf **fb2)
{
  struct fastbuf *in = *fb1;
  struct fastbuf *out1 = NULL;
  struct fastbuf *out2 = NULL;
  struct fastbuf *tbuf;
  byte *buffer, *bufend, *current;
  SORT_NODE *first, **last, *this, *leftover;
  int cont = 1;
  uns run_count = 0;
  uns giant_count = 0;
  uns split_count = 0;

  ASSERT(!*fb2);
  if (sorter_presort_bufsize < 2*sizeof(SORT_NODE))
    die("PresortBuffer set too low");
  buffer = xmalloc(sorter_presort_bufsize);
  bufend = buffer + sorter_presort_bufsize;
  leftover = NULL;
  while (cont)
    {
#ifdef SORT_UP_TO
      if (sorter_presort_bufsize < SORT_UP_TO)
#endif
	SWAP(out1, out2, tbuf);
      if (!out1)
	out1 = bopen_tmp(sorter_stream_bufsize);
      current = buffer;
      last = &first;
      if (leftover)
	{
	  memmove(buffer, leftover, sizeof(SORT_NODE));
	  this = leftover = (SORT_NODE *) buffer;
	  split_count++;
	  goto get_data;
	}
      for(;;)
	{
	  current = (byte *) ALIGN_TO((uintptr_t) current, CPU_STRUCT_ALIGN);
	  if (current + sizeof(*this) > bufend)
	    break;
	  this = (SORT_NODE *) current;
	  cont = P(fetch_key)(in, &this->key);
	  if (!cont)
	    break;
	get_data:
	  current = P(fetch_item)(in, &this->key, bufend);
	  if (!current)
	    {
	      if (leftover)		/* Single node too large */
		{
		  P(copy_data)(in, out1, &leftover->key);
		  leftover = NULL;
		  run_count++;
		  giant_count++;
		}
	      else			/* Node will be left over to the next phase */
		leftover = this;
	      break;
	    }
	  *last = this;
	  last = &this->next;
	  leftover = NULL;
	}
      *last = NULL;
      if (!first)
	continue;

      first = P(mergesort)(first);
      run_count++;
      while (first)
	{
#ifdef SORT_UNIFY
	  SORT_NODE *second = first->next;
	  if (second && !P(compare)(&first->key, &second->key))
	    {
	      SORT_KEY *n = P(merge_items)(&first->key, &second->key);
	      if (n == &first->key)
		first->next = second->next;
	      else if (n)
		first = first->next;
	      else
		first = second->next;
	      continue;
	    }
#endif
#ifdef SORT_ASSERT_UNIQUE
	  ASSERT(!first->next || P(compare)(&first->key, &first->next->key));
#endif
	  P(store_item)(out1, &first->key);
	  first = first->next;
	}
    }

  bclose(in);
  if (sorter_trace)
    msg(L_INFO, "Pass 0: %d runs (%d giants, %d splits), %d+%d KB",
	run_count, giant_count, split_count,
	(out1 ? (int)((btell(out1) + 1023) / 1024) : 0),
	(out2 ? (int)((btell(out2) + 1023) / 1024) : 0));
  *fb1 = P(flush_out)(out1);
  *fb2 = P(flush_out)(out2);
  xfree(buffer);
}

#endif		/* SORT_REGULAR && !SORT_UNIFY */

#endif		/* SORT_PRESORT */

static
#ifdef SORT_OUTPUT_FB
struct fastbuf *
#elif defined(SORT_OUTPUT_FILE)
void
#else
#error No output defined.
#endif
P(sort)(
#ifdef SORT_INPUT_FILE
byte *inname
#elif defined(SORT_INPUT_FB)
struct fastbuf *fb1
#elif defined(SORT_INPUT_FBPAIR)
struct fastbuf *fb1, struct fastbuf *fb2
#else
#error No input defined.
#endif
#ifdef SORT_OUTPUT_FILE
,byte *outname
#endif
)
{
#ifdef SORT_INPUT_FILE
  struct fastbuf *fb1, *fb2;
  fb1 = bopen(inname, O_RDONLY, sorter_stream_bufsize);
  fb2 = NULL;
#elif defined(SORT_INPUT_FB)
  struct fastbuf *fb2 = NULL;
#endif

#ifdef SORT_DELETE_INPUT
  bconfig(fb1, BCONFIG_IS_TEMP_FILE, SORT_DELETE_INPUT);
#endif
  sorter_pass_counter = 1;
#ifdef SORT_PRESORT
  P(presort)(&fb1, &fb2);
  if (fb2)
#endif
#ifndef SORT_UP_TO
    do P(pass)(&fb1, &fb2); while (fb1 && fb2);
#else
    {
      sh_off_t run_count, max_run_count = 0;
      if (fb1)
	max_run_count += bfilesize(fb1);
      if (fb2)
	max_run_count += bfilesize(fb2);
#ifdef SORT_PRESORT
      run_count = max_run_count / sorter_presort_bufsize;
#else
      run_count = max_run_count;
#endif
      if (SORT_UP_TO)
	max_run_count /= SORT_UP_TO;
      do
	run_count = P(pass)(&fb1, &fb2, (run_count+1)/2 <= max_run_count);
      while (fb1 && fb2);
    }
#endif
  if (!fb1)
    fb1 = bopen_tmp(sorter_stream_bufsize);

#ifdef SORT_OUTPUT_FB
  return fb1;
#else
  bconfig(fb1, BCONFIG_IS_TEMP_FILE, 0);
  if (rename(fb1->name, outname) < 0)
    die("rename(%s,%s): %m", fb1->name, outname);
  bclose(fb1);
#endif
}

#undef P
#undef LESS
#undef SWAP
#undef SORT_NODE
#undef SORT_KEY
#undef SORT_PREFIX
#undef SORT_UNIFY
#undef SORT_UNIQUE
#undef SORT_ASSERT_UNIQUE
#undef SORT_REGULAR
#undef SORT_DELETE_INPUT
#undef SORT_INPUT_FILE
#undef SORT_INPUT_FB
#undef SORT_INPUT_FBPAIR
#undef SORT_OUTPUT_FILE
#undef SORT_OUTPUT_FB
#undef SORT_PRESORT
#undef SORT_UP_TO
