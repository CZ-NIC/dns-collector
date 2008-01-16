/*
 *	Character Set Conversion Library 1.2
 *
 *	(c) 1998--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2007 Pavel Charvat <pchar@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/* Generator of inlined conversion routines */

do {

/*** Header ***/
  const byte *s, *se;
  byte *d, *de;
  uns code;
  int e;

#ifdef CONV_READ_STD
  unsigned short *in_to_x = c->in_to_x;
#endif

#ifdef CONV_WRITE_STD
  unsigned short *x_to_out = c->x_to_out;
#endif

#ifdef CONV_READ_UTF8
  uns cc;
#endif

  if (unlikely(c->state))
    goto slow;
 main:
  s = c->source;
  se = c->source_end;
  d = c->dest;
  de = c->dest_end;

  while (1)
    {

/*** Read ***/

#ifdef CONV_READ_STD
      if (unlikely(s >= se))
	break;
#ifndef CONV_WRITE_STD
      code = x_to_uni[in_to_x[*s++]];
#endif
#endif

#ifdef CONV_READ_UTF8
      if (unlikely(s >= se))
	break;
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
	  code = UNI_REPLACEMENT;
	}
#endif

#ifdef CONV_READ_UTF16_BE
      if (unlikely(s + 4 >= se))
        {
	  c->state = UTF16_BE_READ;
	  goto go_slow;
	}
      s = utf16_be_get(s, &code);
#endif

#ifdef CONV_READ_UTF16_LE
      if (unlikely(s + 4 >= se))
        {
	  c->state = UTF16_LE_READ;
	  goto go_slow;
	}
      s = utf16_le_get(s, &code);
#endif

/*** Write ***/

got_code:

#ifdef CONV_WRITE_STD
#ifndef CONV_READ_STD
      code = x_to_out[uni_to_x[code >> 8U][code & 0xff]];
#else
      code = x_to_out[in_to_x[*s++]];
#endif
      if (code < 0x100)
        {
	  if (unlikely(d >= de))
	    {
	      c->state = SINGLE_WRITE;
	      c->code = code;
	      goto go_slow;
	    }
	  *d++ = code;
	}
      else
        {
	  byte *k = string_table + code - 0x100;
	  uns len = *k++;
	  if (unlikely(de - d < len))
	    {
	      c->state = SEQ_WRITE;
	      c->string_at = k;
	      c->remains = len;
	      goto go_slow;
	    }
	  while (len--)
	    *d++ = *k++;
	}
#endif

#ifdef CONV_WRITE_UTF8
      if (code < 0x80)
        {
	  if (d >= de)
	    goto dend_utf;
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
#endif

#ifdef CONV_WRITE_UTF16_BE
      if (unlikely(de - d < 2))
	goto write_slow;
      else if (code < 0xd800 || code - 0xe000 < 0x2000 ||
          ((code -= 0x10000) >= 0x10000 && (code = UNI_REPLACEMENT)))
        {
	  *d++ = code >> 8;
	  *d++ = code & 0xff;
	}
      else if (likely(de - d < 4))
        {
	  *d++ = 0xd8 | (code >> 18);
	  *d++ = (code >> 10) & 0xff;
	  *d++ = 0xdc | ((code >> 8) & 3);
	  *d++ = code & 0xff;
	}
      else
        {
write_slow:
	  c->state = UTF16_BE_WRITE;
	  goto go_slow;
	}
#endif

#ifdef CONV_WRITE_UTF16_LE
      if (unlikely(de - d < 2))
	goto write_slow;
      else if (code < 0xd800 || code - 0xe000 < 0x2000 ||
	  ((code -= 0x10000) >= 0x10000 && (code = UNI_REPLACEMENT)))
        {
	  *d++ = code & 0xff;
	  *d++ = code >> 8;
	}
      else if (likely(de - d < 4))
        {
	  *d++ = (code >> 10) & 0xff;
	  *d++ = 0xd8 | (code >> 18);
	  *d++ = code & 0xff;
	  *d++ = 0xdc | ((code >> 8) & 3);
	}
      else
        {
write_slow:
	  c->state = UTF16_LE_WRITE;
	  goto go_slow;
	}
#endif

    }

/*** Footer ***/

  c->source = s;
  c->dest = d;
  return CONV_SOURCE_END;

#ifdef CONV_READ_UTF8
 send_utf:
  if (cc < 0xe0)		{ c->code = cc & 0x1f; c->remains = 1; }
  else if (cc < 0xf0)		{ c->code = cc & 0x0f; c->remains = 2; }
  else
    {
      c->code = ~0U;
      if (cc < 0xf8)		c->remains = 3;
      else if (cc < 0xfc)	c->remains = 4;
      else if (cc < 0xfe)	c->remains = 5;
      else goto nocode;
    }
  c->state = UTF8_READ;
  goto go_slow;
#endif

#ifdef CONV_WRITE_UTF8
 dend_utf:
  c->state = UTF8_WRITE_START;
  c->code = code;
  goto go_slow;
#endif

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

} while (0);

/*** Undefine all parameters ***/

#undef CONV_READ_STD
#undef CONV_READ_UTF8
#undef CONV_READ_UTF16_BE
#undef CONV_READ_UTF16_LE
#undef CONV_WRITE_STD
#undef CONV_WRITE_UTF8
#undef CONV_WRITE_UTF16_BE
#undef CONV_WRITE_UTF16_LE
