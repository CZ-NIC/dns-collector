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
  c->count = 0;

  /* For in_cs == out_cs, we emulate stk_strdup */
  if (in_cs == out_cs)
  {
    c->size[0] = c->request = l + 1;
    c->buf[0] = s;
    c->c.source = NULL;
    return;
  }
 
  /* Initialization */
  conv_init(&c->c);
  conv_set_charset(&c->c, in_cs, out_cs);
  c->c.source = s;
  c->c.source_end = s + l + 1;
  c->sum = 0;

  /* Size of the first buffer */
  if (l < (INITIAL_MIN_SIZE - 1) / INITIAL_SCALE)
    c->request = INITIAL_MIN_SIZE;
  else
    c->request = l * INITIAL_SCALE + 1;
}

int
stk_conv_step(struct stk_conv_context *c, byte *buf)
{
  /* Merge all buffers to the new one and exit */
  if (!c->c.source)
  {
    c->c.dest_start = buf;
    for (uns i = 0; i <= c->count; i++)
    {
      memcpy(buf, c->buf[i], c->size[i]);
      buf += c->size[i];
    }  
    return 0;
  }
  
  /* Run conv_run using the new buffer */
  c->buf[c->count] = c->c.dest_start = c->c.dest = buf;
  c->c.dest_end = buf + c->request;
  if (!(conv_run(&c->c) & CONV_SOURCE_END))
  {

    /* Buffer is too small, continue with a new one */
    c->size[c->count++] = c->request;
    c->sum += c->request;
    c->request <<= 1; /* This can be freely changed */
    return 1;
  }

  /* We have used only one buffer for the conversion, no merges are needed */
  if (!c->count)
    return 0;
  
  /* We can merge everything to the current buffer ... */
  uns s = c->c.dest - c->c.dest_start;
  if (c->sum + s <= c->request)
  {
    memmove(buf + c->sum, buf, s);
    for (uns i = 0; i < c->count; i++)
    {
      memcpy(buf, c->buf[i], c->size[i]);
      buf += c->size[i];
    }
    return 0;
  }
  
  /* ... or we allocate a new one */
  else
  {
    c->request = c->sum + s;
    c->size[c->count] = s;
    c->c.source = NULL;
    return 1;
  }
}

