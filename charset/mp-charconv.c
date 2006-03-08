/*
 *	Sherlock Library -- Character Conversion with Allocation on a Memory Pool 
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/mempool.h"
#include "charset/mp-charconv.h"
#include <string.h>
#include <alloca.h>

byte *
mp_conv(struct mempool *mp, byte *s, uns in_cs, uns out_cs)
{
  if (in_cs == out_cs)
    return mp_strdup(mp, s);
 
  struct conv_context c;
  char *b[32];
  uns bs[32], n = 0, sum = 0;
  uns l = strlen(s) + 1;
  
  conv_init(&c);
  conv_set_charset(&c, in_cs, out_cs);
  c.source = s;
  c.source_end = s + l;

  for (;;)
    {
      l <<= 1;
      c.dest_start = c.dest = b[n] = alloca(l);
      c.dest_end = c.dest_start+ l;
      uns r = conv_run(&c);
      sum += bs[n++] = c.dest - c.dest_start;
      if (r & CONV_SOURCE_END)
        {
          c.dest_start = c.dest = mp_alloc(mp, sum);
          for (uns i = 0; i < n; i++)
            {
              memcpy(c.dest, b[i], bs[i]);
              c.dest += bs[i];
            }
  	  return c.dest_start;
        }
    }
}

