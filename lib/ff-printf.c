/*
 *	UCW Library -- Printf on Fastbuf Streams
 *
 *	(c) 2002 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"

#include <alloca.h>

int
vbprintf(struct fastbuf *b, byte *msg, va_list args)
{
  byte *buf;
  int len, r;

  len = bdirect_write_prepare(b, &buf);
  if (len >= 16)
    {
      r = vsnprintf(buf, len, msg, args);
      if (r < 0)
	len = 256;
      else if (r < len)
	{
	  bdirect_write_commit(b, buf+r);
	  return r;
	}
      else
	len = r+1;
    }
  else
    len = 256;

  while (1)
    {
      buf = alloca(len);
      r = vsnprintf(buf, len, msg, args);
      if (r < 0)
	len += len;
      else if (r < len)
	{
	  bwrite(b, buf, r);
	  return r;
	}
      else
	len = r+1;
    }
}

int
bprintf(struct fastbuf *b, byte *msg, ...)
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
