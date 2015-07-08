/*
 *	UCW Library -- Memory Pools (String Operations)
 *
 *	(c) 2004--2015 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/mempool.h>
#include <ucw/unicode.h>

#include <alloca.h>
#include <string.h>

char *
mp_strdup(struct mempool *p, const char *s)
{
  if (!s)
    return NULL;
  size_t l = strlen(s) + 1;
  char *t = mp_alloc_fast_noalign(p, l);
  memcpy(t, s, l);
  return t;
}

void *
mp_memdup(struct mempool *p, const void *s, size_t len)
{
  void *t = mp_alloc_fast(p, len);
  memcpy(t, s, len);
  return t;
}

char *
mp_multicat(struct mempool *p, ...)
{
  va_list args, a;
  va_start(args, p);
  char *x, *y;
  uint cnt = 0;
  va_copy(a, args);
  while (x = va_arg(a, char *))
    cnt++;
  size_t *sizes = alloca(cnt * sizeof(*sizes));
  size_t len = 1;
  cnt = 0;
  va_end(a);
  va_copy(a, args);
  while (x = va_arg(a, char *))
    len += sizes[cnt++] = strlen(x);
  char *buf = mp_alloc_fast_noalign(p, len);
  y = buf;
  va_end(a);
  cnt = 0;
  while (x = va_arg(args, char *))
    {
      memcpy(y, x, sizes[cnt]);
      y += sizes[cnt++];
    }
  *y = 0;
  va_end(args);
  return buf;
}

char *
mp_strjoin(struct mempool *p, char **a, uint n, uint sep)
{
  size_t sizes[n];
  size_t len = 1;
  for (uint i=0; i<n; i++)
    len += sizes[i] = strlen(a[i]);
  if (sep && n)
    len += n-1;
  char *dest = mp_alloc_fast_noalign(p, len);
  char *d = dest;
  for (uint i=0; i<n; i++)
    {
      if (sep && i)
	*d++ = sep;
      memcpy(d, a[i], sizes[i]);
      d += sizes[i];
    }
  *d = 0;
  return dest;
}

char *
mp_str_from_mem(struct mempool *a, const void *mem, size_t len)
{
  char *str = mp_alloc_noalign(a, len+1);
  memcpy(str, mem, len);
  str[len] = 0;
  return str;
}

void *mp_append_utf8_32(struct mempool *pool, void *p, uint c)
{
  p = mp_spread(pool, p, utf8_space(c));
  return utf8_32_put(p, c);
}

#ifdef TEST

#include <stdio.h>

int main(void)
{
  struct mempool *p = mp_new(64);
  char *s = mp_strdup(p, "12345");
  char *c = mp_multicat(p, "<<", s, ">>", NULL);
  puts(c);
  char *a[] = { "bugs", "gnats", "insects" };
  puts(mp_strjoin(p, a, 3, '.'));
  puts(mp_strjoin(p, a, 3, 0));
  puts(mp_str_from_mem(p, s+1, 2));
  return 0;
}

#endif
