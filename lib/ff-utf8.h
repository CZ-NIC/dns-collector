/*
 *	Sherlock Library: Reading and writing of UTF-8 on Fastbuf Streams
 *
 *	(c) 2001--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _FF_UTF8_H
#define _FF_UTF8_H

#include "lib/fastbuf.h"
#include "lib/unicode.h"

int bget_utf8_slow(struct fastbuf *b);
void bput_utf8_slow(struct fastbuf *b, uns u);

static inline int
bget_utf8(struct fastbuf *b)
{
  uns u;

  if (b->bptr + 5 <= b->bufend)
    {
      GET_UTF8(b->bptr, u);
      return u;
    }
  else
    return bget_utf8_slow(b);
}

static inline void
bput_utf8(struct fastbuf *b, uns u)
{
  ASSERT(u < 65536);
  if (b->bptr + 5 <= b->bufend)
    PUT_UTF8(b->bptr, u);
  else
    bput_utf8_slow(b, u);
}

#endif
