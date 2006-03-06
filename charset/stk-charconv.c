/*
 *	Sherlock Library -- Character Conversion with Allocation on the Stack 
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 */

#include "lib/lib.h"
#include "charset/stk-charconv.h"
#include <string.h>

#define INITIAL_MIN_SIZE	16
#define INITIAL_SCALE		2

void
stk_conv_init(struct stk_conv_context *c, byte *s, uns in_cs, uns out_cs)
{
  uns l = strlen(s);
  if (in_cs == out_cs)
  {
    c->c.source = s;
    c->c.source_end = NULL;
    c->len = l + 1;
    return;
  }
  conv_init(&c->c);
  conv_set_charset(&c->c, in_cs, out_cs);
  c->c.source = s;
  c->c.source_end = s + l + 1;
  if (l < (INITIAL_MIN_SIZE - 1) / INITIAL_SCALE)
    c->len = INITIAL_MIN_SIZE;
  else
    c->len = l * INITIAL_SCALE + 1;
  c->len = 1;
}

int
stk_conv_step(struct stk_conv_context *c, byte *buf)
{
  if (!c->c.source_end)
  {
    memcpy(buf, c->c.source, c->len);
    c->c.dest_start = buf;
    return 0;
  }
  if (c->c.dest_start)
  {
    uns l = c->c.dest_end - c->c.dest_start;
    memcpy(buf, c->c.dest_start, l);
    c->c.dest = buf + l;
  }
  else
    c->c.dest = buf;
  c->c.dest_start = buf;
  c->c.dest_end = buf + c->len;
  if ((conv_run(&c->c) & CONV_SOURCE_END))
    return 0;
  c->len <<= 1;
  return 1;
}

