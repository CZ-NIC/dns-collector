/*
 *	Sherlock Library -- Content-Type Pattern Matching
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 */

#include "lib/lib.h"
#include "lib/chartype.h"

int
match_ct_patt(byte *p, byte *t)
{
  if (*p == '*' && !p[1])		/* "*" matches everything */
    return 1;

  if (*p == '*' && p[1] == '/')		/* "*" on the left-hand side */
    {
      while (*t && *t != ' ' && *t != ';' && *t != '/')
	t++;
      p += 2;
    }
  else					/* Normal left-hand side */
    {
      while (*p != '/')
	if (Cupcase(*p++) != Cupcase(*t++))
	  return 0;
      p++;
    }
  if (*t++ != '/')
    return 0;

  if (*p == '*' && !p[1])		/* "*" on the right-hand side */
    return 1;
  while (*p)
    if (Cupcase(*p++) != Cupcase(*t++))
      return 0;
  if (*t && *t != ' ' && *t != ';')
    return 0;

  return 1;
}
