/*
 *	UCW Library -- Allocation of Large Aligned Buffers
 *
 *	(c) 2006--2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <sys/mman.h>

static u64
big_round(u64 len)
{
  return ALIGN_TO(len, (u64)CPU_PAGE_SIZE);
}

void *
big_alloc(u64 len)
{
  len = big_round(len);
#ifndef CPU_64BIT_POINTERS
  if (len > 0xff000000)
    die("big_alloc: Size %08Lx is too large for a 32-bit machine", (long long) len);
#endif
#ifdef CONFIG_DEBUG
  len += 2*CPU_PAGE_SIZE;
#endif
  byte *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == (byte*) MAP_FAILED)
    die("Cannot mmap %d bytes of memory: %m", len);
#ifdef CONFIG_DEBUG
  mprotect(p, CPU_PAGE_SIZE, PROT_NONE);
  mprotect(p+len-CPU_PAGE_SIZE, CPU_PAGE_SIZE, PROT_NONE);
  p += CPU_PAGE_SIZE;
#endif
  return p;
}

void
big_free(void *start, u64 len)
{
  byte *p = start;
  ASSERT(!((uintptr_t) p & (CPU_PAGE_SIZE-1)));
  len = big_round(len);
#ifdef CONFIG_DEBUG
  p -= CPU_PAGE_SIZE;
  len += 2*CPU_PAGE_SIZE;
#endif
  munmap(p, len);
}

#ifdef TEST

int main(void)
{
  byte *p = big_alloc(123456);
  // p[-1] = 1;
  big_free(p, 123456);
  return 0;
}

#endif
