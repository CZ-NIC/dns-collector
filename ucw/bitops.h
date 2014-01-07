/*
 *	UCW Library -- Bit Operations
 *
 *	(c) 2005 Martin Mares <mj@ucw.cz>
 *	(c) 2012 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_BITOPS_H
#define _UCW_BITOPS_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define bit_fls ucw_bit_fls
#define ffs_table ucw_ffs_table
#endif

/* Find highest bit set (i.e., the floor of the binary logarithm) (bit-fls.c) */

int bit_fls(u32 x);		/* bit_fls(0)=-1 */

/* Find lowest bit set, undefined for zero argument (bit-ffs.c) */

extern const byte ffs_table[256];

#ifdef __pentium4		/* On other ia32 machines, the C version is faster */

static inline uns bit_ffs(uns w)
{
  asm("bsfl %1,%0" :"=r" (w) :"rm" (w));
  return w;
}

#else

static inline uns bit_ffs(uns w)
{
  uns b = (w & 0xffff) ? 0 : 16;
  b += ((w >> b) & 0xff) ? 0 : 8;
  return b + ffs_table[(w >> b) & 0xff];
}

#endif

/* Count the number of bits set */

#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)

static inline uns bit_count(uns w)
{
  return __builtin_popcount(w);
}

#else

static inline uns bit_count(uns w)
{
  uns n = 0;
  while (w)
    {
      w &= w - 1;
      n++;
    }
  return n;
}

#endif

#endif
