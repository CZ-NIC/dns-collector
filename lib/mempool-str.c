/*
 *	Sherlock Library -- Memory Pools (String Operations)
 *
 *	(c) 2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/mempool.h"

#include <alloca.h>
#include <string.h>

char *
mp_strdup(struct mempool *p, char *s)
{
  uns l = strlen(s) + 1;
  char *t = mp_alloc_fast_noalign(p, l);
  memcpy(t, s, l);
  return t;
}

char *
mp_multicat(struct mempool *p, ...)
{
  va_list args, a;
  va_start(args, p);
  char *x, *y;
  uns cnt = 0;
  a = args;
  while (x = va_arg(a, char *))
    cnt++;
  uns *sizes = alloca(cnt * sizeof(uns));
  uns len = 1;
  cnt = 0;
  a = args;
  while (x = va_arg(a, char *))
    len += sizes[cnt++] = strlen(x);
  char *buf = mp_alloc_fast_noalign(p, len);
  y = buf;
  a = args;
  cnt = 0;
  while (x = va_arg(a, char *))
    {
      memcpy(y, x, sizes[cnt]);
      y += sizes[cnt++];
    }
  *y = 0;
  return buf;
}
