/*
 *	Character Set Conversion Library 1.2
 *
 *	(c) 1998--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/unicode.h"
#include "lib/unaligned.h"
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
  UTF8_WRITE_CONT,
  UTF16_BE_WRITE,
  UTF16_LE_WRITE,
  UTF16_BE_READ,
  UTF16_BE_READ_1,
  UTF16_BE_READ_2,
  UTF16_BE_READ_3,
  UTF16_LE_READ,
  UTF16_LE_READ_1,
  UTF16_LE_READ_2,
  UTF16_LE_READ_3,
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
seq:
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
got_char:
      c->source = s;
      c->state = 0;
      return -1;

    /* Writing of UTF-8 */
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

    /* Writing of UTF-16BE */
    case UTF16_BE_WRITE:
      {
	void *p = &c->code;
	c->string_at = p;
	uns code = c->code;
	c->string_at = p;
	if (code < 0xd800 || code - 0xe000 < 0x2000)
	  {}
	else if ((code -= 0x10000) < 0x100000)
	  {
	    put_u16_be(p, 0xd800 | (code >> 10));
	    put_u16_be(p + 2, 0xdc00 | (code & 0x3ff));
	    c->remains = 4;
	    c->state = SEQ_WRITE;
	    goto seq;
	  }
	else
	  code = UNI_REPLACEMENT;
	put_u16_be(p, code);
	c->remains = 2;
        c->state = SEQ_WRITE;
	goto seq;
      }

    /* Writing of UTF-16LE */
    case UTF16_LE_WRITE:
      {
	void *p = &c->code;
	c->string_at = p;
	uns code = c->code;
	c->string_at = p;
	if (code < 0xd800 || code - 0xe000 < 0x2000)
	  {}
	else if ((code -= 0x10000) < 0x100000)
	  {
	    put_u16_le(p, 0xd800 | (code >> 10));
	    put_u16_le(p + 2, 0xdc00 | (code & 0x3ff));
	    c->remains = 4;
            c->state = SEQ_WRITE;
	  }
	else
	  code = UNI_REPLACEMENT;
        put_u16_le(p, code);
        c->remains = 2;
        c->state = SEQ_WRITE;
	goto seq;
      }

    /* Reading of UTF16-BE */
    case UTF16_BE_READ:
      if (s >= se)
	goto cse;
      c->code = *s++;
      c->state = UTF16_BE_READ_1;
      /* fall-thru */
    case UTF16_BE_READ_1:
      if (s >= se)
	goto cse;
      c->code = (c->code << 8) | *s++;
      if (c->code - 0xd800 >= 0x800)
        goto got_char;
      c->code = (c->code - 0xd800) << 10;
      c->state = UTF16_BE_READ_2;
      /* fall-thru */
    case UTF16_BE_READ_2:
      if (s >= se)
	goto cse;
      if (*s - 0xdc >= 4)
	c->code = ~0U;
      else
	c->code |= (*s - 0xdc) << 8;
      s++;
      c->state = UTF16_BE_READ_3;
      /* fall-thru */
    case UTF16_BE_READ_3:
      if (s >= se)
	goto cse;
      if ((int)c->code >= 0)
	c->code += 0x10000 + *s;
      else
	c->code = UNI_REPLACEMENT;
      s++;
      goto got_char;

    /* Reading of UTF16-LE */
    case UTF16_LE_READ:
      if (s >= se)
	goto cse;
      c->code = *s++;
      c->state = UTF16_LE_READ_1;
      /* fall-thru */
    case UTF16_LE_READ_1:
      if (s >= se)
	goto cse;
      c->code |= *s++ << 8;
      if (c->code - 0xd800 >= 0x800)
        goto got_char;
      c->code = (c->code - 0xd800) << 10;
      c->state = UTF16_LE_READ_2;
      /* fall-thru */
    case UTF16_LE_READ_2:
      if (s >= se)
	goto cse;
      c->code |= *s++;
      c->state = UTF16_LE_READ_3;
      /* fall-thru */
    case UTF16_LE_READ_3:
      if (s >= se)
	goto cse;
      if (*s - 0xdc < 4)
	c->code += 0x10000 + ((*s - 0xdc) << 8);
      else
	c->code = UNI_REPLACEMENT;
      s++;
      goto got_char;

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

/* Generate inlined routines */

static int
conv_std_to_utf8(struct conv_context *c)
{
#define CONV_READ_STD
#define CONV_WRITE_UTF8
#include "charset/charconv-gen.h"
}

static int
conv_utf8_to_std(struct conv_context *c)
{
#define CONV_READ_UTF8
#define CONV_WRITE_STD
#include "charset/charconv-gen.h"
}

static int
conv_std_to_utf16_be(struct conv_context *c)
{
#define CONV_READ_STD
#define CONV_WRITE_UTF16_BE
#include "charset/charconv-gen.h"
}

static int
conv_utf16_be_to_std(struct conv_context *c)
{
#define CONV_READ_UTF16_BE
#define CONV_WRITE_STD
#include "charset/charconv-gen.h"
}

static int
conv_std_to_utf16_le(struct conv_context *c)
{
#define CONV_READ_STD
#define CONV_WRITE_UTF16_LE
#include "charset/charconv-gen.h"
}

static int
conv_utf16_le_to_std(struct conv_context *c)
{
#define CONV_READ_UTF16_LE
#define CONV_WRITE_STD
#include "charset/charconv-gen.h"
}

static int
conv_utf8_to_utf16_be(struct conv_context *c)
{
#define CONV_READ_UTF8
#define CONV_WRITE_UTF16_BE
#include "charset/charconv-gen.h"
}

static int
conv_utf16_be_to_utf8(struct conv_context *c)
{
#define CONV_READ_UTF16_BE
#define CONV_WRITE_UTF8
#include "charset/charconv-gen.h"
}

static int
conv_utf8_to_utf16_le(struct conv_context *c)
{
#define CONV_READ_UTF8
#define CONV_WRITE_UTF16_LE
#include "charset/charconv-gen.h"
}

static int
conv_utf16_le_to_utf8(struct conv_context *c)
{
#define CONV_READ_UTF16_LE
#define CONV_WRITE_UTF8
#include "charset/charconv-gen.h"
}

static int
conv_utf16_be_to_utf16_le(struct conv_context *c)
{
#define CONV_READ_UTF16_BE
#define CONV_WRITE_UTF16_LE
#include "charset/charconv-gen.h"
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
    {
      c->convert = conv_none;
      c->in_to_x = NULL;
      c->x_to_out = NULL;
    }
  else
    {
      static uns lookup[] = {
	[CONV_CHARSET_UTF8] = 1,
	[CONV_CHARSET_UTF16_BE] = 2,
	[CONV_CHARSET_UTF16_LE] = 3,
      };
      static int (*tab[4][4])(struct conv_context *c) = {
	{ conv_standard,	conv_std_to_utf8,	conv_std_to_utf16_be,	conv_std_to_utf16_le },
	{ conv_utf8_to_std,	conv_none,		conv_utf8_to_utf16_be,	conv_utf8_to_utf16_le },
	{ conv_utf16_be_to_std,	conv_utf16_be_to_utf8,	conv_none,		conv_utf16_be_to_utf16_le },
	{ conv_utf16_le_to_std,	conv_utf16_le_to_utf8,	conv_utf16_be_to_utf16_le,	conv_none },
      };
      uns src_idx = ((uns)src < ARRAY_SIZE(lookup)) ? lookup[src] : 0;
      uns dest_idx = ((uns)dest < ARRAY_SIZE(lookup)) ? lookup[dest] : 0;
      c->convert = tab[src_idx][dest_idx];
      c->in_to_x = src_idx ? NULL : input_to_x[src];
      c->x_to_out = dest_idx ? NULL : x_to_output[dest];
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

int
conv_in_to_ucs(struct conv_context *c, unsigned int y)
{
  return x_to_uni[c->in_to_x[y]];
}

int conv_ucs_to_out(struct conv_context *c, unsigned int ucs)
{
  uns x = uni_to_x[ucs >> 8U][ucs & 0xff];
  if (x == 256 || c->x_to_out[x] >= 256)
    return -1;
  else
    return c->x_to_out[x];
}
