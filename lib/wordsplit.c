/*
 *	Sherlock Library -- Word Splitting
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/chartype.h"

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
