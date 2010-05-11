/*
 *	UCW Library -- Conversions of Strings to Numbers
 *
 *	(c) 2010 Daniel Fiala <danfiala@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "ucw/lib.h"
#include "ucw/string.h"
#include "ucw/chartype.h"
#include "ucw/strtonum.h"

static const char err_numeric_overflow[] = "Numeric overflow";
static const char err_no_digits[] = "Number contains no digits";
static const char err_invalid_character[] = "Invalid character";
static const char err_unknown_base[] = "Unknown base";

static uns detect_base(const char *p, const uns flags)
{
  if ((flags & STN_BASES) && *p == '0')
    {
      switch (p[1] & 0xDF)
        {
          case 'X':
            if (flags & STN_HEX)
              {
                return 16;
              }
            break;

          case 'B':
            if (flags & STN_BIN)
              {
                return 2;
              }
            break;

          case 'O':
            if (flags & STN_OCT)
              {
                return 8;
              }
            break;
        }
    }

  return 0;
}

static const char *str_to_num_init(const char **pp, const uns flags, uns *sign, uns *base)
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

  const uns prefix_base = detect_base(p, flags);
  if (prefix_base)
    {
      p += 2;
      *base = prefix_base;
    }
  else
    {
      *base = flags & STN_DBASES_MASK;
    }

  *pp = p;
  return err;
}

static inline uns get_digit(const uns c)
{
  if (c <= '9')
    {
      return c - '0';
    }
  else
    {
      const int a = c & 0xDF;
      unsigned d = a - 'A';
      d &= 0xFF;
      d += 10;
      return d;
    }
}

#define STN_TYPE uns
#define STN_SUFFIX uns
#include "ucw/strtonum-gen.h"

#define STN_TYPE uintmax_t
#define STN_SUFFIX uintmax
#include "ucw/strtonum-gen.h"
