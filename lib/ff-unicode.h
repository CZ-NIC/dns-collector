/*
 *	UCW Library: Reading and writing of UTF-8 on Fastbuf Streams
 *
 *	(c) 2001--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_FF_UTF8_H
#define _UCW_FF_UTF8_H

#include "lib/fastbuf.h"
#include "lib/unicode.h"

int bget_utf8_slow(struct fastbuf *b);
int bget_utf8_32_slow(struct fastbuf *b);
void bput_utf8_slow(struct fastbuf *b, uns u);
void bput_utf8_32_slow(struct fastbuf *b, uns u);

static inline int
bget_utf8(struct fastbuf *b)
{
  uns u;

  if (bavailr(b) >= 3)
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
  if (bavailw(b) >= 3)
    PUT_UTF8(b->bptr, u);
  else
    bput_utf8_slow(b, u);
}

static inline int
bget_utf8_32(struct fastbuf *b)
{
  uns u;

  if (bavailr(b) >= 6)
    {
      GET_UTF8_32(b->bptr, u);
      return u;
    }
  else
    return bget_utf8_32_slow(b);
}

static inline void
bput_utf8_32(struct fastbuf *b, uns u)
{
  ASSERT(u < (1U<<31));
  if (bavailw(b) >= 6)
    PUT_UTF8_32(b->bptr, u);
  else
    bput_utf8_32_slow(b, u);
}

#endif
