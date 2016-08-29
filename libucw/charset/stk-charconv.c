/*
 *	UCW Library -- Character Conversion with Allocation on the Stack
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <charset/stk-charconv.h>
#include <string.h>

#define INITIAL_MIN_SIZE	16
#define INITIAL_SCALE		2

uint
stk_strconv_init(struct conv_context *c, const byte *s, uint in_cs, uint out_cs)
{
  uint l = strlen(s);
  if (in_cs == out_cs)
  {
    c->source = s;
    c->source_end = NULL;
    return l + 1;
  }
  conv_init(c);
  conv_set_charset(c, in_cs, out_cs);
  c->source = s;
  c->source_end = s + l + 1;
  if (l < (INITIAL_MIN_SIZE - 1) / INITIAL_SCALE)
    return INITIAL_MIN_SIZE;
  else
    return l * INITIAL_SCALE + 1;
}

uint
stk_strconv_step(struct conv_context *c, byte *buf, uint len)
{
  if (!c->source_end)
  {
    memcpy(buf, c->source, len);
    c->dest_start = buf;
    return 0;
  }
  if (c->dest_start)
  {
    uint l = c->dest_end - c->dest_start;
    memcpy(buf, c->dest_start, l);
    c->dest = buf + l;
  }
  else
    c->dest = buf;
  c->dest_start = buf;
  c->dest_end = buf + len;
  if (conv_run(c) & CONV_SOURCE_END)
    return 0;
  return len << 1;
}

