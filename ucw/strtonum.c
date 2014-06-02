/*
 *	UCW Library -- Conversions of Strings to Numbers
 *
 *	(c) 2010 Daniel Fiala <danfiala@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/string.h>
#include <ucw/chartype.h>
#include <ucw/strtonum.h>

static uint detect_base(const char **pp, const uint flags)
{
  if ((flags & STN_BASES0) && **pp == '0')
    {
      switch ( (*pp)[1] )
        {
          case 'x':
          case 'X':
            if (flags & STN_HEX)
              {
                *pp += 2;
                return 16;
              }
            break;

          case 'b':
          case 'B':
            if (flags & STN_BIN)
              {
                *pp += 2;
                return 2;
              }
            break;

          case 'o':
          case 'O':
            if (flags & STN_OCT)
              {
                *pp += 2;
                return 8;
              }
            break;

          case '0'...'7':
            if (flags & STN_OCT0)
              {
                (*pp)++;
                return 8;
              }
            break;
        }
    }

  return 0;
}

static const char *str_to_num_init(const char **pp, const uint flags, uint *sign, uint *base)
{
  ASSERT(*pp);

  const char *err = NULL;
  const char *p = *pp;

  // Parse sign
  *sign = 0;
  if (flags & (STN_SIGNS))
    {
      if (*p == '-' && (flags & STN_MINUS))
        {
          *sign = 1;
          p++;
        }
      else if (*p == '+' && (flags & STN_PLUS))
        p++;
    }

  *base = detect_base(&p, flags) ? : flags & STN_DBASES_MASK;

  *pp = p;
  return err;
}

static inline uint get_digit(const uint c)
{
  if (c <= '9')
    return c - '0';
  else
    {
      const int a = c & 0xDF;
      uint d = a - 'A';
      d &= 0xFF;
      d += 10;
      return d;
    }
}

#define STN_TYPE uint
#define STN_SUFFIX uint
#include <ucw/strtonum-gen.h>

#define STN_TYPE uintmax_t
#define STN_SUFFIX uintmax
#include <ucw/strtonum-gen.h>
