/*
 *	Sherlock Library -- Word Splitting
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/chartype.h"

#include <string.h>

int
sepsplit(byte *str, byte sep, byte **rec, uns max)
{
  uns cnt = 0;
  while (1)
  {
    rec[cnt++] = str;
    str = strchr(str, sep);
    if (!str)
      return cnt;
    if (cnt >= max)
      return -1;
    *str++ = 0;
  }
}

int
wordsplit(byte *src, byte **dst, uns max)
{
  uns cnt = 0;

  for(;;)
    {
      while (Cspace(*src))
	*src++ = 0;
      if (!*src)
	break;
      if (cnt >= max)
	return -1;
      if (*src == '"')
	{
	  src++;
	  dst[cnt++] = src;
	  while (*src && *src != '"')
	    src++;
	  if (*src)
	    *src++ = 0;
	}
      else
	{
	  dst[cnt++] = src;
	  while (*src && !Cspace(*src))
	    src++;
	}
    }
  return cnt;
}
