/*
 *	UCW Library -- Conversions of Strings to Numbers
 *
 *	(c) 2010 Daniel Fiala <danfiala@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

/*
 * This is not a normal header file, it is a generator of a function for
 * converting strings to integers of a certain type. This file should be used
 * by ucw/stronum.c only.
 */

#define STN_DECLARE(type, suffix) STN_DECLARE_CONVERTOR(type, suffix)

#define S_HELPER2(name, suffix) name##suffix
#define S_HELPER1(name, suffix) S_HELPER2(name, suffix)
#define S(name) S_HELPER1(name, STN_SUFFIX)

#define STN_MAX ((STN_TYPE)(-1))
static const STN_TYPE S(tops)[STN_DBASES_MASK+1] = { [2] = STN_MAX/2, [8] = STN_MAX/8, [10] = STN_MAX/10, [16] = STN_MAX/16 };

static const char *S(parse_string)(const char **pp, const uns flags, const uns sign, const uns base, STN_TYPE *num)
{
  const STN_TYPE max = STN_MAX;
  const STN_TYPE top = S(tops)[base];
  if (!top)
    return "Unknown base";

  const STN_TYPE sign_max = ((flags & STN_SIGNED) || sign) ? max/2 + sign : max;

  STN_TYPE val = 0;
  uns digits = 0;
  int overflow = 0;
  for (;; (*pp)++)
    {
      const uns c = (byte)**pp;

      if (c == '_')
        {
          if (flags & STN_UNDERSCORE)
            continue;
          else
            break;
        }

      const uns d = get_digit(c);
      if (d >= base)
        break;

      digits++;

      if (overflow)
        continue;

      STN_TYPE v = val;
      if ( (overflow = (v > top || (v *= base) > sign_max - d)) )
        continue;
      val = v + d;
    }

  if (!overflow)
    {
      if (!digits)
        return "Number contains no digits";

      if (sign)
        val = -val;
    }
  else
    {
      if (flags & STN_TRUNC)
        val = sign_max;
      else
        return "Numeric overflow";
    }

  if ((flags & STN_WHOLE) && **pp)
    return "Invalid character";

  if (num)
    *num = val;

  return NULL;
}

STN_DECLARE(STN_TYPE, STN_SUFFIX)
{
  const char *err = NULL;

  uns sign, base;
  err = str_to_num_init(&str, flags, &sign, &base);

  if (!err)
    err = S(parse_string)(&str, flags, sign, base, num);

  if (next)
    *next = str;

  return err;
}

#undef STN_MAX
#undef S
#undef S_HELPER1
#undef S_HELPER2
#undef STN_TYPE
#undef STN_SUFFIX
#undef STN_DECLARE
