/*
 *	UCW Library -- Basic Extended Types
 *
 *	(c) 2014 Martin Mares <mj@ucw.cz>
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/mempool.h>
#include <ucw/strtonum.h>
#include <ucw/xtypes.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>

#define XTYPE_NUM_FORMAT(_type, _fmt, _typename) static const char *xt_##_typename##_format(void *src, u32 fmt UNUSED, struct mempool *pool) \
{\
  return mp_printf(pool, _fmt, *(_type *)src);\
}

#define XTYPE_NUM_PARSE(_typename) static const char *xt_##_typename##_parse(const char *str, void *dest, struct mempool *pool UNUSED)\
{\
  return str_to_##_typename(dest, str, NULL, 10 | STN_WHOLE | STN_MINUS | STN_PLUS | STN_HEX | STN_BIN | STN_OCT);\
}

#define XTYPE_NUM_STRUCT(_type, _typename) const struct xtype xt_##_typename = {\
  .size = sizeof(_type),\
  .name = #_typename,\
  .parse = xt_##_typename##_parse,\
  .format = xt_##_typename##_format,\
};

#define XTYPE_NUM_DEF(_type, _fmt, _typename) XTYPE_NUM_FORMAT(_type, _fmt, _typename) \
  XTYPE_NUM_PARSE(_typename)\
  XTYPE_NUM_STRUCT(_type, _typename)

XTYPE_NUM_DEF(int, "%d", int)
XTYPE_NUM_DEF(s64, "%" PRId64, s64)
XTYPE_NUM_DEF(intmax_t, "%jd", intmax)
XTYPE_NUM_DEF(uint, "%u", uint)
XTYPE_NUM_DEF(u64, "%" PRIu64, u64)
XTYPE_NUM_DEF(uintmax_t, "%ju", uintmax)

/* double */

static const char *xt_double_format(void *src, u32 fmt, struct mempool *pool)
{
  double val = *((double *)src);

  if (fmt & XTYPE_FMT_DBL_PREC)
    {
      uint prec = fmt & ~XTYPE_FMT_DBL_PREC;
      return mp_printf(pool, "%.*lf", prec, val);
    }

  switch(fmt)
    {
    case XTYPE_FMT_RAW:
      return mp_printf(pool, "%.15lg", val);
    case XTYPE_FMT_PRETTY:
      return mp_printf(pool, "%.2lf", val);
    case XTYPE_FMT_DEFAULT:
    default:
      return mp_printf(pool, "%.6lg", val);
    }
}

static const char *xt_double_parse(const char *str, void *dest, struct mempool *pool UNUSED)
{
  char *endptr = NULL;
  errno = 0;
  double result = strtod(str, &endptr);
  if (*endptr != 0 || endptr == str ||
      errno == ERANGE)
    return "Could not parse floating point number.";

  *((double *) dest) = result;
  return NULL;
}

static const char * xt_double_fmt_parse(const char *str, u32 *dest, struct mempool *pool)
{
  uint precision = 0;
  const char *tmp_err = str_to_uint(&precision, str, NULL, 0);
  if (tmp_err)
    return mp_printf(pool, "Could not parse floating point number precision: %s", tmp_err);

  *dest = XTYPE_FMT_DBL_FIXED_PREC(precision);
  return NULL;
}

const struct xtype xt_double = {
  .size = sizeof(double),
  .name = "double",
  .parse = xt_double_parse,
  .format = xt_double_format,
  .parse_fmt = xt_double_fmt_parse
};

/* bool */

static const char *xt_bool_format(void *src, u32 fmt UNUSED, struct mempool *pool UNUSED)
{
  bool val = *((bool *)src);
  switch (fmt)
    {
    case XTYPE_FMT_PRETTY:
      return val ? "true" : "false";
    case XTYPE_FMT_DEFAULT:
    case XTYPE_FMT_RAW:
      return val ? "1" : "0";
    default:
      ASSERT(0);
    }
}

static const char *xt_bool_parse(const char *str, void *dest, struct mempool *pool UNUSED)
{
  if (!strcmp(str, "0") || !strcmp(str, "false") || !strcmp(str, "no"))
    {
      *((bool *)dest) = false;
      return NULL;
    }
  else if (!strcmp(str, "1") || !strcmp(str, "true") || !strcmp(str, "yes"))
    {
      *((bool *)dest) = true;
      return NULL;
    }

  return "Could not parse a boolean value.";
}

XTYPE_NUM_STRUCT(bool, bool)

/* str */

static const char *xt_str_format(void *src, u32 fmt UNUSED, struct mempool *pool)
{
  return mp_strdup(pool, *((const char **) src));
}

static const char *xt_str_parse(const char *str, void *dest, struct mempool *pool)
{
  *((const char **) dest) = mp_strdup(pool, str);
  return NULL;
}

XTYPE_NUM_STRUCT(char *, str)
