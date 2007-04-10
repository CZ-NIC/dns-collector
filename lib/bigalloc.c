/*
 *	UCW Library -- Allocation of Large Aligned Buffers
 *
 *	(c) 2006 Martin Mares <mj@ucw.cz>
 *	(c) 2007 Pavel Charvat <char@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <sys/mman.h>
#include <string.h>

void *
page_alloc(unsigned int len)
{
  ASSERT(!(len & (CPU_PAGE_SIZE-1)));
  byte *p = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  if (p == (byte*) MAP_FAILED)
    die("Cannot mmap %d bytes of memory: %m", len);
  return p;
}

void
page_free(void *start, unsigned int len)
{
  ASSERT(!(len & (CPU_PAGE_SIZE-1)));
  ASSERT(!((uintptr_t) start & (CPU_PAGE_SIZE-1)));
  munmap(start, len);
}

void *
page_realloc(void *start, unsigned int old_len, unsigned int new_len)
{
  void *p = page_alloc(new_len);
  memcpy(p, start, MIN(old_len, new_len));
  page_free(start, old_len);
  return p;
}

static unsigned int
big_round(unsigned int len)
{
  return ALIGN_TO(len, CPU_PAGE_SIZE);
}

void *
big_alloc(unsigned int len)
{
  len = big_round(len);
#ifdef CONFIG_DEBUG
  len += 2*CPU_PAGE_SIZE;
#endif
  byte *p = page_alloc(len);
#ifdef CONFIG_DEBUG
  mprotect(p, CPU_PAGE_SIZE, PROT_NONE);
  mprotect(p+len-CPU_PAGE_SIZE, CPU_PAGE_SIZE, PROT_NONE);
  p += CPU_PAGE_SIZE;
#endif
  return p;
}

void
big_free(void *start, unsigned int len)
{
  byte *p = start;
  len = big_round(len);
#ifdef CONFIG_DEBUG
  p -= CPU_PAGE_SIZE;
  len += 2*CPU_PAGE_SIZE;
#endif
  page_free(start, len);
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
