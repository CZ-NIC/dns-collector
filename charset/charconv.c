/*
 *	Character Set Conversion Library 1.2
 *
 *	(c) 1998--2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "charset/charconv.h"
#include "charset/chartable.h"

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

enum state {
  CLEAN,
  SINGLE_WRITE,
  SEQ_WRITE,
  UTF8_READ,
  UTF8_WRITE_START,
  UTF8_WRITE_CONT
};

static int
conv_slow(struct conv_context *c)
{
  const unsigned char *s = c->source;
  const unsigned char *se = c->source_end;
  unsigned char *d = c->dest;
  unsigned char *de = c->dest_end;

  switch (c->state)
    {
    case SINGLE_WRITE:
      if (d >= de)
	goto cde;
      *d++ = c->code;
      break;
    case SEQ_WRITE:
      while (c->remains)
	{
	  if (d >= de)
	    goto cde;
	  *d++ = *c->string_at++;
	  c->remains--;
	}
      break;
    case UTF8_READ:
      while (c->remains)
	{
	  if (s >= se)
	    goto cse;
	  if ((*s & 0xc0) != 0x80)
	    {
	      c->code = 0xfffd;
	      break;
	    }
	  c->code = (c->code << 6) | (*s++ & 0x3f);
	  c->remains--;
	}
      if (c->code >= 0x10000)
	c->code = 0xfffd;
      c->source = s;
      c->state = 0;
      return -1;
    case UTF8_WRITE_START:
      if (d >= de)
	goto cde;
      if (c->code < 0x80)
	{
	  *d++ = c->code;
	  break;
	}
      else if (c->code < 0x800)
	{
	  *d++ = 0xc0 | (c->code >> 6);
	  c->code <<= 10;
	  c->remains = 1;
	}
      else
	{
	  *d++ = 0xe0 | (c->code >> 12);
	  c->code <<= 4;
	  c->remains = 2;
	}
      c->code &= 0xffff;
      c->state = UTF8_WRITE_CONT;
      /* fall-thru */
    case UTF8_WRITE_CONT:
      while (c->remains)
	{
	  if (d >= de)
	    goto cde;
	  *d++ = 0x80 | (c->code >> 10);
	  c->code <<= 6;
	  c->remains--;
	}
      break;
    default:
      ASSERT(0);
    }
  c->source = s;
  c->dest = d;
  c->state = 0;
  return 0;

 cse:
  c->source = s;
  return CONV_SOURCE_END;

 cde:
  c->dest = d;
  return CONV_DEST_END;
}

static int
conv_from_utf8(struct conv_context *c)
{
  unsigned short *x_to_out = c->x_to_out;
  const unsigned char *s, *se;
  unsigned char *d, *de, *k;
  unsigned int code, cc, len;
  int e;

  if (unlikely(c->state))
    goto slow;

main:
  s = c->source;
  se = c->source_end;
  d = c->dest;
  de = c->dest_end;
  while (s < se)			/* Optimized for speed, beware of spaghetti code */
    {
      cc = *s++;
      if (cc < 0x80)
	code = cc;
      else if (cc >= 0xc0)
	{
	  if (s + 6 > se)
	    goto send_utf;
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
    got_code:
      code = x_to_out[uni_to_x[code >> 8U][code & 0xff]];
      if (code < 0x100)
	{
	  if (d >= de)
	    goto dend_char;
	  *d++ = code;
	}
      else
	{
	  k = string_table + code - 0x100;
	  len = *k++;
	  if (d + len > de)
	    goto dend_str;
	  while (len--)
	    *d++ = *k++;
	}
    }
  c->source = s;
  c->dest = d;
  return CONV_SOURCE_END;

send_utf:
  if (cc < 0xe0)		{ c->code = cc & 0x1f; c->remains = 1; }
  else if (cc < 0xf0)		{ c->code = cc & 0x0f; c->remains = 2; }
  else
    {
      c->code = ~0U;
      if (cc < 0xf8)		c->remains = 3;
      else if (cc < 0xfc)      	c->remains = 4;
      else if (cc < 0xfe)	c->remains = 5;
      else goto nocode;
    }
  c->state = UTF8_READ;
  goto go_slow;

dend_str:
  c->state = SEQ_WRITE;
  c->string_at = k;
  c->remains = len;
  goto go_slow;

dend_char:
  c->state = SINGLE_WRITE;
  c->code = code;
go_slow:
  c->source = s;
  c->dest = d;
slow:
  e = conv_slow(c);
  if (e < 0)
    {
      code = c->code;
      s = c->source;
      se = c->source_end;
      d = c->dest;
      de = c->dest_end;
      goto got_code;
    }
  if (e)
    return e;
  goto main;
}

static int
conv_to_utf8(struct conv_context *c)
{
  unsigned short *in_to_x = c->in_to_x;
  const unsigned char *s, *se;
  unsigned char *d, *de;
  unsigned int code;
  int e;

  if (unlikely(c->state))
    goto slow;

main:
  s = c->source;
  se = c->source_end;
  d = c->dest;
  de = c->dest_end;
  while (s < se)
    {
      code = x_to_uni[in_to_x[*s]];
      if (code < 0x80)
	{
	  if (d >= de)
	    goto dend;
	  *d++ = code;
	}
      else if (code < 0x800)
	{
	  if (d + 2 > de)
	    goto dend_utf;
	  *d++ = 0xc0 | (code >> 6);
	  *d++ = 0x80 | (code & 0x3f);
	}
      else
	{
	  if (d + 3 > de)
	    goto dend_utf;
	  *d++ = 0xe0 | (code >> 12);
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

dend_utf:
  c->source = s+1;
  c->dest = d;
  c->state = UTF8_WRITE_START;
  c->code = code;
slow:
  e = conv_slow(c);
  if (e)
    return e;
  goto main;
}

static int
conv_standard(struct conv_context *c)
{
  unsigned short *in_to_x = c->in_to_x;
  unsigned short *x_to_out = c->x_to_out;
  const unsigned char *s, *se;
  unsigned char *d, *de, *k;
  unsigned int len, e;

  if (unlikely(c->state))
    goto slow;

main:
  s = c->source;
  se = c->source_end;
  d = c->dest;
  de = c->dest_end;
  while (s < se)
    {
      unsigned int code = x_to_out[in_to_x[*s]];
      if (code < 0x100)
	{
	  if (unlikely(d >= de))
	    goto dend;
	  *d++ = code;
	}
      else
	{
	  k = string_table + code - 0x100;
	  len = *k++;
	  if (unlikely(d + len > de))
	    goto dend_str;
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

dend_str:
  c->source = s;
  c->dest = d;
  c->state = SEQ_WRITE;
  c->string_at = k;
  c->remains = len;
slow:
  e = conv_slow(c);
  if (e)
    return e;
  goto main;
}

void
conv_set_charset(struct conv_context *c, int src, int dest)
{
  c->source_charset = src;
  c->dest_charset = dest;
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

unsigned int
conv_x_to_ucs(unsigned int x)
{
  return x_to_uni[x];
}

unsigned int
conv_ucs_to_x(unsigned int ucs)
{
  return uni_to_x[ucs >> 8U][ucs & 0xff];
}

unsigned int
conv_x_count(void)
{
  return sizeof(x_to_uni) / sizeof(x_to_uni[0]);
}
