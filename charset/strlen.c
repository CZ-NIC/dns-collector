/*
 *	The UniCode Library -- String Length
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *	(c) 2003 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "charset/unicode.h"

uns
Ustrlen(word *w)
{
  word *z = w;

  while (*z)
    z++;
  return z - w;
}

uns
utf8_strlen(byte *str)
{
  uns len = 0;
  while (1)
  {
    uns c;
    GET_UTF8(str, c);
    if (!c)
      return len;
    len++;
  }
}

