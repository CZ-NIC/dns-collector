/*
 *	The UniCode Library: Reading and writing of UTF-8 on Fastbuf Streams
 *
 *	(c) 2001--2002 Martin Mares <mj@ucw.cz>
 */

#ifndef _UNISTREAM_H
#define _UNISTREAM_H

#include "charset/unicode.h"

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
