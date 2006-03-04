/*
 *	Sherlock Library -- Character Conversion with Allocation on the Stack 
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 */

#include "lib/lib.h"
#include "charset/stk-charconv.h"
#include <string.h>

uns
stk_conv_internal(struct conv_context *c, byte *s, uns in_cs, uns out_cs)
{
  /* We do not allocate anything for identical charsets. */
  if (in_cs == out_cs)
    {
      c->dest_start = s;
      return 0;
    }  
  
  uns l = strlen(s);
  
  conv_init(c);
  conv_set_charset(c, in_cs, out_cs);
  c->source = s;
  c->source_end = s + l + 1;

  /* Resulting string can be longer after the conversion.
   * The following constant must be at least 3 for conversion to UTF-8
   * and at least the maximum length of the strings in string_table for other charsets. */
  return 3 * l + 1;
}
