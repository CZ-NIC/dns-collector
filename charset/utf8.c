/*
 *	The UniCode Library -- UTF-8 Functions
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include "unicode.h"

uns
ucs2_to_utf8(byte *d, word *s)
{
  byte *d0 = d;

  while (*s)
    {
      uns u = *s++;
      PUT_UTF8(d,u);
    }
  *d = 0;
  return d - d0;
}

uns
utf8_to_ucs2(word *d, byte *s)
{
  word *d0 = d;

  while (*s)
    if (IS_UTF8(*s))
      {
	uns u;
	GET_UTF8_CHAR(s,u);
	*d++ = u;
      }
    else if (*s >= 0x80)
      *d++ = UNI_REPLACEMENT;
    else
      *d++ = *s++;
  *d = 0;
  return d0 - d;
}
