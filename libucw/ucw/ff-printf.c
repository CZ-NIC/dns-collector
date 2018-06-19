/*
 *	UCW Library -- Printf on Fastbuf Streams
 *
 *	(c) 2002--2013 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/fastbuf.h>
#include <ucw/resource.h>

#include <stdio.h>
#include <alloca.h>

int
vbprintf(struct fastbuf *b, const char *msg, va_list args)
{
  byte *buf;
  int remains, len;
  va_list args2;

  va_copy(args2, args);
  remains = bdirect_write_prepare(b, &buf);
  len = vsnprintf(buf, remains, msg, args2);
  va_end(args2);

  if (len < remains)
    {
      bdirect_write_commit(b, buf + len);
      return len;
    }

  /*
   *  We need a temporary buffer. If it is small, let's use stack.
   *  Otherwise, we malloc it, but we have to be careful, since we
   *  might be running inside a transaction and bwrite() could
   *  throw an exception.
   *
   *  FIXME: This deserves a more systematic solution, the same
   *  problem is likely to happen at other places, too.
   */
  int bufsize = len + 1;
  struct resource *res = NULL;
  byte *extra_buffer = NULL;
  if (bufsize <= 256)
    buf = alloca(bufsize);
  else if (rp_current())
    buf = res_malloc(bufsize, &res);
  else
    buf = extra_buffer = xmalloc(bufsize);

  vsnprintf(buf, bufsize, msg, args);
  bwrite(b, buf, len);

  if (res)
    res_free(res);
  else if (extra_buffer)
    xfree(extra_buffer);
  return len;
}

int
bprintf(struct fastbuf *b, const char *msg, ...)
{
  va_list args;
  int res;

  va_start(args, msg);
  res = vbprintf(b, msg, args);
  va_end(args);
  return res;
}

#ifdef TEST

int main(void)
{
  struct fastbuf *b = bfdopen_shared(1, 65536);
  for (int i=0; i<10000; i++)
    bprintf(b, "13=%d str=<%s> msg=%m\n", 13, "str");
  bclose(b);
  return 0;
}

#endif
