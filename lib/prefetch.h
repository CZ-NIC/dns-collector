/*
 *	Sherlock Library -- Prefetch
 *
 *	(c) 1997--2003 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _SHERLOCK_PREFETCH_H
#define _SHERLOCK_PREFETCH_H

#if defined(__athlon) || defined(__i686)

static inline void prefetch(void *addr)
{
  asm volatile ("prefetcht0 %0" : : "m" (*(byte*)addr));
}

#else

#if !defined(__k6)
#warning "Don't know how to prefetch on your CPU. Please fix lib/prefetch.h."
#endif

static inline void prefetch(void *addr UNUSED)
{
}

#endif

#endif
