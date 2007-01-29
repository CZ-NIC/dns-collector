/*
 *	UCW Library -- String Routines
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *	(c) 2007 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/chartype.h"
#include <stdlib.h>

/* Expands C99-like escape sequences.
 * It is safe to use the same buffer for both input and output. */
byte *
str_unesc(byte *d, byte *s)
{
  while (*s)
    {
      if (*s == '\\')
	switch (s[1])
	  {
	    case 'a': *d++ = '\a'; s += 2; break;
	    case 'b': *d++ = '\b'; s += 2; break;
	    case 'f': *d++ = '\f'; s += 2; break;
	    case 'n': *d++ = '\n'; s += 2; break;
	    case 'r': *d++ = '\r'; s += 2; break;
	    case 't': *d++ = '\t'; s += 2; break;
	    case 'v': *d++ = '\v'; s += 2; break;
	    case '\?': *d++ = '\?'; s += 2; break;
	    case '\'': *d++ = '\''; s += 2; break;
	    case '\"': *d++ = '\"'; s += 2; break;
	    case '\\': *d++ = '\\'; s += 2; break;
	    case 'x':
	      if (!Cxdigit(s[2]))
	        {
		  s++;
		  DBG("\\x used with no following hex digits");
		}
	      else
	        {
		  char *p;
		  uns v = strtoul(s + 2, &p, 16);
		  if (v <= 255)
		    *d++ = v;
		  else
		    DBG("hex escape sequence out of range");
                  s = (byte *)p;
		}
	      break;
            default:
	      if (s[1] >= '0' && s[1] <= '7')
	        {
		  uns v = s[1] - '0';
		  s += 2;
		  for (uns i = 0; i < 2 && *s >= '0' && *s <= '7'; s++, i++)
		    v = (v << 3) + *s - '0';
		  if (v <= 255)
		    *d++ = v;
		  else
		    DBG("octal escape sequence out of range");
	        }
	      *d++ = *s++;
	      break;
	  }
      else
	*d++ = *s++;
    }
  *d = 0;
  return d;
}

byte *
str_format_flags(byte *dest, const byte *fmt, uns flags)
{
  byte *start = dest;
  for (uns i=0; fmt[i]; i++)
    {
      if (flags & (1 << i))
	*dest++ = fmt[i];
      else
	*dest++ = '-';
    }
  *dest = 0;
  return start;
}
