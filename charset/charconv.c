/*
 *	Character Set Conversion Library 1.0
 *
 *	(c) 1998 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU General Public License.
 */

#include "charset/charconv.h"
#include "charset/chartable.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

void
conv_init(struct conv_context *c)
{
  c->source = c->source_end = NULL;
  c->dest = c->dest_start = c->dest_end = NULL;
}

static int
conv_none(struct conv_context *c)
{
  c->dest_start = (char *) c->source;
  c->dest = (char *) c->source_end;
  return CONV_SOURCE_END | CONV_DEST_END | CONV_SKIP;
}

static int
conv_from_utf8(struct conv_context *c)
{
  unsigned short *x_to_out = c->x_to_out;
  const unsigned char *s = c->source;
  const unsigned char *se = c->source_end;
  unsigned char *d = c->dest;
  unsigned char *de = c->dest_end;
  unsigned char *strings = string_table - 0x100;
  unsigned int counter, code, cc;

  if (c->state)
    goto go_slow;

  while (s < se)			/* Optimized for speed, beware of spaghetti code */
    {
      cc = *s++;
      if (cc < 0x80)
	code = cc;
      else if (cc >= 0xc0)
	{
	  if (s + 6 > se)
	    goto go_slow_1;
	  if (cc < 0xe0)
	    {
	      if ((s[0] & 0xc0) != 0x80)
		goto nocode;
	      code = cc & 0x1f;
	      code = (code << 6) | (*s++ & 0x3f);
	    }
	  else if (cc < 0xf0)
	    {
	      if ((s[0] & 0xc0) != 0x80 || (s[1] & 0xc0) != 0x80)
		goto nocode;
	      code = cc & 0x0f;
	      code = (code << 6) | (*s++ & 0x3f);
	      code = (code << 6) | (*s++ & 0x3f);
	    }
	  else if (cc < 0xfc)
	    {
	      while (cc & 0x80)
		{
		  if ((*s++ & 0xc0) != 0x80)
		    break;
		  cc <<= 1;
		}
	      goto nocode;
	    }
	  else
	    goto nocode;
	}
      else
	{
	nocode:
	  code = 0xfffd;
	}
    uni_again:
      code = x_to_out[uni_to_x[code >> 8U][code & 0xff]];
    code_again:
      if (code < 0x100)
	{
	  if (d >= de)
	    goto dend;
	  *d++ = code;
	}
      else
	{
	  unsigned char *k = strings + code;
	  unsigned int len = *k++;

	  if (d + len > de)
	    goto dend;
	  while (len--)
	    *d++ = *k++;
	}
    }
  c->state = 0;
send_noreset:
  c->source = s;
  c->dest = d;
  return CONV_SOURCE_END;

dend:
  c->state = ~0;
  c->value = code;
  c->source = s;
  c->dest = d;
  return CONV_DEST_END;

go_slow:
  code = c->value;
  counter = c->state;
  if (counter == ~0U)
    goto code_again;
  goto go_slow_2;

go_slow_1:
  if (cc < 0xe0) { code = cc & 0x1f; counter = 1; }
  else if (cc < 0xf0) { code = cc & 0x0f; counter = 2; }
  else
    {
      code = ~0;
      if (cc < 0xf8) counter = 3;
      else if (cc < 0xfc) counter = 4;
      else if (cc < 0xfe) counter = 5;
      else goto nocode;
    }
go_slow_2:
  while (counter)
    {
      if (s >= se)
	{
	  c->state = counter;
	  c->value = code;
	  goto send_noreset;
	}
      if ((*s & 0xc0) != 0x80)
	goto nocode;
      code = (code << 6) | (*s++ & 0x3f);
      counter--;
    }
  if (code >= 0x10000)
    goto nocode;
  goto uni_again;
}

static int
conv_to_utf8(struct conv_context *c)
{
  unsigned short *in_to_x = c->in_to_x;
  const unsigned char *s = c->source;
  const unsigned char *se = c->source_end;
  unsigned char *d = c->dest;
  unsigned char *de = c->dest_end;

  while (s < se)
    {
      unsigned int code = x_to_uni[in_to_x[*s]];
      if (code < 0x80)
	{
	  if (d >= de)
	    goto dend;
	  *d++ = code;
	}
      else if (code < 0x800)
	{
	  if (d + 2 > de)
	    goto dend;
	  *d++ = 0xc0 | (code >> 6);
	  *d++ = 0x80 | (code & 0x3f);
	}
      else
	{
	  if (d + 3 > de)
	    goto dend;
	  *d++ = 0xc0 | (code >> 12);
	  *d++ = 0x80 | ((code >> 6) & 0x3f);
	  *d++ = 0x80 | (code & 0x3f);
	}
      s++;
    }
  c->source = s;
  c->dest = d;
  return CONV_SOURCE_END;

dend:
  c->source = s;
  c->dest = d;
  return CONV_DEST_END;
}

static int
conv_standard(struct conv_context *c)
{
  unsigned short *in_to_x = c->in_to_x;
  unsigned short *x_to_out = c->x_to_out;
  const unsigned char *s = c->source;
  const unsigned char *se = c->source_end;
  unsigned char *d = c->dest;
  unsigned char *de = c->dest_end;
  unsigned char *strings = string_table - 0x100;

  while (s < se)
    {
      unsigned int code = x_to_out[in_to_x[*s]];
      if (code < 0x100)
	{
	  if (d >= de)
	    goto dend;
	  *d++ = code;
	}
      else
	{
	  unsigned char *k = strings + code;
	  unsigned int len = *k++;

	  if (d + len > de)
	    goto dend;
	  while (len--)
	    *d++ = *k++;
	}
      s++;
    }
  c->source = s;
  c->dest = d;
  return CONV_SOURCE_END;

dend:
  c->source = s;
  c->dest = d;
  return CONV_DEST_END;
}

void
conv_set_charset(struct conv_context *c, int src, int dest)
{
  if (src == dest)
    c->convert = conv_none;
  else
    {
      c->convert = conv_standard;
      if (src == CONV_CHARSET_UTF8)
	c->convert = conv_from_utf8;
      else
	c->in_to_x = input_to_x[src];
      if (dest == CONV_CHARSET_UTF8)
	c->convert = conv_to_utf8;
      else
	c->x_to_out = x_to_output[dest];
    }
  c->state = 0;
}
