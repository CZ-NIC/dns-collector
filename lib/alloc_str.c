/*
 *	Sherlock Library -- String Allocation
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <string.h>

byte *
xstrdup(byte *s)
{
  uns l = strlen(s) + 1;
  return memcpy(xmalloc(l), s, l);
}
