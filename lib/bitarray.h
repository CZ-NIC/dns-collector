/*
 *	Bit Array Operations
 *
 *	(c) 2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <string.h>

#define BIT_ARRAY_WORDS(n) (((n)+31)/32)
#define BIT_ARRAY(name,size) u32 name[BIT_ARRAY_WORDS(size)]

static inline void
bit_array_zero(u32 *a, uns n)
{
  bzero(a, 4*BIT_ARRAY_WORDS(n));
}

static inline void
bit_array_set(u32 *a, uns i)
{
  a[i/32] |= (1 << (i%32));
}

static inline void
bit_array_clear(u32 *a, uns i)
{
  a[i/32] &= ~(1 << (i%32));
}

static inline uns
bit_array_isset(u32 *a, uns i)
{
  return a[i/32] & (1 << (i%32));
}

static inline uns
bit_array_get(u32 *a, uns i)
{
  return !! bit_array_isset(a, i);
}

static inline uns
bit_array_test_and_set(u32 *a, uns i)
{
  uns t = bit_array_isset(a, i);
  bit_array_set(a, i);
  return t;
}

static inline uns
bit_array_test_and_clear(u32 *a, uns i)
{
  uns t = bit_array_isset(a, i);
  bit_array_clear(a, i);
  return t;
}

/* Iterate over all set bits, possibly destructively */
#define BIT_ARRAY_FISH_BITS_BEGIN(var,ary,size)					\
  for (uns var##_hi=0; var##_hi < BIT_ARRAY_WORDS(size); var##_hi++)		\
    for (uns var##_lo=0; ary[var##_hi]; var##_lo++)				\
      if (ary[var##_hi] & (1 << var##_lo))					\
        {									\
	  uns var = 32*var##_hi + var##_lo;					\
	  ary[var##_hi] &= ~(1 << var##_lo);					\
	  do

#define BIT_ARRAY_FISH_BITS_END							\
	  while (0);								\
	}
