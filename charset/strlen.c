/*
 *	The UniCode Library -- String Length
 *
 *	(c) 1997--2003 Martin Mares <mj@ucw.cz>
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
  while (*str)
    {
      UTF8_SKIP(str);
      len++;
    }
  return len;
}

uns
utf8_strnlen(byte *str, uns n)
{
  uns len = 0;
  byte *end = str + n;
  while (str < end)
    {
      UTF8_SKIP(str);
      len++;
    }
  return len;
}
