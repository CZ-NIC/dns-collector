/*
 *	UCW Library -- Memory Pools (Formatting)
 *
 *	(c) 2005 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/mempool.h"

#include <alloca.h>
#include <stdio.h>
#include <string.h>

char *
mp_vprintf(struct mempool *p, char *fmt, va_list args)
{
  char *ret = p->free;
  int free = p->last - p->free;
  if (!free)
    {
      ret = mp_alloc(p, 1);
      free = p->last - p->free;
    }
  int cnt = vsnprintf(ret, free, fmt, args);
  if (cnt < 0)
    {
      /* Our C library doesn't support C99 return value of vsnprintf, so we need to iterate */
      uns len = 128;
      char *buf;
      do
	{
	  len *= 2;
	  buf = alloca(len);
	  cnt = vsnprintf(buf, len, fmt, args);
	}
      while (cnt < 0);
      ret = mp_alloc(p, cnt+1);
      memcpy(ret, buf, cnt+1);
    }
  else if (cnt < free)
    p->free += cnt + 1;
  else
    {
      ret = mp_alloc(p, cnt+1);
      int cnt2 = vsnprintf(ret, cnt+1, fmt, args);
      ASSERT(cnt2 == cnt);
    }
  return ret;
}

char *
mp_printf(struct mempool *p, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  char *res = mp_vprintf(p, fmt, args);
  va_end(args);
  return res;
}

#ifdef TEST

int main(void)
{
  struct mempool *mp = mp_new(64);
  char *x = mp_printf(mp, "Hello, %s!\n", "World");
  fputs(x, stdout);
  x = mp_printf(mp, "Hello, %100s!\n", "World");
  fputs(x, stdout);
  return 0;
}

#endif
