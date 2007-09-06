/*
 *	UCW Library -- Optimized Array Sorter
 *
 *	(c) 2003--2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 *  This is a generator of routines for sorting arrays, very similar to the one
 *  in lib/arraysort.h.
 *
 *  FIXME: <comments>
 *  FIXME: Note on thread-safety
 *
 *  So much for advocacy, there are the parameters (those marked with [*]
 *  are mandatory):
 *
 *  ASORT_PREFIX(x) [*]	add a name prefix (used on all global names
 *			defined by the sorter)
 *  ASORT_KEY_TYPE  [*]	data type of a single array entry key
 *  ASORT_LT(x,y)	x < y for ASORT_TYPE (default: "x<y")
 *  ASORT_THRESHOLD	threshold for switching between quicksort and insertsort
 *  ASORT_PAGE_ALIGNED	the array is guaranteed to be aligned to a multiple of CPU_PAGE_SIZE  (FIXME: Do we need this?)
 *  ASORT_HASH(x)	FIXME
 *  ASORT_RADIX_BITS	FIXME
 *  ASORT_SWAP		FIXME: probably keep private
 *
 *  After including this file, a function
 * 	ASORT_KEY_TYPE *ASORT_PREFIX(sort)(ASORT_KEY_TYPE *array, uns num_elts, ASORT_KEY_TYPE *buf, uns hash_bits)
 *  is declared and all parameter macros are automatically undef'd.
 */

#include "lib/sorter/common.h"

#define Q(x) ASORT_PREFIX(x)

typedef ASORT_KEY_TYPE Q(key);

#ifndef ASORT_LT
#define ASORT_LT(x,y) ((x) < (y))
#endif

#ifndef ASORT_SWAP
#define ASORT_SWAP(i,j) do { Q(key) tmp = array[i]; array[i]=array[j]; array[j]=tmp; } while (0)
#endif

#ifndef ASORT_THRESHOLD
#define ASORT_THRESHOLD 8		/* Guesswork and experimentation */
#endif

#ifndef ASORT_RADIX_BITS
#define ASORT_RADIX_BITS 10		// FIXME: Tune automatically?
#endif
#define ASORT_RADIX_MASK ((1 << (ASORT_RADIX_BITS)) - 1)

static void Q(quicksort)(void *array_ptr, uns num_elts)
{
  Q(key) *array = array_ptr;
  struct stk { int l, r; } stack[8*sizeof(uns)];
  int l, r, left, right, m;
  uns sp = 0;
  Q(key) pivot;

  if (num_elts <= 1)
    return;

  /* QuickSort with optimizations a'la Sedgewick, but stop at ASORT_THRESHOLD */

  left = 0;
  right = num_elts - 1;
  for(;;)
    {
      l = left;
      r = right;
      m = (l+r)/2;
      if (ASORT_LT(array[m], array[l]))
	ASORT_SWAP(l,m);
      if (ASORT_LT(array[r], array[m]))
	{
	  ASORT_SWAP(m,r);
	  if (ASORT_LT(array[m], array[l]))
	    ASORT_SWAP(l,m);
	}
      pivot = array[m];
      do
	{
	  while (ASORT_LT(array[l], pivot))
	    l++;
	  while (ASORT_LT(pivot, array[r]))
	    r--;
	  if (l < r)
	    {
	      ASORT_SWAP(l,r);
	      l++;
	      r--;
	    }
	  else if (l == r)
	    {
	      l++;
	      r--;
	    }
	}
      while (l <= r);
      if ((r - left) >= ASORT_THRESHOLD && (right - l) >= ASORT_THRESHOLD)
	{
	  /* Both partitions ok => push the larger one */
	  if ((r - left) > (right - l))
	    {
	      stack[sp].l = left;
	      stack[sp].r = r;
	      left = l;
	    }
	  else
	    {
	      stack[sp].l = l;
	      stack[sp].r = right;
	      right = r;
	    }
	  sp++;
	}
      else if ((r - left) >= ASORT_THRESHOLD)
	{
	  /* Left partition OK, right undersize */
	  right = r;
	}
      else if ((right - l) >= ASORT_THRESHOLD)
	{
	  /* Right partition OK, left undersize */
	  left = l;
	}
      else
	{
	  /* Both partitions undersize => pop */
	  if (!sp)
	    break;
	  sp--;
	  left = stack[sp].l;
	  right = stack[sp].r;
	}
    }

  /*
   * We have a partially sorted array, finish by insertsort. Inspired
   * by qsort() in GNU libc.
   */

  /* Find minimal element which will serve as a barrier */
  r = MIN(num_elts, ASORT_THRESHOLD);
  m = 0;
  for (l=1; l<r; l++)
    if (ASORT_LT(array[l], array[m]))
      m = l;
  ASORT_SWAP(0,m);

  /* Insertion sort */
  for (m=1; m<(int)num_elts; m++)
    {
      l=m;
      while (ASORT_LT(array[m], array[l-1]))
	l--;
      while (l < m)
	{
	  ASORT_SWAP(l,m);
	  l++;
	}
    }
}

#ifdef ASORT_HASH

static void Q(radix_count)(void *src_ptr, uns num_elts, uns *cnt, uns shift)
{
  Q(key) *src = src_ptr;
  for (uns i=0; i<num_elts; i++)
    cnt[ (ASORT_HASH(src[i]) >> shift) & ASORT_RADIX_MASK ] ++;
}

static void Q(radix_split)(void *src_ptr, void *dest_ptr, uns num_elts, uns *ptrs, uns shift)
{
  Q(key) *src = src_ptr, *dest = dest_ptr;
  for (uns i=0; i<num_elts; i++)
    dest[ ptrs[ (ASORT_HASH(src[i]) >> shift) & ASORT_RADIX_MASK ]++ ] = src[i];
}

#endif

static Q(key) *Q(sort)(Q(key) *array, uns num_elts, Q(key) *buffer, uns hash_bits)
{
  struct asort_context ctx = {
    .array = array,
    .buffer = buffer,
    .num_elts = num_elts,
    .hash_bits = hash_bits,
    .elt_size = sizeof(Q(key)),
    .quicksort = Q(quicksort),
#ifdef ASORT_HASH
    .radix_count = Q(radix_count),
    .radix_split = Q(radix_split),
    .radix_bits = ASORT_RADIX_BITS,
#endif
  };
  asort_run(&ctx);
  return ctx.array;
}

/* FIXME */
#undef ASORT_PREFIX
#undef ASORT_KEY_TYPE
#undef ASORT_LT
#undef ASORT_SWAP
#undef ASORT_THRESHOLD
#undef ASORT_PAGE_ALIGNED
#undef ASORT_HASH
#undef ASORT_RADIX_BITS
#undef ASORT_RADIX_MASK
#undef Q
