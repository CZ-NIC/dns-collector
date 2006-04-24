/*
 *	UCW Library -- String Routines
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/chartype.h"

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
	    case '0':
	      if (s[2] < '0' || s[2] > '7')
		*d++ = *s++;
	      else
	        {
		  uns v = 0;
		  for (s += 2; *s >= '0' && *s <= '7' && v < 32; s++)
		    v = (v << 3) + *s - '0';
		  *d++ = v;
		}
	      break;
	    case 'x':
	      if (!Cxdigit(s[2]))
		*d++ = *s++;
	      else
	        {
		  uns v = 0;
		  for (s += 2; Cxdigit(*s) && v < 16; s++)
		    v = (v << 4) + (Cdigit(*s) ? (*s - '0') : ((*s | 32) - 'A' + 10));
		  *d++ = v;
		}
	      break;
            default:
	      *d++ = *s++;
	      break;
	  }
      else
	*d++ = *s++;
    }
  *d = 0;
  return d;
}

