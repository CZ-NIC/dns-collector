/*
 *	Sherlock Library -- Shell-Like Case-Insensitive Pattern Matching (currently only '?' and '*')
 *
 *	(c) 1997 Martin Mares, <mj@atrey.karlin.mff.cuni.cz>
 */

#include <stdio.h>
#include <string.h>

#include "lib.h"
#include "string.h"

int
match_pattern_nocase(byte *p, byte *s)
{
  while (*p)
    {
      if (*p == '?' && *s)
	p++, s++;
      else if (*p == '*')
	{
	  int z = p[1];

	  if (!z)
	    return 1;
	  while (s = strchr(s, z))
	    {
	      if (match_pattern_nocase(p+1, s))
		return 1;
	      s++;
	    }
	  return 0;
	}
      else
	{
	  if (*p == '\\' && p[1])
	    p++;
	  if (Cupcase(*p++) != Cupcase(*s++))
	    return 0;
	}
    }
  return !*s;
}
