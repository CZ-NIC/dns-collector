/*
 *	The UniCode Library: Reading and writing of UTF-8 on Fastbuf Streams
 *
 *	(c) 2001 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/fastbuf.h"
#include "charset/unicode.h"
#include "charset/unistream.h"

int
bget_utf8_slow(struct fastbuf *b)
{
  int c = bgetc(b);
  int code;

  if (c < 0x80)				/* Includes EOF */
    return c;
  if (c < 0xc0)				/* Incorrect combination */
    return UNI_REPLACEMENT;
  if (c >= 0xf0)			/* Too large, skip it */
    {
      while ((c = bgetc(b)) >= 0x80 && c < 0xc0)
	;
      goto wrong;
    }
  if (c >= 0xe0)			/* 3 bytes */
    {
      code = c & 0x0f;
      if ((c = bgetc(b)) < 0x80 || c >= 0xc0)
	goto wrong;
      code = (code << 6) | (c & 0x3f);
      if ((c = bgetc(b)) < 0x80 || c >= 0xc0)
	goto wrong;
      code = (code << 6) | (c & 0x3f);
    }
  else					/* 2 bytes */
    {
      code = c & 0x1f;
      if ((c = bgetc(b)) < 0x80 || c >= 0xc0)
	goto wrong;
      code = (code << 6) | (c & 0x3f);
    }
  return code;

 wrong:
  if (c >= 0)
    bungetc(b, c);
  return UNI_REPLACEMENT;
}

void
bput_utf8_slow(struct fastbuf *b, uns u)
{
  ASSERT(u < 65536);
  if (u < 0x80)
    bputc(b, u);
  else
    {
      if (u < 0x800)
	bputc(b, 0xc0 | (u >> 6));
      else
	{
	  bputc(b, 0xe0 | (u >> 12));
	  bputc(b, 0x80 | ((u >> 6) & 0x3f));
	}
      bputc(b, 0x80 | (u & 0x3f));
    }
}
