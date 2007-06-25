/*
 *	UCW Library -- Unicode Characters
 *
 *	(c) 1997--2004 Martin Mares <mj@ucw.cz>
 *	(c) 2004 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_UNICODE_H
#define _UCW_UNICODE_H

/* Macros for handling UTF-8 */

#define UNI_REPLACEMENT 0xfffc

/* Encode a character from the basic multilingual plane [0, 0xFFFF]
 * (subset of Unicode 4.0); up to 3 bytes needed (RFC2279) */
static inline byte *
utf8_put(byte *p, uns u)
{
  if (u < 0x80)
    *p++ = u;
  else if (u < 0x800)
    {
      *p++ = 0xc0 | (u >> 6);
      *p++ = 0x80 | (u & 0x3f);
    }
  else
    {
      ASSERT(u < 0x10000);
      *p++ = 0xe0 | (u >> 12);
      *p++ = 0x80 | ((u >> 6) & 0x3f);
      *p++ = 0x80 | (u & 0x3f);
    }
  return p;
}

/* Encode a value from the range [0, 0x7FFFFFFF];
 * (superset of Unicode 4.0) up to 6 bytes needed (RFC2279) */
static inline byte *
utf8_32_put(byte *p, uns u)
{
  if (u < 0x80)
    *p++ = u;
  else if (u < 0x800)
    {
      *p++ = 0xc0 | (u >> 6);
      goto put1;
    }
  else if (u < (1<<16))
    {
      *p++ = 0xe0 | (u >> 12);
      goto put2;
    }
  else if (u < (1<<21))
    {
      *p++ = 0xf0 | (u >> 18);
      goto put3;
    }
  else if (u < (1<<26))
    {
      *p++ = 0xf8 | (u >> 24);
      goto put4;
    }
  else if (u < (1U<<31))
    {
      *p++ = 0xfc | (u >> 30);
      *p++ = 0x80 | ((u >> 24) & 0x3f);
put4: *p++ = 0x80 | ((u >> 18) & 0x3f);
put3: *p++ = 0x80 | ((u >> 12) & 0x3f);
put2: *p++ = 0x80 | ((u >> 6) & 0x3f);
put1: *p++ = 0x80 | (u & 0x3f);
    }
  else
    ASSERT(0);
  return p;
}

#define UTF8_GET_NEXT if (unlikely((*p & 0xc0) != 0x80)) goto bad; u = (u << 6) | (*p++ & 0x3f)

/* Decode a character from the basic multilingual plane [0, 0xFFFF]
 * or return UNI_REPLACEMENT if the encoding has been corrupted */
static inline byte *
utf8_get(const byte *p, uns *uu)
{
  uns u = *p++;
  if (u < 0x80)
    ;
  else if (unlikely(u < 0xc0))
    {
      /* Incorrect byte sequence */
    bad:
      u = UNI_REPLACEMENT;
    }
  else if (u < 0xe0)
    {
      u &= 0x1f;
      UTF8_GET_NEXT;
    }
  else if (likely(u < 0xf0))
    {
      u &= 0x0f;
      UTF8_GET_NEXT;
      UTF8_GET_NEXT;
    }
  else
    goto bad;
  *uu = u;
  return (byte *)p;
}

/* Decode a value from the range [0, 0x7FFFFFFF] 
 * or return UNI_REPLACEMENT if the encoding has been corrupted */
static inline byte *
utf8_32_get(const byte *p, uns *uu)
{
  uns u = *p++;
  if (u < 0x80)
    ;
  else if (unlikely(u < 0xc0))
    {
      /* Incorrect byte sequence */
    bad:
      u = UNI_REPLACEMENT;
    }
  else if (u < 0xe0)
    {
      u &= 0x1f;
      goto get1;
    }
  else if (u < 0xf0)
    {
      u &= 0x0f;
      goto get2;
    }
  else if (u < 0xf8)
    {
      u &= 0x07;
      goto get3;
    }
  else if (u < 0xfc)
    {
      u &= 0x03;
      goto get4;
    }
  else if (u < 0xfe)
    {
      u &= 0x01;
      UTF8_GET_NEXT;
get4: UTF8_GET_NEXT;
get3: UTF8_GET_NEXT;
get2: UTF8_GET_NEXT;
get1: UTF8_GET_NEXT;
    }
  else
    goto bad;
  *uu = u;
  return (byte *)p;
}

#define PUT_UTF8(p,u) p = utf8_put(p, u)
#define GET_UTF8(p,u) p = (byte*)utf8_get(p, &(u))

#define PUT_UTF8_32(p,u) p = utf8_32_put(p, u)
#define GET_UTF8_32(p,u) p = (byte*)utf8_32_get(p, &(u))

#define UTF8_SKIP(p) do {				\
    uns c = *p++;					\
    if (c >= 0xc0)					\
      while (c & 0x40 && *p >= 0x80 && *p < 0xc0)	\
        p++, c <<= 1;					\
  } while (0)

#define UTF8_SKIP_BWD(p) while ((*--(p) & 0xc0) == 0x80)

static inline uns
utf8_space(uns u)
{
  if (u < 0x80)
    return 1;
  if (u < 0x800)
    return 2;
  if (u < (1<<16))
    return 3;
  if (u < (1<<21))
    return 4;
  if (u < (1<<26))
    return 5;
  return 6;
}

static inline uns
utf8_encoding_len(uns c)
{
  if (c < 0x80)
    return 1;
  ASSERT(c >= 0xc0 && c < 0xfe);
  if (c < 0xe0)
    return 2;
  if (c < 0xf0)
    return 3;
  if (c < 0xf8)
    return 4;
  if (c < 0xfc)
    return 5;
  return 6;
}

/* unicode-utf8.c */

uns utf8_strlen(const byte *str);
uns utf8_strnlen(const byte *str, uns n);
uns utf8_check(const byte *str);

#endif
