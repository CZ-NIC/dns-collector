/*
 *	UCW Library -- Allocation of Large Aligned Buffers
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <sys/mman.h>
#include <sys/user.h>

static unsigned int
big_round(unsigned int len)
{
  return ALIGN_TO(len, PAGE_SIZE);
}

void *
big_alloc(unsigned int len)
{
  len = big_round(len);
#ifdef CONFIG_DEBUG
  len += 2*PAGE_SIZE;
#endif
  byte *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == (byte*) MAP_FAILED)
    die("Cannot mmap %d bytes of memory: %m", len);
#ifdef CONFIG_DEBUG
  mprotect(p, PAGE_SIZE, PROT_NONE);
  mprotect(p+len-PAGE_SIZE, PAGE_SIZE, PROT_NONE);
  p += PAGE_SIZE;
#endif
  return p;
}

void
big_free(void *start, unsigned int len)
{
  byte *p = start;
  ASSERT(!((addr_int_t) p & (PAGE_SIZE-1)));
  len = big_round(len);
#ifdef CONFIG_DEBUG
  p -= PAGE_SIZE;
  len += 2*PAGE_SIZE;
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
