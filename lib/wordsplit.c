/*
 *	Sherlock Library -- Word Splitting
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>

#include "lib.h"
#include "string.h"

int
wordsplit(byte *src, byte **dst, uns max)
{
  int cnt = 0;

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
