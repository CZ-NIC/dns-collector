/*
 *	UCW Library -- Prefetch
 *
 *	(c) 1997--2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_PREFETCH_H
#define _UCW_PREFETCH_H

#if defined(__pentium4)
  /* Default prefetches are good enough */

#elif defined(__k6)
  /* K6 doesn't have prefetches */

#elif defined(__athlon) || defined(__i686)

#define HAVE_PREFETCH
static inline void prefetch(void *addr)
{
  asm volatile ("prefetcht0 %0" : : "m" (*(byte*)addr));
}

#else
#warning "Don't know how to prefetch on your CPU. Please fix lib/prefetch.h."
#endif

#ifndef HAVE_PREFETCH
static inline void prefetch(void *addr UNUSED)
{
}
#endif

#endif
