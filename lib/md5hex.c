/*
 *	Sherlock Library -- MD5 Binary <-> Hex Conversions
 *
 *	(c) 1997 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/chartype.h"

#include <stdio.h>

void
md5_to_hex(byte *s, byte *d)
{
  int i;
  for(i=0; i<MD5_SIZE; i++)
    d += sprintf(d, "%02X", *s++);
}

void
hex_to_md5(byte *s, byte *d)
{
  uns i, j;
  for(i=0; i<MD5_SIZE; i++)
    {
      if (!Cxdigit(s[0]) || !Cxdigit(s[1]))
	die("hex_to_md5: syntax error");
      j = Cxvalue(*s); s++;
      j = (j << 4) | Cxvalue(*s); s++;
      *d++ = j;
    }
}
