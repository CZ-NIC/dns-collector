/*
 *	Sherlock: Processing of tagged characters
 *
 *	(c) 2001--2003 Martin Mares <mj@ucw.cz>
 */

#ifndef _SHERLOCK_TAGGED_TEXT_H
#define _SHERLOCK_TAGGED_TEXT_H

#include "lib/fastbuf.h"
#include "charset/unistream.h"

/* Reading of tagged text (Unicode values, tags mapped to 0x80000000 and higher) */

#define GET_TAGGED_CHAR(p,u) do {				\
  u = *p;							\
  if (u >= 0xc0)						\
    GET_UTF8_CHAR(p,u);						\
  else if (u >= 0x80)						\
    {								\
      p++;							\
      if (u >= 0xb0)						\
        {							\
	  ASSERT(u == 0xb0);					\
	  u += 0x80020000;					\
        }							\
      else if (u >= 0xa0)					\
        {							\
	  ASSERT(*p >= 0x80 && *p <= 0xbf);			\
	  u = 0x80010000 + ((u & 0x0f) << 6) + (*p++ & 0x3f);	\
        }							\
      else							\
	u += 0x80000000;					\
    }								\
  else								\
    p++;							\
} while (0)

#define SKIP_TAGGED_CHAR(p) do {				\
  if (*p >= 0x80 && *p < 0xc0)					\
    {								\
      uns u = *p++;						\
      if (u >= 0xa0 && u < 0xb0 && *p >= 0x80 && *p < 0xc0)	\
	p++;							\
    }								\
  else								\
    UTF8_SKIP(p);						\
} while (0)

static inline uns
bget_tagged_char(struct fastbuf *f)
{
  uns u = bgetc(f);
  if ((int)u < 0x80)
    ;
  else if (u < 0xc0)
    {
      if (u >= 0xb0)
	{
	  ASSERT(u == 0xb0);
	  u += 0x80020000;
	}
      else if (u >= 0xa0)
	{
	  uns v = bgetc(f);
	  ASSERT(v >= 0x80 && v <= 0xbf);
	  u = 0x80010000 + ((u & 0x0f) << 6) + (v & 0x3f);
	}
      else
	u += 0x80000000;
    }
  else
    {
      bungetc(f);
      u = bget_utf8(f);
    }
  return u;
}

#endif
